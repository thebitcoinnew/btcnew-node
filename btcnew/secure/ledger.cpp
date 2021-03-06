#include <btcnew/lib/rep_weights.hpp>
#include <btcnew/lib/stats.hpp>
#include <btcnew/lib/utility.hpp>
#include <btcnew/lib/work.hpp>
#include <btcnew/secure/blockstore.hpp>
#include <btcnew/secure/ledger.hpp>

namespace
{
/**
 * Roll back the visited block
 */
class rollback_visitor : public btcnew::block_visitor
{
public:
	rollback_visitor (btcnew::write_transaction const & transaction_a, btcnew::ledger & ledger_a, std::vector<std::shared_ptr<btcnew::block>> & list_a) :
	transaction (transaction_a),
	ledger (ledger_a),
	list (list_a)
	{
	}
	virtual ~rollback_visitor () = default;
	void send_block (btcnew::send_block const & block_a) override
	{
		auto hash (block_a.hash ());
		btcnew::pending_info pending;
		btcnew::pending_key key (block_a.hashables.destination, hash);
		while (!error && ledger.store.pending_get (transaction, key, pending))
		{
			error = ledger.rollback (transaction, ledger.latest (transaction, block_a.hashables.destination), list);
		}
		if (!error)
		{
			btcnew::account_info info;
			auto error (ledger.store.account_get (transaction, pending.source, info));
			(void)error;
			assert (!error);
			ledger.store.pending_del (transaction, key);
			ledger.rep_weights.representation_add (info.representative, pending.amount.number ());
			btcnew::account_info new_info (block_a.hashables.previous, info.representative, info.open_block, ledger.balance (transaction, block_a.hashables.previous), btcnew::seconds_since_epoch (), info.block_count - 1, btcnew::epoch::epoch_0);
			ledger.change_latest (transaction, pending.source, info, new_info);
			ledger.store.block_del (transaction, hash);
			ledger.store.frontier_del (transaction, hash);
			ledger.store.frontier_put (transaction, block_a.hashables.previous, pending.source);
			ledger.store.block_successor_clear (transaction, block_a.hashables.previous);
			ledger.stats.inc (btcnew::stat::type::rollback, btcnew::stat::detail::send);
		}
	}
	void receive_block (btcnew::receive_block const & block_a) override
	{
		auto hash (block_a.hash ());
		auto amount (ledger.amount (transaction, block_a.hashables.source));
		auto destination_account (ledger.account (transaction, hash));
		auto source_account (ledger.account (transaction, block_a.hashables.source));
		btcnew::account_info info;
		auto error (ledger.store.account_get (transaction, destination_account, info));
		(void)error;
		assert (!error);
		ledger.rep_weights.representation_add (info.representative, 0 - amount);
		btcnew::account_info new_info (block_a.hashables.previous, info.representative, info.open_block, ledger.balance (transaction, block_a.hashables.previous), btcnew::seconds_since_epoch (), info.block_count - 1, btcnew::epoch::epoch_0);
		ledger.change_latest (transaction, destination_account, info, new_info);
		ledger.store.block_del (transaction, hash);
		ledger.store.pending_put (transaction, btcnew::pending_key (destination_account, block_a.hashables.source), { source_account, amount, btcnew::epoch::epoch_0 });
		ledger.store.frontier_del (transaction, hash);
		ledger.store.frontier_put (transaction, block_a.hashables.previous, destination_account);
		ledger.store.block_successor_clear (transaction, block_a.hashables.previous);
		ledger.stats.inc (btcnew::stat::type::rollback, btcnew::stat::detail::receive);
	}
	void open_block (btcnew::open_block const & block_a) override
	{
		auto hash (block_a.hash ());
		auto amount (ledger.amount (transaction, block_a.hashables.source));
		auto destination_account (ledger.account (transaction, hash));
		auto source_account (ledger.account (transaction, block_a.hashables.source));
		ledger.rep_weights.representation_add (block_a.representative (), 0 - amount);
		btcnew::account_info new_info;
		ledger.change_latest (transaction, destination_account, new_info, new_info);
		ledger.store.block_del (transaction, hash);
		ledger.store.pending_put (transaction, btcnew::pending_key (destination_account, block_a.hashables.source), { source_account, amount, btcnew::epoch::epoch_0 });
		ledger.store.frontier_del (transaction, hash);
		ledger.stats.inc (btcnew::stat::type::rollback, btcnew::stat::detail::open);
	}
	void change_block (btcnew::change_block const & block_a) override
	{
		auto hash (block_a.hash ());
		auto rep_block (ledger.representative (transaction, block_a.hashables.previous));
		auto account (ledger.account (transaction, block_a.hashables.previous));
		btcnew::account_info info;
		auto error (ledger.store.account_get (transaction, account, info));
		(void)error;
		assert (!error);
		auto balance (ledger.balance (transaction, block_a.hashables.previous));
		auto block = ledger.store.block_get (transaction, rep_block);
		release_assert (block != nullptr);
		auto representative = block->representative ();
		ledger.rep_weights.representation_add (block_a.representative (), 0 - balance);
		ledger.rep_weights.representation_add (representative, balance);
		ledger.store.block_del (transaction, hash);
		btcnew::account_info new_info (block_a.hashables.previous, representative, info.open_block, info.balance, btcnew::seconds_since_epoch (), info.block_count - 1, btcnew::epoch::epoch_0);
		ledger.change_latest (transaction, account, info, new_info);
		ledger.store.frontier_del (transaction, hash);
		ledger.store.frontier_put (transaction, block_a.hashables.previous, account);
		ledger.store.block_successor_clear (transaction, block_a.hashables.previous);
		ledger.stats.inc (btcnew::stat::type::rollback, btcnew::stat::detail::change);
	}
	void state_block (btcnew::state_block const & block_a) override
	{
		auto hash (block_a.hash ());
		btcnew::block_hash rep_block_hash (0);
		if (!block_a.hashables.previous.is_zero ())
		{
			rep_block_hash = ledger.representative (transaction, block_a.hashables.previous);
		}
		auto balance (ledger.balance (transaction, block_a.hashables.previous));
		auto is_send (block_a.hashables.balance < balance);
		// Add in amount delta
		ledger.rep_weights.representation_add (block_a.representative (), 0 - block_a.hashables.balance.number ());
		btcnew::account representative{ 0 };
		if (!rep_block_hash.is_zero ())
		{
			// Move existing representation
			auto block (ledger.store.block_get (transaction, rep_block_hash));
			assert (block != nullptr);
			representative = block->representative ();
			ledger.rep_weights.representation_add (representative, balance);
		}

		btcnew::account_info info;
		auto error (ledger.store.account_get (transaction, block_a.hashables.account, info));

		if (is_send)
		{
			btcnew::pending_key key (block_a.hashables.link, hash);
			while (!error && !ledger.store.pending_exists (transaction, key))
			{
				error = ledger.rollback (transaction, ledger.latest (transaction, block_a.hashables.link), list);
			}
			ledger.store.pending_del (transaction, key);
			ledger.stats.inc (btcnew::stat::type::rollback, btcnew::stat::detail::send);
		}
		else if (!block_a.hashables.link.is_zero () && !ledger.is_epoch_link (block_a.hashables.link))
		{
			auto source_version (ledger.store.block_version (transaction, block_a.hashables.link));
			btcnew::pending_info pending_info (ledger.account (transaction, block_a.hashables.link), block_a.hashables.balance.number () - balance, source_version);
			ledger.store.pending_put (transaction, btcnew::pending_key (block_a.hashables.account, block_a.hashables.link), pending_info);
			ledger.stats.inc (btcnew::stat::type::rollback, btcnew::stat::detail::receive);
		}

		assert (!error);
		auto previous_version (ledger.store.block_version (transaction, block_a.hashables.previous));
		btcnew::account_info new_info (block_a.hashables.previous, representative, info.open_block, balance, btcnew::seconds_since_epoch (), info.block_count - 1, previous_version);
		ledger.change_latest (transaction, block_a.hashables.account, info, new_info);

		auto previous (ledger.store.block_get (transaction, block_a.hashables.previous));
		if (previous != nullptr)
		{
			ledger.store.block_successor_clear (transaction, block_a.hashables.previous);
			if (previous->type () < btcnew::block_type::state)
			{
				ledger.store.frontier_put (transaction, block_a.hashables.previous, block_a.hashables.account);
			}
		}
		else
		{
			ledger.stats.inc (btcnew::stat::type::rollback, btcnew::stat::detail::open);
		}
		ledger.store.block_del (transaction, hash);
	}
	btcnew::write_transaction const & transaction;
	btcnew::ledger & ledger;
	std::vector<std::shared_ptr<btcnew::block>> & list;
	bool error{ false };
};

class ledger_processor : public btcnew::block_visitor
{
public:
	ledger_processor (btcnew::ledger &, btcnew::write_transaction const &, btcnew::signature_verification = btcnew::signature_verification::unknown);
	virtual ~ledger_processor () = default;
	void send_block (btcnew::send_block const &) override;
	void receive_block (btcnew::receive_block const &) override;
	void open_block (btcnew::open_block const &) override;
	void change_block (btcnew::change_block const &) override;
	void state_block (btcnew::state_block const &) override;
	void state_block_impl (btcnew::state_block const &);
	void epoch_block_impl (btcnew::state_block const &);
	btcnew::ledger & ledger;
	btcnew::write_transaction const & transaction;
	btcnew::signature_verification verification;
	btcnew::process_return result;

private:
	bool validate_epoch_block (btcnew::state_block const & block_a);
};

// Returns true if this block which has an epoch link is correctly formed.
bool ledger_processor::validate_epoch_block (btcnew::state_block const & block_a)
{
	assert (ledger.is_epoch_link (block_a.hashables.link));
	btcnew::amount prev_balance (0);
	if (!block_a.hashables.previous.is_zero ())
	{
		result.code = ledger.store.block_exists (transaction, block_a.hashables.previous) ? btcnew::process_result::progress : btcnew::process_result::gap_previous;
		if (result.code == btcnew::process_result::progress)
		{
			prev_balance = ledger.balance (transaction, block_a.hashables.previous);
		}
		else if (result.verified == btcnew::signature_verification::unknown)
		{
			// Check for possible regular state blocks with epoch link (send subtype)
			if (validate_message (block_a.hashables.account, block_a.hash (), block_a.signature))
			{
				// Is epoch block signed correctly
				if (validate_message (ledger.epoch_signer (block_a.link ()), block_a.hash (), block_a.signature))
				{
					result.verified = btcnew::signature_verification::invalid;
					result.code = btcnew::process_result::bad_signature;
				}
				else
				{
					result.verified = btcnew::signature_verification::valid_epoch;
				}
			}
			else
			{
				result.verified = btcnew::signature_verification::valid;
			}
		}
	}
	return (block_a.hashables.balance == prev_balance);
}

void ledger_processor::state_block (btcnew::state_block const & block_a)
{
	result.code = btcnew::process_result::progress;
	auto is_epoch_block = false;
	if (ledger.is_epoch_link (block_a.hashables.link))
	{
		// This function also modifies the result variable if epoch is mal-formed
		is_epoch_block = validate_epoch_block (block_a);
	}

	if (result.code == btcnew::process_result::progress)
	{
		if (is_epoch_block)
		{
			epoch_block_impl (block_a);
		}
		else
		{
			state_block_impl (block_a);
		}
	}
}

void ledger_processor::state_block_impl (btcnew::state_block const & block_a)
{
	auto hash (block_a.hash ());
	auto existing (ledger.store.block_exists (transaction, block_a.type (), hash));
	result.code = existing ? btcnew::process_result::old : btcnew::process_result::progress; // Have we seen this block before? (Unambiguous)
	if (result.code == btcnew::process_result::progress)
	{
		// Validate block if not verified outside of ledger
		if (result.verified != btcnew::signature_verification::valid)
		{
			result.code = validate_message (block_a.hashables.account, hash, block_a.signature) ? btcnew::process_result::bad_signature : btcnew::process_result::progress; // Is this block signed correctly (Unambiguous)
		}
		if (result.code == btcnew::process_result::progress)
		{
			assert (!validate_message (block_a.hashables.account, hash, block_a.signature));
			result.verified = btcnew::signature_verification::valid;
			result.code = block_a.hashables.account.is_zero () ? btcnew::process_result::opened_burn_account : btcnew::process_result::progress; // Is this for the burn account? (Unambiguous)
			if (result.code == btcnew::process_result::progress)
			{
				btcnew::epoch epoch (btcnew::epoch::epoch_0);
				btcnew::account_info info;
				result.amount = block_a.hashables.balance;
				auto is_send (false);
				auto account_error (ledger.store.account_get (transaction, block_a.hashables.account, info));
				if (!account_error)
				{
					epoch = info.epoch ();
					// Account already exists
					result.code = block_a.hashables.previous.is_zero () ? btcnew::process_result::fork : btcnew::process_result::progress; // Has this account already been opened? (Ambigious)
					if (result.code == btcnew::process_result::progress)
					{
						result.code = ledger.store.block_exists (transaction, block_a.hashables.previous) ? btcnew::process_result::progress : btcnew::process_result::gap_previous; // Does the previous block exist in the ledger? (Unambigious)
						if (result.code == btcnew::process_result::progress)
						{
							is_send = block_a.hashables.balance < info.balance;
							result.amount = is_send ? (info.balance.number () - result.amount.number ()) : (result.amount.number () - info.balance.number ());
							result.code = block_a.hashables.previous == info.head ? btcnew::process_result::progress : btcnew::process_result::fork; // Is the previous block the account's head block? (Ambigious)
						}
					}
				}
				else
				{
					// Account does not yet exists
					result.code = block_a.previous ().is_zero () ? btcnew::process_result::progress : btcnew::process_result::gap_previous; // Does the first block in an account yield 0 for previous() ? (Unambigious)
					if (result.code == btcnew::process_result::progress)
					{
						result.code = !block_a.hashables.link.is_zero () ? btcnew::process_result::progress : btcnew::process_result::gap_source; // Is the first block receiving from a send ? (Unambigious)
					}
				}
				if (result.code == btcnew::process_result::progress)
				{
					if (!is_send)
					{
						if (!block_a.hashables.link.is_zero ())
						{
							result.code = ledger.store.source_exists (transaction, block_a.hashables.link) ? btcnew::process_result::progress : btcnew::process_result::gap_source; // Have we seen the source block already? (Harmless)
							if (result.code == btcnew::process_result::progress)
							{
								btcnew::pending_key key (block_a.hashables.account, block_a.hashables.link);
								btcnew::pending_info pending;
								result.code = ledger.store.pending_get (transaction, key, pending) ? btcnew::process_result::unreceivable : btcnew::process_result::progress; // Has this source already been received (Malformed)
								if (result.code == btcnew::process_result::progress)
								{
									result.code = result.amount == pending.amount ? btcnew::process_result::progress : btcnew::process_result::balance_mismatch;
									epoch = std::max (epoch, pending.epoch);
								}
							}
						}
						else
						{
							// If there's no link, the balance must remain the same, only the representative can change
							result.code = result.amount.is_zero () ? btcnew::process_result::progress : btcnew::process_result::balance_mismatch;
						}
					}
				}
				if (result.code == btcnew::process_result::progress)
				{
					ledger.stats.inc (btcnew::stat::type::ledger, btcnew::stat::detail::state_block);
					result.state_is_send = is_send;
					btcnew::block_sideband sideband (btcnew::block_type::state, block_a.hashables.account /* unused */, 0, 0 /* unused */, info.block_count + 1, btcnew::seconds_since_epoch (), epoch);
					ledger.store.block_put (transaction, hash, block_a, sideband);

					if (!info.head.is_zero ())
					{
						// Move existing representation
						ledger.rep_weights.representation_add (info.representative, 0 - info.balance.number ());
					}
					// Add in amount delta
					ledger.rep_weights.representation_add (block_a.representative (), block_a.hashables.balance.number ());

					if (is_send)
					{
						btcnew::pending_key key (block_a.hashables.link, hash);
						btcnew::pending_info info (block_a.hashables.account, result.amount.number (), epoch);
						ledger.store.pending_put (transaction, key, info);
					}
					else if (!block_a.hashables.link.is_zero ())
					{
						ledger.store.pending_del (transaction, btcnew::pending_key (block_a.hashables.account, block_a.hashables.link));
					}

					btcnew::account_info new_info (hash, block_a.representative (), info.open_block.is_zero () ? hash : info.open_block, block_a.hashables.balance, btcnew::seconds_since_epoch (), info.block_count + 1, epoch);
					ledger.change_latest (transaction, block_a.hashables.account, info, new_info);
					if (!ledger.store.frontier_get (transaction, info.head).is_zero ())
					{
						ledger.store.frontier_del (transaction, info.head);
					}
					// Frontier table is unnecessary for state blocks and this also prevents old blocks from being inserted on top of state blocks
					result.account = block_a.hashables.account;
				}
			}
		}
	}
}

void ledger_processor::epoch_block_impl (btcnew::state_block const & block_a)
{
	auto hash (block_a.hash ());
	auto existing (ledger.store.block_exists (transaction, block_a.type (), hash));
	result.code = existing ? btcnew::process_result::old : btcnew::process_result::progress; // Have we seen this block before? (Unambiguous)
	if (result.code == btcnew::process_result::progress)
	{
		// Validate block if not verified outside of ledger
		if (result.verified != btcnew::signature_verification::valid_epoch)
		{
			result.code = validate_message (ledger.epoch_signer (block_a.hashables.link), hash, block_a.signature) ? btcnew::process_result::bad_signature : btcnew::process_result::progress; // Is this block signed correctly (Unambiguous)
		}
		if (result.code == btcnew::process_result::progress)
		{
			assert (!validate_message (ledger.epoch_signer (block_a.hashables.link), hash, block_a.signature));
			result.verified = btcnew::signature_verification::valid_epoch;
			result.code = block_a.hashables.account.is_zero () ? btcnew::process_result::opened_burn_account : btcnew::process_result::progress; // Is this for the burn account? (Unambiguous)
			if (result.code == btcnew::process_result::progress)
			{
				btcnew::account_info info;
				auto account_error (ledger.store.account_get (transaction, block_a.hashables.account, info));
				if (!account_error)
				{
					// Account already exists
					result.code = block_a.hashables.previous.is_zero () ? btcnew::process_result::fork : btcnew::process_result::progress; // Has this account already been opened? (Ambigious)
					if (result.code == btcnew::process_result::progress)
					{
						result.code = block_a.hashables.previous == info.head ? btcnew::process_result::progress : btcnew::process_result::fork; // Is the previous block the account's head block? (Ambigious)
						if (result.code == btcnew::process_result::progress)
						{
							result.code = block_a.hashables.representative == info.representative ? btcnew::process_result::progress : btcnew::process_result::representative_mismatch;
						}
					}
				}
				else
				{
					result.code = block_a.hashables.representative.is_zero () ? btcnew::process_result::progress : btcnew::process_result::representative_mismatch;
				}
				if (result.code == btcnew::process_result::progress)
				{
					auto epoch = ledger.network_params.ledger.epochs.epoch (block_a.hashables.link);
					// Must be an epoch for an unopened account or the epoch upgrade must be sequential
					auto is_valid_epoch_upgrade = account_error ? static_cast<std::underlying_type_t<btcnew::epoch>> (epoch) > 0 : btcnew::epochs::is_sequential (info.epoch (), epoch);
					result.code = is_valid_epoch_upgrade ? btcnew::process_result::progress : btcnew::process_result::block_position;
					if (result.code == btcnew::process_result::progress)
					{
						result.code = block_a.hashables.balance == info.balance ? btcnew::process_result::progress : btcnew::process_result::balance_mismatch;
						if (result.code == btcnew::process_result::progress)
						{
							ledger.stats.inc (btcnew::stat::type::ledger, btcnew::stat::detail::epoch_block);
							result.account = block_a.hashables.account;
							result.amount = 0;
							btcnew::block_sideband sideband (btcnew::block_type::state, block_a.hashables.account /* unused */, 0, 0 /* unused */, info.block_count + 1, btcnew::seconds_since_epoch (), epoch);
							ledger.store.block_put (transaction, hash, block_a, sideband);
							btcnew::account_info new_info (hash, block_a.representative (), info.open_block.is_zero () ? hash : info.open_block, info.balance, btcnew::seconds_since_epoch (), info.block_count + 1, epoch);
							ledger.change_latest (transaction, block_a.hashables.account, info, new_info);
							if (!ledger.store.frontier_get (transaction, info.head).is_zero ())
							{
								ledger.store.frontier_del (transaction, info.head);
							}
						}
					}
				}
			}
		}
	}
}

void ledger_processor::change_block (btcnew::change_block const & block_a)
{
	auto hash (block_a.hash ());
	auto existing (ledger.store.block_exists (transaction, block_a.type (), hash));
	result.code = existing ? btcnew::process_result::old : btcnew::process_result::progress; // Have we seen this block before? (Harmless)
	if (result.code == btcnew::process_result::progress)
	{
		auto previous (ledger.store.block_get (transaction, block_a.hashables.previous));
		result.code = previous != nullptr ? btcnew::process_result::progress : btcnew::process_result::gap_previous; // Have we seen the previous block already? (Harmless)
		if (result.code == btcnew::process_result::progress)
		{
			result.code = block_a.valid_predecessor (*previous) ? btcnew::process_result::progress : btcnew::process_result::block_position;
			if (result.code == btcnew::process_result::progress)
			{
				auto account (ledger.store.frontier_get (transaction, block_a.hashables.previous));
				result.code = account.is_zero () ? btcnew::process_result::fork : btcnew::process_result::progress;
				if (result.code == btcnew::process_result::progress)
				{
					btcnew::account_info info;
					auto latest_error (ledger.store.account_get (transaction, account, info));
					(void)latest_error;
					assert (!latest_error);
					assert (info.head == block_a.hashables.previous);
					// Validate block if not verified outside of ledger
					if (result.verified != btcnew::signature_verification::valid)
					{
						result.code = validate_message (account, hash, block_a.signature) ? btcnew::process_result::bad_signature : btcnew::process_result::progress; // Is this block signed correctly (Malformed)
					}
					if (result.code == btcnew::process_result::progress)
					{
						assert (!validate_message (account, hash, block_a.signature));
						result.verified = btcnew::signature_verification::valid;
						btcnew::block_sideband sideband (btcnew::block_type::change, account, 0, info.balance, info.block_count + 1, btcnew::seconds_since_epoch (), btcnew::epoch::epoch_0);
						ledger.store.block_put (transaction, hash, block_a, sideband);
						auto balance (ledger.balance (transaction, block_a.hashables.previous));
						ledger.rep_weights.representation_add (block_a.representative (), balance);
						ledger.rep_weights.representation_add (info.representative, 0 - balance);
						btcnew::account_info new_info (hash, block_a.representative (), info.open_block, info.balance, btcnew::seconds_since_epoch (), info.block_count + 1, btcnew::epoch::epoch_0);
						ledger.change_latest (transaction, account, info, new_info);
						ledger.store.frontier_del (transaction, block_a.hashables.previous);
						ledger.store.frontier_put (transaction, hash, account);
						result.account = account;
						result.amount = 0;
						ledger.stats.inc (btcnew::stat::type::ledger, btcnew::stat::detail::change);
					}
				}
			}
		}
	}
}

void ledger_processor::send_block (btcnew::send_block const & block_a)
{
	auto hash (block_a.hash ());
	auto existing (ledger.store.block_exists (transaction, block_a.type (), hash));
	result.code = existing ? btcnew::process_result::old : btcnew::process_result::progress; // Have we seen this block before? (Harmless)
	if (result.code == btcnew::process_result::progress)
	{
		auto previous (ledger.store.block_get (transaction, block_a.hashables.previous));
		result.code = previous != nullptr ? btcnew::process_result::progress : btcnew::process_result::gap_previous; // Have we seen the previous block already? (Harmless)
		if (result.code == btcnew::process_result::progress)
		{
			result.code = block_a.valid_predecessor (*previous) ? btcnew::process_result::progress : btcnew::process_result::block_position;
			if (result.code == btcnew::process_result::progress)
			{
				auto account (ledger.store.frontier_get (transaction, block_a.hashables.previous));
				result.code = account.is_zero () ? btcnew::process_result::fork : btcnew::process_result::progress;
				if (result.code == btcnew::process_result::progress)
				{
					// Validate block if not verified outside of ledger
					if (result.verified != btcnew::signature_verification::valid)
					{
						result.code = validate_message (account, hash, block_a.signature) ? btcnew::process_result::bad_signature : btcnew::process_result::progress; // Is this block signed correctly (Malformed)
					}
					if (result.code == btcnew::process_result::progress)
					{
						assert (!validate_message (account, hash, block_a.signature));
						result.verified = btcnew::signature_verification::valid;
						btcnew::account_info info;
						auto latest_error (ledger.store.account_get (transaction, account, info));
						(void)latest_error;
						assert (!latest_error);
						assert (info.head == block_a.hashables.previous);
						result.code = info.balance.number () >= block_a.hashables.balance.number () ? btcnew::process_result::progress : btcnew::process_result::negative_spend; // Is this trying to spend a negative amount (Malicious)
						if (result.code == btcnew::process_result::progress)
						{
							auto amount (info.balance.number () - block_a.hashables.balance.number ());
							ledger.rep_weights.representation_add (info.representative, 0 - amount);
							btcnew::block_sideband sideband (btcnew::block_type::send, account, 0, block_a.hashables.balance /* unused */, info.block_count + 1, btcnew::seconds_since_epoch (), btcnew::epoch::epoch_0);
							ledger.store.block_put (transaction, hash, block_a, sideband);
							btcnew::account_info new_info (hash, info.representative, info.open_block, block_a.hashables.balance, btcnew::seconds_since_epoch (), info.block_count + 1, btcnew::epoch::epoch_0);
							ledger.change_latest (transaction, account, info, new_info);
							ledger.store.pending_put (transaction, btcnew::pending_key (block_a.hashables.destination, hash), { account, amount, btcnew::epoch::epoch_0 });
							ledger.store.frontier_del (transaction, block_a.hashables.previous);
							ledger.store.frontier_put (transaction, hash, account);
							result.account = account;
							result.amount = amount;
							result.pending_account = block_a.hashables.destination;
							ledger.stats.inc (btcnew::stat::type::ledger, btcnew::stat::detail::send);
						}
					}
				}
			}
		}
	}
}

void ledger_processor::receive_block (btcnew::receive_block const & block_a)
{
	auto hash (block_a.hash ());
	auto existing (ledger.store.block_exists (transaction, block_a.type (), hash));
	result.code = existing ? btcnew::process_result::old : btcnew::process_result::progress; // Have we seen this block already?  (Harmless)
	if (result.code == btcnew::process_result::progress)
	{
		auto previous (ledger.store.block_get (transaction, block_a.hashables.previous));
		result.code = previous != nullptr ? btcnew::process_result::progress : btcnew::process_result::gap_previous;
		if (result.code == btcnew::process_result::progress)
		{
			result.code = block_a.valid_predecessor (*previous) ? btcnew::process_result::progress : btcnew::process_result::block_position;
			if (result.code == btcnew::process_result::progress)
			{
				auto account (ledger.store.frontier_get (transaction, block_a.hashables.previous));
				result.code = account.is_zero () ? btcnew::process_result::gap_previous : btcnew::process_result::progress; //Have we seen the previous block? No entries for account at all (Harmless)
				if (result.code == btcnew::process_result::progress)
				{
					// Validate block if not verified outside of ledger
					if (result.verified != btcnew::signature_verification::valid)
					{
						result.code = validate_message (account, hash, block_a.signature) ? btcnew::process_result::bad_signature : btcnew::process_result::progress; // Is the signature valid (Malformed)
					}
					if (result.code == btcnew::process_result::progress)
					{
						assert (!validate_message (account, hash, block_a.signature));
						result.verified = btcnew::signature_verification::valid;
						result.code = ledger.store.source_exists (transaction, block_a.hashables.source) ? btcnew::process_result::progress : btcnew::process_result::gap_source; // Have we seen the source block already? (Harmless)
						if (result.code == btcnew::process_result::progress)
						{
							btcnew::account_info info;
							ledger.store.account_get (transaction, account, info);
							result.code = info.head == block_a.hashables.previous ? btcnew::process_result::progress : btcnew::process_result::gap_previous; // Block doesn't immediately follow latest block (Harmless)
							if (result.code == btcnew::process_result::progress)
							{
								btcnew::pending_key key (account, block_a.hashables.source);
								btcnew::pending_info pending;
								result.code = ledger.store.pending_get (transaction, key, pending) ? btcnew::process_result::unreceivable : btcnew::process_result::progress; // Has this source already been received (Malformed)
								if (result.code == btcnew::process_result::progress)
								{
									result.code = pending.epoch == btcnew::epoch::epoch_0 ? btcnew::process_result::progress : btcnew::process_result::unreceivable; // Are we receiving a state-only send? (Malformed)
									if (result.code == btcnew::process_result::progress)
									{
										auto new_balance (info.balance.number () + pending.amount.number ());
										btcnew::account_info source_info;
										auto error (ledger.store.account_get (transaction, pending.source, source_info));
										(void)error;
										assert (!error);
										ledger.store.pending_del (transaction, key);
										btcnew::block_sideband sideband (btcnew::block_type::receive, account, 0, new_balance, info.block_count + 1, btcnew::seconds_since_epoch (), btcnew::epoch::epoch_0);
										ledger.store.block_put (transaction, hash, block_a, sideband);
										btcnew::account_info new_info (hash, info.representative, info.open_block, new_balance, btcnew::seconds_since_epoch (), info.block_count + 1, btcnew::epoch::epoch_0);
										ledger.change_latest (transaction, account, info, new_info);
										ledger.rep_weights.representation_add (info.representative, pending.amount.number ());
										ledger.store.frontier_del (transaction, block_a.hashables.previous);
										ledger.store.frontier_put (transaction, hash, account);
										result.account = account;
										result.amount = pending.amount;
										ledger.stats.inc (btcnew::stat::type::ledger, btcnew::stat::detail::receive);
									}
								}
							}
						}
					}
				}
				else
				{
					result.code = ledger.store.block_exists (transaction, block_a.hashables.previous) ? btcnew::process_result::fork : btcnew::process_result::gap_previous; // If we have the block but it's not the latest we have a signed fork (Malicious)
				}
			}
		}
	}
}

void ledger_processor::open_block (btcnew::open_block const & block_a)
{
	auto hash (block_a.hash ());
	auto existing (ledger.store.block_exists (transaction, block_a.type (), hash));
	result.code = existing ? btcnew::process_result::old : btcnew::process_result::progress; // Have we seen this block already? (Harmless)
	if (result.code == btcnew::process_result::progress)
	{
		// Validate block if not verified outside of ledger
		if (result.verified != btcnew::signature_verification::valid)
		{
			result.code = validate_message (block_a.hashables.account, hash, block_a.signature) ? btcnew::process_result::bad_signature : btcnew::process_result::progress; // Is the signature valid (Malformed)
		}
		if (result.code == btcnew::process_result::progress)
		{
			assert (!validate_message (block_a.hashables.account, hash, block_a.signature));
			result.verified = btcnew::signature_verification::valid;
			result.code = ledger.store.source_exists (transaction, block_a.hashables.source) ? btcnew::process_result::progress : btcnew::process_result::gap_source; // Have we seen the source block? (Harmless)
			if (result.code == btcnew::process_result::progress)
			{
				btcnew::account_info info;
				result.code = ledger.store.account_get (transaction, block_a.hashables.account, info) ? btcnew::process_result::progress : btcnew::process_result::fork; // Has this account already been opened? (Malicious)
				if (result.code == btcnew::process_result::progress)
				{
					btcnew::pending_key key (block_a.hashables.account, block_a.hashables.source);
					btcnew::pending_info pending;
					result.code = ledger.store.pending_get (transaction, key, pending) ? btcnew::process_result::unreceivable : btcnew::process_result::progress; // Has this source already been received (Malformed)
					if (result.code == btcnew::process_result::progress)
					{
						result.code = block_a.hashables.account == ledger.network_params.ledger.burn_account ? btcnew::process_result::opened_burn_account : btcnew::process_result::progress; // Is it burning 0 account? (Malicious)
						if (result.code == btcnew::process_result::progress)
						{
							result.code = pending.epoch == btcnew::epoch::epoch_0 ? btcnew::process_result::progress : btcnew::process_result::unreceivable; // Are we receiving a state-only send? (Malformed)
							if (result.code == btcnew::process_result::progress)
							{
								btcnew::account_info source_info;
								auto error (ledger.store.account_get (transaction, pending.source, source_info));
								(void)error;
								assert (!error);
								ledger.store.pending_del (transaction, key);
								btcnew::block_sideband sideband (btcnew::block_type::open, block_a.hashables.account, 0, pending.amount, 1, btcnew::seconds_since_epoch (), btcnew::epoch::epoch_0);
								ledger.store.block_put (transaction, hash, block_a, sideband);
								btcnew::account_info new_info (hash, block_a.representative (), hash, pending.amount.number (), btcnew::seconds_since_epoch (), 1, btcnew::epoch::epoch_0);
								ledger.change_latest (transaction, block_a.hashables.account, info, new_info);
								ledger.rep_weights.representation_add (block_a.representative (), pending.amount.number ());
								ledger.store.frontier_put (transaction, hash, block_a.hashables.account);
								result.account = block_a.hashables.account;
								result.amount = pending.amount;
								ledger.stats.inc (btcnew::stat::type::ledger, btcnew::stat::detail::open);
							}
						}
					}
				}
			}
		}
	}
}

ledger_processor::ledger_processor (btcnew::ledger & ledger_a, btcnew::write_transaction const & transaction_a, btcnew::signature_verification verification_a) :
ledger (ledger_a),
transaction (transaction_a),
verification (verification_a)
{
	result.verified = verification;
}
} // namespace

btcnew::ledger::ledger (btcnew::block_store & store_a, btcnew::stat & stat_a, bool cache_reps_a, bool cache_cemented_count_a) :
store (store_a),
stats (stat_a),
check_bootstrap_weights (true)
{
	if (!store.init_error ())
	{
		auto transaction = store.tx_begin_read ();
		if (cache_reps_a)
		{
			for (auto i (store.latest_begin (transaction)), n (store.latest_end ()); i != n; ++i)
			{
				btcnew::account_info const & info (i->second);
				rep_weights.representation_add (info.representative, info.balance.number ());
			}
		}

		if (cache_cemented_count_a)
		{
			for (auto i (store.confirmation_height_begin (transaction)), n (store.confirmation_height_end ()); i != n; ++i)
			{
				cemented_count += i->second;
			}
		}

		// Cache block count
		block_count_cache = store.block_count (transaction).sum ();
	}
}

// Balance for account containing hash
btcnew::uint128_t btcnew::ledger::balance (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a) const
{
	return hash_a.is_zero () ? 0 : store.block_balance (transaction_a, hash_a);
}

// Balance for an account by account number
btcnew::uint128_t btcnew::ledger::account_balance (btcnew::transaction const & transaction_a, btcnew::account const & account_a)
{
	btcnew::uint128_t result (0);
	btcnew::account_info info;
	auto none (store.account_get (transaction_a, account_a, info));
	if (!none)
	{
		result = info.balance.number ();
	}
	return result;
}

btcnew::uint128_t btcnew::ledger::account_pending (btcnew::transaction const & transaction_a, btcnew::account const & account_a)
{
	btcnew::uint128_t result (0);
	btcnew::account end (account_a.number () + 1);
	for (auto i (store.pending_begin (transaction_a, btcnew::pending_key (account_a, 0))), n (store.pending_begin (transaction_a, btcnew::pending_key (end, 0))); i != n; ++i)
	{
		btcnew::pending_info const & info (i->second);
		result += info.amount.number ();
	}
	return result;
}

btcnew::process_return btcnew::ledger::process (btcnew::write_transaction const & transaction_a, btcnew::block const & block_a, btcnew::signature_verification verification)
{
	assert (!btcnew::work_validate (block_a));
	ledger_processor processor (*this, transaction_a, verification);
	block_a.visit (processor);
	if (processor.result.code == btcnew::process_result::progress)
	{
		++block_count_cache;
	}
	return processor.result;
}

btcnew::block_hash btcnew::ledger::representative (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a)
{
	auto result (representative_calculated (transaction_a, hash_a));
	assert (result.is_zero () || store.block_exists (transaction_a, result));
	return result;
}

btcnew::block_hash btcnew::ledger::representative_calculated (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a)
{
	representative_visitor visitor (transaction_a, store);
	visitor.compute (hash_a);
	return visitor.result;
}

bool btcnew::ledger::block_exists (btcnew::block_hash const & hash_a)
{
	auto transaction (store.tx_begin_read ());
	auto result (store.block_exists (transaction, hash_a));
	return result;
}

bool btcnew::ledger::block_exists (btcnew::block_type type, btcnew::block_hash const & hash_a)
{
	auto transaction (store.tx_begin_read ());
	auto result (store.block_exists (transaction, type, hash_a));
	return result;
}

std::string btcnew::ledger::block_text (char const * hash_a)
{
	return block_text (btcnew::block_hash (hash_a));
}

std::string btcnew::ledger::block_text (btcnew::block_hash const & hash_a)
{
	std::string result;
	auto transaction (store.tx_begin_read ());
	auto block (store.block_get (transaction, hash_a));
	if (block != nullptr)
	{
		block->serialize_json (result);
	}
	return result;
}

bool btcnew::ledger::is_send (btcnew::transaction const & transaction_a, btcnew::state_block const & block_a) const
{
	bool result (false);
	btcnew::block_hash previous (block_a.hashables.previous);
	if (!previous.is_zero ())
	{
		if (block_a.hashables.balance < balance (transaction_a, previous))
		{
			result = true;
		}
	}
	return result;
}

btcnew::account const & btcnew::ledger::block_destination (btcnew::transaction const & transaction_a, btcnew::block const & block_a)
{
	btcnew::send_block const * send_block (dynamic_cast<btcnew::send_block const *> (&block_a));
	btcnew::state_block const * state_block (dynamic_cast<btcnew::state_block const *> (&block_a));
	if (send_block != nullptr)
	{
		return send_block->hashables.destination;
	}
	else if (state_block != nullptr && is_send (transaction_a, *state_block))
	{
		return state_block->hashables.link;
	}
	static btcnew::account result (0);
	return result;
}

btcnew::block_hash btcnew::ledger::block_source (btcnew::transaction const & transaction_a, btcnew::block const & block_a)
{
	/*
	 * block_source() requires that the previous block of the block
	 * passed in exist in the database.  This is because it will try
	 * to check account balances to determine if it is a send block.
	 */
	assert (block_a.previous ().is_zero () || store.block_exists (transaction_a, block_a.previous ()));

	// If block_a.source () is nonzero, then we have our source.
	// However, universal blocks will always return zero.
	btcnew::block_hash result (block_a.source ());
	btcnew::state_block const * state_block (dynamic_cast<btcnew::state_block const *> (&block_a));
	if (state_block != nullptr && !is_send (transaction_a, *state_block))
	{
		result = state_block->hashables.link;
	}
	return result;
}

// Vote weight of an account
btcnew::uint128_t btcnew::ledger::weight (btcnew::account const & account_a)
{
	if (check_bootstrap_weights.load ())
	{
		if (block_count_cache < bootstrap_weight_max_blocks)
		{
			auto weight = bootstrap_weights.find (account_a);
			if (weight != bootstrap_weights.end ())
			{
				return weight->second;
			}
		}
		else
		{
			check_bootstrap_weights = false;
		}
	}
	return rep_weights.representation_get (account_a);
}

// Rollback blocks until `block_a' doesn't exist or it tries to penetrate the confirmation height
bool btcnew::ledger::rollback (btcnew::write_transaction const & transaction_a, btcnew::block_hash const & block_a, std::vector<std::shared_ptr<btcnew::block>> & list_a)
{
	assert (store.block_exists (transaction_a, block_a));
	auto account_l (account (transaction_a, block_a));
	auto block_account_height (store.block_account_height (transaction_a, block_a));
	rollback_visitor rollback (transaction_a, *this, list_a);
	btcnew::account_info account_info;
	auto error (false);
	while (!error && store.block_exists (transaction_a, block_a))
	{
		uint64_t confirmation_height;
		auto latest_error = store.confirmation_height_get (transaction_a, account_l, confirmation_height);
		assert (!latest_error);
		(void)latest_error;
		if (block_account_height > confirmation_height)
		{
			latest_error = store.account_get (transaction_a, account_l, account_info);
			assert (!latest_error);
			auto block (store.block_get (transaction_a, account_info.head));
			list_a.push_back (block);
			block->visit (rollback);
			error = rollback.error;
			if (!error)
			{
				--block_count_cache;
			}
		}
		else
		{
			error = true;
		}
	}
	return error;
}

bool btcnew::ledger::rollback (btcnew::write_transaction const & transaction_a, btcnew::block_hash const & block_a)
{
	std::vector<std::shared_ptr<btcnew::block>> rollback_list;
	return rollback (transaction_a, block_a, rollback_list);
}

// Return account containing hash
btcnew::account btcnew::ledger::account (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a) const
{
	return store.block_account (transaction_a, hash_a);
}

// Return amount decrease or increase for block
btcnew::uint128_t btcnew::ledger::amount (btcnew::transaction const & transaction_a, btcnew::account const & account_a)
{
	release_assert (account_a == network_params.ledger.genesis_account);
	return network_params.ledger.genesis_amount;
}

btcnew::uint128_t btcnew::ledger::amount (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a)
{
	auto block (store.block_get (transaction_a, hash_a));
	auto block_balance (balance (transaction_a, hash_a));
	auto previous_balance (balance (transaction_a, block->previous ()));
	return block_balance > previous_balance ? block_balance - previous_balance : previous_balance - block_balance;
}

// Return latest block for account
btcnew::block_hash btcnew::ledger::latest (btcnew::transaction const & transaction_a, btcnew::account const & account_a)
{
	btcnew::account_info info;
	auto latest_error (store.account_get (transaction_a, account_a, info));
	return latest_error ? 0 : info.head;
}

// Return latest root for account, account number if there are no blocks for this account.
btcnew::root btcnew::ledger::latest_root (btcnew::transaction const & transaction_a, btcnew::account const & account_a)
{
	btcnew::account_info info;
	if (store.account_get (transaction_a, account_a, info))
	{
		return account_a;
	}
	else
	{
		return info.head;
	}
}

void btcnew::ledger::dump_account_chain (btcnew::account const & account_a)
{
	auto transaction (store.tx_begin_read ());
	auto hash (latest (transaction, account_a));
	while (!hash.is_zero ())
	{
		auto block (store.block_get (transaction, hash));
		assert (block != nullptr);
		std::cerr << hash.to_string () << std::endl;
		hash = block->previous ();
	}
}

class block_fit_visitor : public btcnew::block_visitor
{
public:
	block_fit_visitor (btcnew::ledger & ledger_a, btcnew::transaction const & transaction_a) :
	ledger (ledger_a),
	transaction (transaction_a),
	result (false)
	{
	}
	void send_block (btcnew::send_block const & block_a) override
	{
		result = ledger.store.block_exists (transaction, block_a.previous ());
	}
	void receive_block (btcnew::receive_block const & block_a) override
	{
		result = ledger.store.block_exists (transaction, block_a.previous ());
		result &= ledger.store.block_exists (transaction, block_a.source ());
	}
	void open_block (btcnew::open_block const & block_a) override
	{
		result = ledger.store.block_exists (transaction, block_a.source ());
	}
	void change_block (btcnew::change_block const & block_a) override
	{
		result = ledger.store.block_exists (transaction, block_a.previous ());
	}
	void state_block (btcnew::state_block const & block_a) override
	{
		result = block_a.previous ().is_zero () || ledger.store.block_exists (transaction, block_a.previous ());
		if (result && !ledger.is_send (transaction, block_a))
		{
			result &= ledger.store.block_exists (transaction, block_a.hashables.link) || block_a.hashables.link.is_zero () || ledger.is_epoch_link (block_a.hashables.link);
		}
	}
	btcnew::ledger & ledger;
	btcnew::transaction const & transaction;
	bool result;
};

bool btcnew::ledger::could_fit (btcnew::transaction const & transaction_a, btcnew::block const & block_a)
{
	block_fit_visitor visitor (*this, transaction_a);
	block_a.visit (visitor);
	return visitor.result;
}

bool btcnew::ledger::is_epoch_link (btcnew::link const & link_a)
{
	return network_params.ledger.epochs.is_epoch_link (link_a);
}

btcnew::account const & btcnew::ledger::epoch_signer (btcnew::link const & link_a) const
{
	return network_params.ledger.epochs.signer (network_params.ledger.epochs.epoch (link_a));
}

btcnew::link const & btcnew::ledger::epoch_link (btcnew::epoch epoch_a) const
{
	return network_params.ledger.epochs.link (epoch_a);
}

void btcnew::ledger::change_latest (btcnew::write_transaction const & transaction_a, btcnew::account const & account_a, btcnew::account_info const & old_a, btcnew::account_info const & new_a)
{
	if (!new_a.head.is_zero ())
	{
		if (old_a.head.is_zero () && new_a.open_block == new_a.head)
		{
			assert (!store.confirmation_height_exists (transaction_a, account_a));
			store.confirmation_height_put (transaction_a, account_a, 0);
		}
		if (!old_a.head.is_zero () && old_a.epoch () != new_a.epoch ())
		{
			// store.account_put won't erase existing entries if they're in different tables
			store.account_del (transaction_a, account_a);
		}
		store.account_put (transaction_a, account_a, new_a);
	}
	else
	{
		store.confirmation_height_del (transaction_a, account_a);
		store.account_del (transaction_a, account_a);
	}
}

std::shared_ptr<btcnew::block> btcnew::ledger::successor (btcnew::transaction const & transaction_a, btcnew::qualified_root const & root_a)
{
	btcnew::block_hash successor (0);
	auto get_from_previous = false;
	if (root_a.previous ().is_zero ())
	{
		btcnew::account_info info;
		if (!store.account_get (transaction_a, root_a.root (), info))
		{
			successor = info.open_block;
		}
		else
		{
			get_from_previous = true;
		}
	}
	else
	{
		get_from_previous = true;
	}

	if (get_from_previous)
	{
		successor = store.block_successor (transaction_a, root_a.previous ());
	}
	std::shared_ptr<btcnew::block> result;
	if (!successor.is_zero ())
	{
		result = store.block_get (transaction_a, successor);
	}
	assert (successor.is_zero () || result != nullptr);
	return result;
}

std::shared_ptr<btcnew::block> btcnew::ledger::forked_block (btcnew::transaction const & transaction_a, btcnew::block const & block_a)
{
	assert (!store.block_exists (transaction_a, block_a.type (), block_a.hash ()));
	auto root (block_a.root ());
	assert (store.block_exists (transaction_a, root) || store.account_exists (transaction_a, root));
	auto result (store.block_get (transaction_a, store.block_successor (transaction_a, root)));
	if (result == nullptr)
	{
		btcnew::account_info info;
		auto error (store.account_get (transaction_a, root, info));
		(void)error;
		assert (!error);
		result = store.block_get (transaction_a, info.open_block);
		assert (result != nullptr);
	}
	return result;
}

bool btcnew::ledger::block_confirmed (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a) const
{
	auto confirmed (false);
	auto block_height (store.block_account_height (transaction_a, hash_a));
	if (block_height > 0) // 0 indicates that the block doesn't exist
	{
		uint64_t confirmation_height;
		release_assert (!store.confirmation_height_get (transaction_a, account (transaction_a, hash_a), confirmation_height));
		confirmed = (confirmation_height >= block_height);
	}
	return confirmed;
}

bool btcnew::ledger::block_not_confirmed_or_not_exists (btcnew::block const & block_a) const
{
	bool result (true);
	auto hash (block_a.hash ());
	auto transaction (store.tx_begin_read ());
	if (store.block_exists (transaction, block_a.type (), hash))
	{
		result = !block_confirmed (transaction, hash);
	}
	return result;
}

namespace btcnew
{
std::unique_ptr<seq_con_info_component> collect_seq_con_info (ledger & ledger, const std::string & name)
{
	auto composite = std::make_unique<seq_con_info_composite> (name);
	auto count = ledger.bootstrap_weights_size.load ();
	auto sizeof_element = sizeof (decltype (ledger.bootstrap_weights)::value_type);
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "bootstrap_weights", count, sizeof_element }));
	composite->add_component (collect_seq_con_info (ledger.rep_weights, "rep_weights"));
	return composite;
}
}

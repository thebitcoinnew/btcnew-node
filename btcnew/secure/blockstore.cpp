#include <btcnew/secure/blockstore.hpp>

#include <boost/endian/conversion.hpp>
#include <boost/polymorphic_cast.hpp>

btcnew::block_sideband::block_sideband (btcnew::block_type type_a, btcnew::account const & account_a, btcnew::block_hash const & successor_a, btcnew::amount const & balance_a, uint64_t height_a, uint64_t timestamp_a, btcnew::epoch epoch_a) :
type (type_a),
successor (successor_a),
account (account_a),
balance (balance_a),
height (height_a),
timestamp (timestamp_a),
epoch (epoch_a)
{
}

size_t btcnew::block_sideband::size (btcnew::block_type type_a)
{
	size_t result (0);
	result += sizeof (successor);
	if (type_a != btcnew::block_type::state && type_a != btcnew::block_type::open)
	{
		result += sizeof (account);
	}
	if (type_a != btcnew::block_type::open)
	{
		result += sizeof (height);
	}
	if (type_a == btcnew::block_type::receive || type_a == btcnew::block_type::change || type_a == btcnew::block_type::open)
	{
		result += sizeof (balance);
	}
	result += sizeof (timestamp);
	if (type_a == btcnew::block_type::state)
	{
		result += sizeof (epoch);
	}
	return result;
}

void btcnew::block_sideband::serialize (btcnew::stream & stream_a) const
{
	btcnew::write (stream_a, successor.bytes);
	if (type != btcnew::block_type::state && type != btcnew::block_type::open)
	{
		btcnew::write (stream_a, account.bytes);
	}
	if (type != btcnew::block_type::open)
	{
		btcnew::write (stream_a, boost::endian::native_to_big (height));
	}
	if (type == btcnew::block_type::receive || type == btcnew::block_type::change || type == btcnew::block_type::open)
	{
		btcnew::write (stream_a, balance.bytes);
	}
	btcnew::write (stream_a, boost::endian::native_to_big (timestamp));
	if (type == btcnew::block_type::state)
	{
		btcnew::write (stream_a, epoch);
	}
}

bool btcnew::block_sideband::deserialize (btcnew::stream & stream_a)
{
	bool result (false);
	try
	{
		btcnew::read (stream_a, successor.bytes);
		if (type != btcnew::block_type::state && type != btcnew::block_type::open)
		{
			btcnew::read (stream_a, account.bytes);
		}
		if (type != btcnew::block_type::open)
		{
			btcnew::read (stream_a, height);
			boost::endian::big_to_native_inplace (height);
		}
		else
		{
			height = 1;
		}
		if (type == btcnew::block_type::receive || type == btcnew::block_type::change || type == btcnew::block_type::open)
		{
			btcnew::read (stream_a, balance.bytes);
		}
		btcnew::read (stream_a, timestamp);
		boost::endian::big_to_native_inplace (timestamp);
		if (type == btcnew::block_type::state)
		{
			btcnew::read (stream_a, epoch);
		}
	}
	catch (std::runtime_error &)
	{
		result = true;
	}

	return result;
}

btcnew::summation_visitor::summation_visitor (btcnew::transaction const & transaction_a, btcnew::block_store const & store_a, bool is_v14_upgrade_a) :
transaction (transaction_a),
store (store_a),
is_v14_upgrade (is_v14_upgrade_a)
{
}

void btcnew::summation_visitor::send_block (btcnew::send_block const & block_a)
{
	assert (current->type != summation_type::invalid && current != nullptr);
	if (current->type == summation_type::amount)
	{
		sum_set (block_a.hashables.balance.number ());
		current->balance_hash = block_a.hashables.previous;
		current->amount_hash = 0;
	}
	else
	{
		sum_add (block_a.hashables.balance.number ());
		current->balance_hash = 0;
	}
}

void btcnew::summation_visitor::state_block (btcnew::state_block const & block_a)
{
	assert (current->type != summation_type::invalid && current != nullptr);
	sum_set (block_a.hashables.balance.number ());
	if (current->type == summation_type::amount)
	{
		current->balance_hash = block_a.hashables.previous;
		current->amount_hash = 0;
	}
	else
	{
		current->balance_hash = 0;
	}
}

void btcnew::summation_visitor::receive_block (btcnew::receive_block const & block_a)
{
	assert (current->type != summation_type::invalid && current != nullptr);
	if (current->type == summation_type::amount)
	{
		current->amount_hash = block_a.hashables.source;
	}
	else
	{
		btcnew::block_info block_info;
		if (!store.block_info_get (transaction, block_a.hash (), block_info))
		{
			sum_add (block_info.balance.number ());
			current->balance_hash = 0;
		}
		else
		{
			current->amount_hash = block_a.hashables.source;
			current->balance_hash = block_a.hashables.previous;
		}
	}
}

void btcnew::summation_visitor::open_block (btcnew::open_block const & block_a)
{
	assert (current->type != summation_type::invalid && current != nullptr);
	if (current->type == summation_type::amount)
	{
		if (block_a.hashables.source != network_params.ledger.genesis_account)
		{
			current->amount_hash = block_a.hashables.source;
		}
		else
		{
			sum_set (network_params.ledger.genesis_amount);
			current->amount_hash = 0;
		}
	}
	else
	{
		current->amount_hash = block_a.hashables.source;
		current->balance_hash = 0;
	}
}

void btcnew::summation_visitor::change_block (btcnew::change_block const & block_a)
{
	assert (current->type != summation_type::invalid && current != nullptr);
	if (current->type == summation_type::amount)
	{
		sum_set (0);
		current->amount_hash = 0;
	}
	else
	{
		btcnew::block_info block_info;
		if (!store.block_info_get (transaction, block_a.hash (), block_info))
		{
			sum_add (block_info.balance.number ());
			current->balance_hash = 0;
		}
		else
		{
			current->balance_hash = block_a.hashables.previous;
		}
	}
}

btcnew::summation_visitor::frame btcnew::summation_visitor::push (btcnew::summation_visitor::summation_type type_a, btcnew::block_hash const & hash_a)
{
	frames.emplace (type_a, type_a == summation_type::balance ? hash_a : 0, type_a == summation_type::amount ? hash_a : 0);
	return frames.top ();
}

void btcnew::summation_visitor::sum_add (btcnew::uint128_t addend_a)
{
	current->sum += addend_a;
	result = current->sum;
}

void btcnew::summation_visitor::sum_set (btcnew::uint128_t value_a)
{
	current->sum = value_a;
	result = current->sum;
}

btcnew::uint128_t btcnew::summation_visitor::compute_internal (btcnew::summation_visitor::summation_type type_a, btcnew::block_hash const & hash_a)
{
	push (type_a, hash_a);

	/*
	 Invocation loop representing balance and amount computations calling each other.
	 This is usually better done by recursion or something like boost::coroutine2, but
	 segmented stacks are not supported on all platforms so we do it manually to avoid
	 stack overflow (the mutual calls are not tail-recursive so we cannot rely on the
	 compiler optimizing that into a loop, though a future alternative is to do a
	 CPS-style implementation to enforce tail calls.)
	*/
	while (!frames.empty ())
	{
		current = &frames.top ();
		assert (current->type != summation_type::invalid && current != nullptr);

		if (current->type == summation_type::balance)
		{
			if (current->awaiting_result)
			{
				sum_add (current->incoming_result);
				current->awaiting_result = false;
			}

			while (!current->awaiting_result && (!current->balance_hash.is_zero () || !current->amount_hash.is_zero ()))
			{
				if (!current->amount_hash.is_zero ())
				{
					// Compute amount
					current->awaiting_result = true;
					push (summation_type::amount, current->amount_hash);
					current->amount_hash = 0;
				}
				else
				{
					auto block (block_get (transaction, current->balance_hash));
					assert (block != nullptr);
					block->visit (*this);
				}
			}

			epilogue ();
		}
		else if (current->type == summation_type::amount)
		{
			if (current->awaiting_result)
			{
				sum_set (current->sum < current->incoming_result ? current->incoming_result - current->sum : current->sum - current->incoming_result);
				current->awaiting_result = false;
			}

			while (!current->awaiting_result && (!current->amount_hash.is_zero () || !current->balance_hash.is_zero ()))
			{
				if (!current->amount_hash.is_zero ())
				{
					auto block = block_get (transaction, current->amount_hash);
					if (block != nullptr)
					{
						block->visit (*this);
					}
					else
					{
						if (current->amount_hash == network_params.ledger.genesis_account)
						{
							sum_set ((std::numeric_limits<btcnew::uint128_t>::max) ());
							current->amount_hash = 0;
						}
						else
						{
							assert (false);
							sum_set (0);
							current->amount_hash = 0;
						}
					}
				}
				else
				{
					// Compute balance
					current->awaiting_result = true;
					push (summation_type::balance, current->balance_hash);
					current->balance_hash = 0;
				}
			}

			epilogue ();
		}
	}

	return result;
}

void btcnew::summation_visitor::epilogue ()
{
	if (!current->awaiting_result)
	{
		frames.pop ();
		if (!frames.empty ())
		{
			frames.top ().incoming_result = current->sum;
		}
	}
}

btcnew::uint128_t btcnew::summation_visitor::compute_amount (btcnew::block_hash const & block_hash)
{
	return compute_internal (summation_type::amount, block_hash);
}

btcnew::uint128_t btcnew::summation_visitor::compute_balance (btcnew::block_hash const & block_hash)
{
	return compute_internal (summation_type::balance, block_hash);
}

std::shared_ptr<btcnew::block> btcnew::summation_visitor::block_get (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a) const
{
	return is_v14_upgrade ? store.block_get_v14 (transaction, hash_a) : store.block_get (transaction, hash_a);
}

btcnew::representative_visitor::representative_visitor (btcnew::transaction const & transaction_a, btcnew::block_store & store_a) :
transaction (transaction_a),
store (store_a),
result (0)
{
}

void btcnew::representative_visitor::compute (btcnew::block_hash const & hash_a)
{
	current = hash_a;
	while (result.is_zero ())
	{
		auto block (store.block_get (transaction, current));
		assert (block != nullptr);
		block->visit (*this);
	}
}

void btcnew::representative_visitor::send_block (btcnew::send_block const & block_a)
{
	current = block_a.previous ();
}

void btcnew::representative_visitor::receive_block (btcnew::receive_block const & block_a)
{
	current = block_a.previous ();
}

void btcnew::representative_visitor::open_block (btcnew::open_block const & block_a)
{
	result = block_a.hash ();
}

void btcnew::representative_visitor::change_block (btcnew::change_block const & block_a)
{
	result = block_a.hash ();
}

void btcnew::representative_visitor::state_block (btcnew::state_block const & block_a)
{
	result = block_a.hash ();
}

btcnew::read_transaction::read_transaction (std::unique_ptr<btcnew::read_transaction_impl> read_transaction_impl) :
impl (std::move (read_transaction_impl))
{
}

void * btcnew::read_transaction::get_handle () const
{
	return impl->get_handle ();
}

void btcnew::read_transaction::reset () const
{
	impl->reset ();
}

void btcnew::read_transaction::renew () const
{
	impl->renew ();
}

void btcnew::read_transaction::refresh () const
{
	reset ();
	renew ();
}

btcnew::write_transaction::write_transaction (std::unique_ptr<btcnew::write_transaction_impl> write_transaction_impl) :
impl (std::move (write_transaction_impl))
{
	/*
	 * For IO threads, we do not want them to block on creating write transactions.
	 */
	assert (btcnew::thread_role::get () != btcnew::thread_role::name::io);
}

void * btcnew::write_transaction::get_handle () const
{
	return impl->get_handle ();
}

void btcnew::write_transaction::commit () const
{
	impl->commit ();
}

void btcnew::write_transaction::renew ()
{
	impl->renew ();
}

bool btcnew::write_transaction::contains (btcnew::tables table_a) const
{
	return impl->contains (table_a);
}

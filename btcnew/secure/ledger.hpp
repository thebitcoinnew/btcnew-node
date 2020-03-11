#pragma once

#include <btcnew/lib/config.hpp>
#include <btcnew/lib/rep_weights.hpp>
#include <btcnew/secure/common.hpp>

namespace btcnew
{
class block_store;
class stat;

using tally_t = std::map<btcnew::uint128_t, std::shared_ptr<btcnew::block>, std::greater<btcnew::uint128_t>>;
class ledger final
{
public:
	ledger (btcnew::block_store &, btcnew::stat &, bool = true, bool = true);
	btcnew::account account (btcnew::transaction const &, btcnew::block_hash const &) const;
	btcnew::uint128_t amount (btcnew::transaction const &, btcnew::account const &);
	btcnew::uint128_t amount (btcnew::transaction const &, btcnew::block_hash const &);
	btcnew::uint128_t balance (btcnew::transaction const &, btcnew::block_hash const &) const;
	btcnew::uint128_t account_balance (btcnew::transaction const &, btcnew::account const &);
	btcnew::uint128_t account_pending (btcnew::transaction const &, btcnew::account const &);
	btcnew::uint128_t weight (btcnew::account const &);
	std::shared_ptr<btcnew::block> successor (btcnew::transaction const &, btcnew::qualified_root const &);
	std::shared_ptr<btcnew::block> forked_block (btcnew::transaction const &, btcnew::block const &);
	bool block_confirmed (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a) const;
	bool block_not_confirmed_or_not_exists (btcnew::block const & block_a) const;
	btcnew::block_hash latest (btcnew::transaction const &, btcnew::account const &);
	btcnew::root latest_root (btcnew::transaction const &, btcnew::account const &);
	btcnew::block_hash representative (btcnew::transaction const &, btcnew::block_hash const &);
	btcnew::block_hash representative_calculated (btcnew::transaction const &, btcnew::block_hash const &);
	bool block_exists (btcnew::block_hash const &);
	bool block_exists (btcnew::block_type, btcnew::block_hash const &);
	std::string block_text (char const *);
	std::string block_text (btcnew::block_hash const &);
	bool is_send (btcnew::transaction const &, btcnew::state_block const &) const;
	btcnew::account const & block_destination (btcnew::transaction const &, btcnew::block const &);
	btcnew::block_hash block_source (btcnew::transaction const &, btcnew::block const &);
	btcnew::process_return process (btcnew::write_transaction const &, btcnew::block const &, btcnew::signature_verification = btcnew::signature_verification::unknown);
	bool rollback (btcnew::write_transaction const &, btcnew::block_hash const &, std::vector<std::shared_ptr<btcnew::block>> &);
	bool rollback (btcnew::write_transaction const &, btcnew::block_hash const &);
	void change_latest (btcnew::write_transaction const &, btcnew::account const &, btcnew::account_info const &, btcnew::account_info const &);
	void dump_account_chain (btcnew::account const &);
	bool could_fit (btcnew::transaction const &, btcnew::block const &);
	bool is_epoch_link (btcnew::link const &);
	btcnew::account const & epoch_signer (btcnew::link const &) const;
	btcnew::link const & epoch_link (btcnew::epoch) const;
	static btcnew::uint128_t const unit;
	btcnew::network_params network_params;
	btcnew::block_store & store;
	std::atomic<uint64_t> cemented_count{ 0 };
	std::atomic<uint64_t> block_count_cache{ 0 };
	btcnew::rep_weights rep_weights;
	btcnew::stat & stats;
	std::unordered_map<btcnew::account, btcnew::uint128_t> bootstrap_weights;
	std::atomic<size_t> bootstrap_weights_size{ 0 };
	uint64_t bootstrap_weight_max_blocks{ 1 };
	std::atomic<bool> check_bootstrap_weights;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (ledger & ledger, const std::string & name);
}

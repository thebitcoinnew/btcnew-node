#pragma once

#include <btcnew/node/active_transactions.hpp>
#include <btcnew/secure/blockstore.hpp>
#include <btcnew/secure/common.hpp>
#include <btcnew/secure/ledger.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <unordered_set>

namespace btcnew
{
class channel;
class node;
class vote_info final
{
public:
	std::chrono::steady_clock::time_point time;
	uint64_t sequence;
	btcnew::block_hash hash;
};
class election_vote_result final
{
public:
	election_vote_result () = default;
	election_vote_result (bool, bool);
	bool replay{ false };
	bool processed{ false };
};
class election final : public std::enable_shared_from_this<btcnew::election>
{
	std::function<void (std::shared_ptr<btcnew::block>)> confirmation_action;

public:
	election (btcnew::node &, std::shared_ptr<btcnew::block>, bool const, std::function<void (std::shared_ptr<btcnew::block>)> const &);
	btcnew::election_vote_result vote (btcnew::account, uint64_t, btcnew::block_hash);
	btcnew::tally_t tally ();
	// Check if we have vote quorum
	bool have_quorum (btcnew::tally_t const &, btcnew::uint128_t) const;
	// Change our winner to agree with the network
	void compute_rep_votes (btcnew::transaction const &);
	void confirm_once (btcnew::election_status_type = btcnew::election_status_type::active_confirmed_quorum);
	// Confirm this block if quorum is met
	void confirm_if_quorum ();
	void log_votes (btcnew::tally_t const &) const;
	bool publish (std::shared_ptr<btcnew::block> block_a);
	size_t last_votes_size ();
	void update_dependent ();
	void clear_dependent ();
	void clear_blocks ();
	void insert_inactive_votes_cache ();
	void stop ();
	btcnew::node & node;
	std::unordered_map<btcnew::account, btcnew::vote_info> last_votes;
	std::unordered_map<btcnew::block_hash, std::shared_ptr<btcnew::block>> blocks;
	std::chrono::steady_clock::time_point election_start;
	btcnew::election_status status;
	bool skip_delay;
	std::atomic<bool> confirmed;
	bool stopped;
	std::unordered_map<btcnew::block_hash, btcnew::uint128_t> last_tally;
	unsigned confirmation_request_count{ 0 };
	std::unordered_set<btcnew::block_hash> dependent_blocks;
	std::chrono::seconds late_blocks_delay{ 5 };
};
}

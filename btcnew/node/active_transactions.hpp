#pragma once

#include <btcnew/lib/numbers.hpp>
#include <btcnew/lib/timer.hpp>
#include <btcnew/node/gap_cache.hpp>
#include <btcnew/node/repcrawler.hpp>
#include <btcnew/node/transport/transport.hpp>
#include <btcnew/secure/blockstore.hpp>
#include <btcnew/secure/common.hpp>

#include <boost/circular_buffer.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/thread/thread.hpp>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace btcnew
{
class node;
class block;
class block_sideband;
class vote;
class election;
class transaction;

class conflict_info final
{
public:
	btcnew::qualified_root root;
	uint64_t difficulty;
	uint64_t adjusted_difficulty;
	std::shared_ptr<btcnew::election> election;
};

enum class election_status_type : uint8_t
{
	ongoing = 0,
	active_confirmed_quorum = 1,
	active_confirmation_height = 2,
	inactive_confirmation_height = 3,
	stopped = 5
};

class election_status final
{
public:
	std::shared_ptr<btcnew::block> winner;
	btcnew::amount tally;
	std::chrono::milliseconds election_end;
	std::chrono::milliseconds election_duration;
	unsigned confirmation_request_count;
	election_status_type type;
};

class cementable_account final
{
public:
	cementable_account (btcnew::account const & account_a, size_t blocks_uncemented_a);
	btcnew::account account;
	uint64_t blocks_uncemented{ 0 };
};

class election_timepoint final
{
public:
	std::chrono::steady_clock::time_point time;
	btcnew::qualified_root root;
};

// Core class for determining consensus
// Holds all active blocks i.e. recently added blocks that need confirmation
class active_transactions final
{
public:
	explicit active_transactions (btcnew::node &);
	~active_transactions ();
	// Start an election for a block
	// Call action with confirmed block, may be different than what we started with
	// clang-format off
	bool start (std::shared_ptr<btcnew::block>, bool const = false, std::function<void(std::shared_ptr<btcnew::block>)> const & = [](std::shared_ptr<btcnew::block>) {});
	// clang-format on
	// If this returns true, the vote is a replay
	// If this returns false, the vote may or may not be a replay
	bool vote (std::shared_ptr<btcnew::vote>, bool = false);
	// Is the root of this block in the roots container
	bool active (btcnew::block const &);
	bool active (btcnew::qualified_root const &);
	void update_difficulty (std::shared_ptr<btcnew::block>, boost::optional<btcnew::write_transaction const &> = boost::none);
	void adjust_difficulty (btcnew::block_hash const &);
	void update_active_difficulty (btcnew::unique_lock<std::mutex> &);
	uint64_t active_difficulty ();
	uint64_t limited_active_difficulty ();
	std::deque<std::shared_ptr<btcnew::block>> list_blocks (bool = false);
	void erase (btcnew::block const &);
	bool empty ();
	size_t size ();
	void stop ();
	bool publish (std::shared_ptr<btcnew::block> block_a);
	boost::optional<btcnew::election_status_type> confirm_block (btcnew::transaction const &, std::shared_ptr<btcnew::block>);
	void post_confirmation_height_set (btcnew::transaction const & transaction_a, std::shared_ptr<btcnew::block> block_a, btcnew::block_sideband const & sideband_a, btcnew::election_status_type election_status_type_a);
	boost::multi_index_container<
	btcnew::conflict_info,
	boost::multi_index::indexed_by<
	boost::multi_index::hashed_unique<
	boost::multi_index::member<btcnew::conflict_info, btcnew::qualified_root, &btcnew::conflict_info::root>>,
	boost::multi_index::ordered_non_unique<
	boost::multi_index::member<btcnew::conflict_info, uint64_t, &btcnew::conflict_info::adjusted_difficulty>,
	std::greater<uint64_t>>>>
	roots;
	std::unordered_map<btcnew::block_hash, std::shared_ptr<btcnew::election>> blocks;
	std::deque<btcnew::election_status> list_confirmed ();
	std::deque<btcnew::election_status> confirmed;
	void add_confirmed (btcnew::election_status const &, btcnew::qualified_root const &);
	void add_inactive_votes_cache (btcnew::block_hash const &, btcnew::account const &);
	btcnew::gap_information find_inactive_votes_cache (btcnew::block_hash const &);
	btcnew::node & node;
	std::mutex mutex;
	std::chrono::seconds const long_election_threshold;
	// Delay until requesting confirmation for an election
	std::chrono::milliseconds const election_request_delay;
	// Maximum time an election can be kept active if it is extending the container
	std::chrono::seconds const election_time_to_live;
	static size_t constexpr max_block_broadcasts = 30;
	static size_t constexpr max_confirm_representatives = 30;
	static size_t constexpr max_confirm_req_batches = 20;
	static size_t constexpr max_confirm_req = 5;
	boost::circular_buffer<double> multipliers_cb;
	uint64_t trended_active_difficulty;
	size_t priority_cementable_frontiers_size ();
	size_t priority_wallet_cementable_frontiers_size ();
	boost::circular_buffer<double> difficulty_trend ();
	size_t inactive_votes_cache_size ();
	std::unordered_map<btcnew::block_hash, std::shared_ptr<btcnew::election>> pending_conf_height;
	void clear_block (btcnew::block_hash const & hash_a);
	void add_dropped_elections_cache (btcnew::qualified_root const &);
	std::chrono::steady_clock::time_point find_dropped_elections_cache (btcnew::qualified_root const &);
	size_t dropped_elections_cache_size ();

private:
	// Call action with confirmed block, may be different than what we started with
	// clang-format off
	bool add (std::shared_ptr<btcnew::block>, bool const = false, std::function<void(std::shared_ptr<btcnew::block>)> const & = [](std::shared_ptr<btcnew::block>) {});
	// clang-format on
	void request_loop ();
	void search_frontiers (btcnew::transaction const &);
	void election_escalate (std::shared_ptr<btcnew::election> &, btcnew::transaction const &, size_t const &);
	void election_broadcast (std::shared_ptr<btcnew::election> &, btcnew::transaction const &, std::deque<std::shared_ptr<btcnew::block>> &, std::unordered_set<btcnew::qualified_root> &, btcnew::qualified_root &);
	bool election_request_confirm (std::shared_ptr<btcnew::election> &, std::vector<btcnew::representative> const &, size_t const &,
	std::deque<std::pair<std::shared_ptr<btcnew::block>, std::shared_ptr<std::vector<std::shared_ptr<btcnew::transport::channel>>>>> & single_confirm_req_bundle_l,
	std::unordered_map<std::shared_ptr<btcnew::transport::channel>, std::deque<std::pair<btcnew::block_hash, btcnew::root>>> & batched_confirm_req_bundle_l);
	void request_confirm (btcnew::unique_lock<std::mutex> &);
	btcnew::account next_frontier_account{ 0 };
	std::chrono::steady_clock::time_point next_frontier_check{ std::chrono::steady_clock::now () };
	btcnew::condition_variable condition;
	bool started{ false };
	std::atomic<bool> stopped{ false };
	unsigned ongoing_broadcasts{ 0 };
	using ordered_elections_timepoint = boost::multi_index_container<
	btcnew::election_timepoint,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<btcnew::election_timepoint, std::chrono::steady_clock::time_point, &btcnew::election_timepoint::time>>,
	boost::multi_index::hashed_unique<boost::multi_index::member<btcnew::election_timepoint, btcnew::qualified_root, &btcnew::election_timepoint::root>>>>;
	ordered_elections_timepoint confirmed_set;
	void prioritize_frontiers_for_confirmation (btcnew::transaction const &, std::chrono::milliseconds, std::chrono::milliseconds);
	using prioritize_num_uncemented = boost::multi_index_container<
	btcnew::cementable_account,
	boost::multi_index::indexed_by<
	boost::multi_index::hashed_unique<
	boost::multi_index::member<btcnew::cementable_account, btcnew::account, &btcnew::cementable_account::account>>,
	boost::multi_index::ordered_non_unique<
	boost::multi_index::member<btcnew::cementable_account, uint64_t, &btcnew::cementable_account::blocks_uncemented>,
	std::greater<uint64_t>>>>;
	prioritize_num_uncemented priority_wallet_cementable_frontiers;
	prioritize_num_uncemented priority_cementable_frontiers;
	std::unordered_set<btcnew::wallet_id> wallet_ids_already_iterated;
	std::unordered_map<btcnew::wallet_id, btcnew::account> next_wallet_id_accounts;
	bool skip_wallets{ false };
	void prioritize_account_for_confirmation (prioritize_num_uncemented &, size_t &, btcnew::account const &, btcnew::account_info const &, uint64_t);
	static size_t constexpr max_priority_cementable_frontiers{ 100000 };
	static size_t constexpr confirmed_frontiers_max_pending_cut_off{ 1000 };
	boost::multi_index_container<
	btcnew::gap_information,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<btcnew::gap_information, std::chrono::steady_clock::time_point, &btcnew::gap_information::arrival>>,
	boost::multi_index::hashed_unique<boost::multi_index::member<btcnew::gap_information, btcnew::block_hash, &btcnew::gap_information::hash>>>>
	inactive_votes_cache;
	static size_t constexpr inactive_votes_cache_max{ 16 * 1024 };
	ordered_elections_timepoint dropped_elections_cache;
	static size_t constexpr dropped_elections_cache_max{ 32 * 1024 };
	boost::thread thread;

	friend class confirmation_height_prioritize_frontiers_Test;
	friend class confirmation_height_prioritize_frontiers_overwrite_Test;
	friend class confirmation_height_many_accounts_single_confirmation_Test;
	friend class confirmation_height_many_accounts_many_confirmations_Test;
	friend class confirmation_height_long_chains_Test;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (active_transactions & active_transactions, const std::string & name);
}

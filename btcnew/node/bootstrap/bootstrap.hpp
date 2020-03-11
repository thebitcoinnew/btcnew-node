#pragma once

#include <btcnew/node/bootstrap/bootstrap_bulk_pull.hpp>
#include <btcnew/node/common.hpp>
#include <btcnew/node/socket.hpp>
#include <btcnew/secure/blockstore.hpp>
#include <btcnew/secure/ledger.hpp>

#include <boost/log/sources/logger.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/thread/thread.hpp>

#include <atomic>
#include <future>
#include <queue>
#include <stack>
#include <unordered_set>

namespace btcnew
{
class bootstrap_attempt;
class bootstrap_client;
class node;
namespace transport
{
	class channel_tcp;
}
enum class sync_result
{
	success,
	error,
	fork
};
enum class bootstrap_mode
{
	legacy,
	lazy,
	wallet_lazy
};
class lazy_state_backlog_item final
{
public:
	btcnew::link link{ 0 };
	btcnew::uint128_t balance{ 0 };
	unsigned retry_limit{ 0 };
};
class lazy_destinations_item final
{
public:
	btcnew::account account{ 0 };
	uint64_t count{ 0 };
};
class frontier_req_client;
class bulk_push_client;
class bootstrap_attempt final : public std::enable_shared_from_this<bootstrap_attempt>
{
public:
	explicit bootstrap_attempt (std::shared_ptr<btcnew::node> node_a, btcnew::bootstrap_mode mode_a = btcnew::bootstrap_mode::legacy);
	~bootstrap_attempt ();
	void run ();
	std::shared_ptr<btcnew::bootstrap_client> connection (btcnew::unique_lock<std::mutex> &, bool = false);
	bool consume_future (std::future<bool> &);
	void populate_connections ();
	void start_populate_connections ();
	bool request_frontier (btcnew::unique_lock<std::mutex> &, bool = false);
	void request_pull (btcnew::unique_lock<std::mutex> &);
	void request_push (btcnew::unique_lock<std::mutex> &);
	void add_connection (btcnew::endpoint const &);
	void connect_client (btcnew::tcp_endpoint const &);
	void pool_connection (std::shared_ptr<btcnew::bootstrap_client>);
	void stop ();
	void requeue_pull (btcnew::pull_info const &, bool = false);
	void add_pull (btcnew::pull_info const &);
	bool still_pulling ();
	void run_start (btcnew::unique_lock<std::mutex> &);
	unsigned target_connections (size_t pulls_remaining);
	bool should_log ();
	void add_bulk_push_target (btcnew::block_hash const &, btcnew::block_hash const &);
	void attempt_restart_check (btcnew::unique_lock<std::mutex> &);
	bool confirm_frontiers (btcnew::unique_lock<std::mutex> &);
	bool process_block (std::shared_ptr<btcnew::block>, btcnew::account const &, uint64_t, btcnew::bulk_pull::count_t, bool, unsigned);
	/** Lazy bootstrap */
	void lazy_run ();
	void lazy_start (btcnew::hash_or_account const &, bool confirmed = true);
	void lazy_add (btcnew::hash_or_account const &, unsigned = std::numeric_limits<unsigned>::max ());
	void lazy_requeue (btcnew::block_hash const &, btcnew::block_hash const &, bool);
	bool lazy_finished ();
	bool lazy_has_expired () const;
	void lazy_pull_flush ();
	void lazy_clear ();
	bool process_block_lazy (std::shared_ptr<btcnew::block>, btcnew::account const &, uint64_t, btcnew::bulk_pull::count_t, unsigned);
	void lazy_block_state (std::shared_ptr<btcnew::block>, unsigned);
	void lazy_block_state_backlog_check (std::shared_ptr<btcnew::block>, btcnew::block_hash const &);
	void lazy_backlog_cleanup ();
	void lazy_destinations_increment (btcnew::account const &);
	void lazy_destinations_flush ();
	bool lazy_processed_or_exists (btcnew::block_hash const &);
	/** Lazy bootstrap */
	/** Wallet bootstrap */
	void request_pending (btcnew::unique_lock<std::mutex> &);
	void requeue_pending (btcnew::account const &);
	void wallet_run ();
	void wallet_start (std::deque<btcnew::account> &);
	bool wallet_finished ();
	/** Wallet bootstrap */
	std::mutex next_log_mutex;
	std::chrono::steady_clock::time_point next_log;
	std::deque<std::weak_ptr<btcnew::bootstrap_client>> clients;
	std::weak_ptr<btcnew::bootstrap_client> connection_frontier_request;
	btcnew::tcp_endpoint endpoint_frontier_request;
	std::weak_ptr<btcnew::frontier_req_client> frontiers;
	std::weak_ptr<btcnew::bulk_push_client> push;
	std::deque<btcnew::pull_info> pulls;
	std::deque<btcnew::block_hash> recent_pulls_head;
	std::deque<std::shared_ptr<btcnew::bootstrap_client>> idle;
	std::atomic<unsigned> connections{ 0 };
	std::atomic<unsigned> pulling{ 0 };
	std::shared_ptr<btcnew::node> node;
	std::atomic<unsigned> account_count{ 0 };
	std::atomic<uint64_t> total_blocks{ 0 };
	std::atomic<unsigned> runs_count{ 0 };
	std::atomic<unsigned> requeued_pulls{ 0 };
	std::vector<std::pair<btcnew::block_hash, btcnew::block_hash>> bulk_push_targets;
	std::atomic<bool> frontiers_received{ false };
	std::atomic<bool> frontiers_confirmed{ false };
	std::atomic<bool> populate_connections_started{ false };
	std::atomic<bool> stopped{ false };
	std::chrono::steady_clock::time_point attempt_start{ std::chrono::steady_clock::now () };
	btcnew::bootstrap_mode mode;
	std::mutex mutex;
	btcnew::condition_variable condition;
	// Lazy bootstrap
	std::unordered_set<btcnew::block_hash> lazy_blocks;
	std::unordered_map<btcnew::block_hash, btcnew::lazy_state_backlog_item> lazy_state_backlog;
	std::unordered_set<btcnew::block_hash> lazy_undefined_links;
	std::unordered_map<btcnew::block_hash, btcnew::uint128_t> lazy_balances;
	std::unordered_set<btcnew::block_hash> lazy_keys;
	std::deque<std::pair<btcnew::hash_or_account, unsigned>> lazy_pulls;
	std::chrono::steady_clock::time_point lazy_start_time;
	std::chrono::steady_clock::time_point last_lazy_flush{ std::chrono::steady_clock::now () };
	class account_tag
	{
	};
	class count_tag
	{
	};
	boost::multi_index_container<
	lazy_destinations_item,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::tag<count_tag>, boost::multi_index::member<lazy_destinations_item, uint64_t, &lazy_destinations_item::count>, std::greater<uint64_t>>,
	boost::multi_index::hashed_unique<boost::multi_index::tag<account_tag>, boost::multi_index::member<lazy_destinations_item, btcnew::account, &lazy_destinations_item::account>>>>
	lazy_destinations;
	std::atomic<size_t> lazy_blocks_count{ 0 };
	std::atomic<bool> lazy_destinations_flushed{ false };
	std::mutex lazy_mutex;
	// Wallet lazy bootstrap
	std::deque<btcnew::account> wallet_accounts;
};
class bootstrap_client final : public std::enable_shared_from_this<bootstrap_client>
{
public:
	bootstrap_client (std::shared_ptr<btcnew::node>, std::shared_ptr<btcnew::bootstrap_attempt>, std::shared_ptr<btcnew::transport::channel_tcp>, std::shared_ptr<btcnew::socket>);
	~bootstrap_client ();
	std::shared_ptr<btcnew::bootstrap_client> shared ();
	void stop (bool force);
	double block_rate () const;
	double elapsed_seconds () const;
	std::shared_ptr<btcnew::node> node;
	std::shared_ptr<btcnew::bootstrap_attempt> attempt;
	std::shared_ptr<btcnew::transport::channel_tcp> channel;
	std::shared_ptr<btcnew::socket> socket;
	std::shared_ptr<std::vector<uint8_t>> receive_buffer;
	std::chrono::steady_clock::time_point start_time;
	std::atomic<uint64_t> block_count;
	std::atomic<bool> pending_stop;
	std::atomic<bool> hard_stop;
};
class cached_pulls final
{
public:
	std::chrono::steady_clock::time_point time;
	btcnew::uint512_union account_head;
	btcnew::block_hash new_head;
};
class pulls_cache final
{
public:
	void add (btcnew::pull_info const &);
	void update_pull (btcnew::pull_info &);
	void remove (btcnew::pull_info const &);
	std::mutex pulls_cache_mutex;
	class account_head_tag
	{
	};
	boost::multi_index_container<
	btcnew::cached_pulls,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<btcnew::cached_pulls, std::chrono::steady_clock::time_point, &btcnew::cached_pulls::time>>,
	boost::multi_index::hashed_unique<boost::multi_index::tag<account_head_tag>, boost::multi_index::member<btcnew::cached_pulls, btcnew::uint512_union, &btcnew::cached_pulls::account_head>>>>
	cache;
	constexpr static size_t cache_size_max = 10000;
};
class excluded_peers_item final
{
public:
	std::chrono::steady_clock::time_point exclude_until;
	btcnew::tcp_endpoint endpoint;
	uint64_t score;
};
class bootstrap_excluded_peers final
{
public:
	uint64_t add (btcnew::tcp_endpoint const &, size_t);
	bool check (btcnew::tcp_endpoint const &);
	void remove (btcnew::tcp_endpoint const &);
	std::mutex excluded_peers_mutex;
	class endpoint_tag
	{
	};
	boost::multi_index_container<
	btcnew::excluded_peers_item,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<btcnew::excluded_peers_item, std::chrono::steady_clock::time_point, &btcnew::excluded_peers_item::exclude_until>>,
	boost::multi_index::hashed_unique<boost::multi_index::tag<endpoint_tag>, boost::multi_index::member<btcnew::excluded_peers_item, btcnew::tcp_endpoint, &btcnew::excluded_peers_item::endpoint>>>>
	peers;
	constexpr static size_t excluded_peers_size_max = 5000;
	constexpr static double excluded_peers_percentage_limit = 0.5;
	constexpr static uint64_t score_limit = 2;
	constexpr static std::chrono::hours exclude_time_hours = std::chrono::hours (1);
	constexpr static std::chrono::hours exclude_remove_hours = std::chrono::hours (24);
};

class bootstrap_initiator final
{
public:
	explicit bootstrap_initiator (btcnew::node &);
	~bootstrap_initiator ();
	void bootstrap (btcnew::endpoint const &, bool add_to_peers = true, bool frontiers_confirmed = false);
	void bootstrap (bool force = false);
	void bootstrap_lazy (btcnew::hash_or_account const &, bool force = false, bool confirmed = true);
	void bootstrap_wallet (std::deque<btcnew::account> &);
	void run_bootstrap ();
	void notify_listeners (bool);
	void add_observer (std::function<void (bool)> const &);
	bool in_progress ();
	std::shared_ptr<btcnew::bootstrap_attempt> current_attempt ();
	btcnew::pulls_cache cache;
	btcnew::bootstrap_excluded_peers excluded_peers;
	void stop ();

private:
	btcnew::node & node;
	std::shared_ptr<btcnew::bootstrap_attempt> attempt;
	std::atomic<bool> stopped;
	std::mutex mutex;
	btcnew::condition_variable condition;
	std::mutex observers_mutex;
	std::vector<std::function<void (bool)>> observers;
	boost::thread thread;

	friend std::unique_ptr<seq_con_info_component> collect_seq_con_info (bootstrap_initiator & bootstrap_initiator, const std::string & name);
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (bootstrap_initiator & bootstrap_initiator, const std::string & name);
class bootstrap_limits final
{
public:
	static constexpr double bootstrap_connection_scale_target_blocks = 50000.0;
	static constexpr double bootstrap_connection_scale_target_blocks_lazy = bootstrap_connection_scale_target_blocks / 5;
	static constexpr double bootstrap_connection_warmup_time_sec = 5.0;
	static constexpr double bootstrap_minimum_blocks_per_sec = 10.0;
	static constexpr double bootstrap_minimum_elapsed_seconds_blockrate = 0.02;
	static constexpr double bootstrap_minimum_frontier_blocks_per_sec = 1000.0;
	static constexpr double bootstrap_minimum_termination_time_sec = 30.0;
	static constexpr unsigned bootstrap_max_new_connections = 32;
	static constexpr size_t bootstrap_max_confirm_frontiers = 70;
	static constexpr double required_frontier_confirmation_ratio = 0.8;
	static constexpr unsigned frontier_confirmation_blocks_limit = 128 * 1024;
	static constexpr unsigned requeued_pulls_limit = 256;
	static constexpr unsigned requeued_pulls_limit_test = 2;
	static constexpr unsigned bulk_push_cost_limit = 200;
	static constexpr std::chrono::seconds lazy_flush_delay_sec = std::chrono::seconds (5);
	static constexpr unsigned lazy_destinations_request_limit = 256 * 1024;
	static constexpr uint64_t lazy_batch_pull_count_resize_blocks_limit = 4 * 1024 * 1024;
	static constexpr double lazy_batch_pull_count_resize_ratio = 2.0;
	static constexpr size_t lazy_blocks_restart_limit = 1024 * 1024;
};
}

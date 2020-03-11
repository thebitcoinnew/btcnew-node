#pragma once

#include <btcnew/boost/asio.hpp>
#include <btcnew/lib/alarm.hpp>
#include <btcnew/lib/rep_weights.hpp>
#include <btcnew/lib/stats.hpp>
#include <btcnew/lib/work.hpp>
#include <btcnew/node/active_transactions.hpp>
#include <btcnew/node/blockprocessor.hpp>
#include <btcnew/node/bootstrap/bootstrap.hpp>
#include <btcnew/node/bootstrap/bootstrap_bulk_pull.hpp>
#include <btcnew/node/bootstrap/bootstrap_bulk_push.hpp>
#include <btcnew/node/bootstrap/bootstrap_frontier.hpp>
#include <btcnew/node/bootstrap/bootstrap_server.hpp>
#include <btcnew/node/confirmation_height_processor.hpp>
#include <btcnew/node/distributed_work.hpp>
#include <btcnew/node/election.hpp>
#include <btcnew/node/gap_cache.hpp>
#include <btcnew/node/logging.hpp>
#include <btcnew/node/network.hpp>
#include <btcnew/node/node_observers.hpp>
#include <btcnew/node/nodeconfig.hpp>
#include <btcnew/node/online_reps.hpp>
#include <btcnew/node/payment_observer_processor.hpp>
#include <btcnew/node/portmapping.hpp>
#include <btcnew/node/repcrawler.hpp>
#include <btcnew/node/signatures.hpp>
#include <btcnew/node/vote_processor.hpp>
#include <btcnew/node/wallet.hpp>
#include <btcnew/node/websocket.hpp>
#include <btcnew/node/write_database_queue.hpp>
#include <btcnew/secure/ledger.hpp>

#include <boost/iostreams/device/array.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/thread/latch.hpp>
#include <boost/thread/thread.hpp>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <queue>
#include <vector>

namespace btcnew
{
class node;

class work_pool;
class block_arrival_info final
{
public:
	std::chrono::steady_clock::time_point arrival;
	btcnew::block_hash hash;
};
// This class tracks blocks that are probably live because they arrived in a UDP packet
// This gives a fairly reliable way to differentiate between blocks being inserted via bootstrap or new, live blocks.
class block_arrival final
{
public:
	// Return `true' to indicated an error if the block has already been inserted
	bool add (btcnew::block_hash const &);
	bool recent (btcnew::block_hash const &);
	boost::multi_index_container<
	btcnew::block_arrival_info,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<btcnew::block_arrival_info, std::chrono::steady_clock::time_point, &btcnew::block_arrival_info::arrival>>,
	boost::multi_index::hashed_unique<boost::multi_index::member<btcnew::block_arrival_info, btcnew::block_hash, &btcnew::block_arrival_info::hash>>>>
	arrival;
	std::mutex mutex;
	static size_t constexpr arrival_size_min = 8 * 1024;
	static std::chrono::seconds constexpr arrival_time_min = std::chrono::seconds (300);
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (block_arrival & block_arrival, const std::string & name);

std::unique_ptr<seq_con_info_component> collect_seq_con_info (rep_crawler & rep_crawler, const std::string & name);
std::unique_ptr<seq_con_info_component> collect_seq_con_info (block_processor & block_processor, const std::string & name);

class node final : public std::enable_shared_from_this<btcnew::node>
{
public:
	node (boost::asio::io_context &, uint16_t, boost::filesystem::path const &, btcnew::alarm &, btcnew::logging const &, btcnew::work_pool &, btcnew::node_flags = btcnew::node_flags ());
	node (boost::asio::io_context &, boost::filesystem::path const &, btcnew::alarm &, btcnew::node_config const &, btcnew::work_pool &, btcnew::node_flags = btcnew::node_flags ());
	~node ();
	template <typename T>
	void background (T action_a)
	{
		alarm.io_ctx.post (action_a);
	}
	bool copy_with_compaction (boost::filesystem::path const &);
	void keepalive (std::string const &, uint16_t);
	void start ();
	void stop ();
	std::shared_ptr<btcnew::node> shared ();
	int store_version ();
	void receive_confirmed (btcnew::transaction const &, std::shared_ptr<btcnew::block>, btcnew::block_hash const &);
	void process_confirmed_data (btcnew::transaction const &, std::shared_ptr<btcnew::block>, btcnew::block_hash const &, btcnew::block_sideband const &, btcnew::account &, btcnew::uint128_t &, bool &, btcnew::account &);
	void process_confirmed (btcnew::election_status const &, uint8_t = 0);
	void process_active (std::shared_ptr<btcnew::block>);
	btcnew::process_return process (btcnew::block const &);
	btcnew::process_return process_local (std::shared_ptr<btcnew::block>, bool const = false);
	void keepalive_preconfigured (std::vector<std::string> const &);
	btcnew::block_hash latest (btcnew::account const &);
	btcnew::uint128_t balance (btcnew::account const &);
	std::shared_ptr<btcnew::block> block (btcnew::block_hash const &);
	std::pair<btcnew::uint128_t, btcnew::uint128_t> balance_pending (btcnew::account const &);
	btcnew::uint128_t weight (btcnew::account const &);
	btcnew::block_hash rep_block (btcnew::account const &);
	btcnew::uint128_t minimum_principal_weight ();
	btcnew::uint128_t minimum_principal_weight (btcnew::uint128_t const &);
	void ongoing_rep_calculation ();
	void ongoing_bootstrap ();
	void ongoing_store_flush ();
	void ongoing_peer_store ();
	void ongoing_unchecked_cleanup ();
	void backup_wallet ();
	void search_pending ();
	void bootstrap_wallet ();
	void unchecked_cleanup ();
	int price (btcnew::uint128_t const &, int);
	bool local_work_generation_enabled () const;
	bool work_generation_enabled () const;
	bool work_generation_enabled (std::vector<std::pair<std::string, uint16_t>> const &) const;
	boost::optional<uint64_t> work_generate_blocking (btcnew::block &, uint64_t);
	boost::optional<uint64_t> work_generate_blocking (btcnew::block &);
	boost::optional<uint64_t> work_generate_blocking (btcnew::root const &, uint64_t, boost::optional<btcnew::account> const & = boost::none);
	boost::optional<uint64_t> work_generate_blocking (btcnew::root const &, boost::optional<btcnew::account> const & = boost::none);
	void work_generate (btcnew::root const &, std::function<void (boost::optional<uint64_t>)>, uint64_t, boost::optional<btcnew::account> const & = boost::none, bool const = false);
	void work_generate (btcnew::root const &, std::function<void (boost::optional<uint64_t>)>, boost::optional<btcnew::account> const & = boost::none);
	void add_initial_peers ();
	void block_confirm (std::shared_ptr<btcnew::block>);
	bool block_confirmed_or_being_confirmed (btcnew::transaction const &, btcnew::block_hash const &);
	void process_fork (btcnew::transaction const &, std::shared_ptr<btcnew::block>);
	bool validate_block_by_previous (btcnew::transaction const &, std::shared_ptr<btcnew::block>);
	void do_rpc_callback (boost::asio::ip::tcp::resolver::iterator i_a, std::string const &, uint16_t, std::shared_ptr<std::string>, std::shared_ptr<std::string>, std::shared_ptr<boost::asio::ip::tcp::resolver>);
	btcnew::uint128_t delta () const;
	void ongoing_online_weight_calculation ();
	void ongoing_online_weight_calculation_queue ();
	bool online () const;
	bool init_error () const;
	btcnew::worker worker;
	btcnew::write_database_queue write_database_queue;
	boost::asio::io_context & io_ctx;
	boost::latch node_initialized_latch;
	btcnew::network_params network_params;
	btcnew::node_config config;
	btcnew::stat stats;
	std::shared_ptr<btcnew::websocket::listener> websocket_server;
	btcnew::node_flags flags;
	btcnew::alarm & alarm;
	btcnew::work_pool & work;
	btcnew::distributed_work_factory distributed_work;
	btcnew::logger_mt logger;
	std::unique_ptr<btcnew::block_store> store_impl;
	btcnew::block_store & store;
	std::unique_ptr<btcnew::wallets_store> wallets_store_impl;
	btcnew::wallets_store & wallets_store;
	btcnew::gap_cache gap_cache;
	btcnew::ledger ledger;
	btcnew::signature_checker checker;
	btcnew::network network;
	btcnew::bootstrap_initiator bootstrap_initiator;
	btcnew::bootstrap_listener bootstrap;
	boost::filesystem::path application_path;
	btcnew::node_observers observers;
	btcnew::port_mapping port_mapping;
	btcnew::vote_processor vote_processor;
	btcnew::rep_crawler rep_crawler;
	unsigned warmed_up;
	btcnew::block_processor block_processor;
	boost::thread block_processor_thread;
	btcnew::block_arrival block_arrival;
	btcnew::online_reps online_reps;
	btcnew::votes_cache votes_cache;
	btcnew::keypair node_id;
	btcnew::block_uniquer block_uniquer;
	btcnew::vote_uniquer vote_uniquer;
	btcnew::pending_confirmation_height pending_confirmation_height; // Used by both active and confirmation height processor
	btcnew::active_transactions active;
	btcnew::confirmation_height_processor confirmation_height_processor;
	btcnew::payment_observer_processor payment_observer_processor;
	btcnew::wallets wallets;
	const std::chrono::steady_clock::time_point startup_time;
	std::chrono::seconds unchecked_cutoff = std::chrono::seconds (7 * 24 * 60 * 60); // Week
	std::atomic<bool> unresponsive_work_peers{ false };
	std::atomic<bool> stopped{ false };
	static double constexpr price_max = 16.0;
	static double constexpr free_cutoff = 1024.0;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (node & node, const std::string & name);

btcnew::node_flags const & inactive_node_flag_defaults ();

class inactive_node final
{
public:
	inactive_node (boost::filesystem::path const & path = btcnew::working_path (), uint16_t = 24000, btcnew::node_flags const & = btcnew::inactive_node_flag_defaults ());
	~inactive_node ();
	boost::filesystem::path path;
	std::shared_ptr<boost::asio::io_context> io_context;
	btcnew::alarm alarm;
	btcnew::logging logging;
	btcnew::work_pool work;
	uint16_t peering_port;
	std::shared_ptr<btcnew::node> node;
};
}

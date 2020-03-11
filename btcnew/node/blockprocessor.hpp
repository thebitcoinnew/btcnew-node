#pragma once

#include <btcnew/lib/blocks.hpp>
#include <btcnew/node/voting.hpp>
#include <btcnew/secure/common.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>

#include <chrono>
#include <memory>
#include <unordered_set>

namespace btcnew
{
class node;
class transaction;
class write_transaction;
class write_database_queue;

class rolled_hash
{
public:
	std::chrono::steady_clock::time_point time;
	btcnew::block_hash hash;
};
/**
 * Processing blocks is a potentially long IO operation.
 * This class isolates block insertion from other operations like servicing network operations
 */
class block_processor final
{
public:
	explicit block_processor (btcnew::node &, btcnew::write_database_queue &);
	~block_processor ();
	void stop ();
	void flush ();
	size_t size ();
	bool full ();
	bool half_full ();
	void add (btcnew::unchecked_info const &);
	void add (std::shared_ptr<btcnew::block>, uint64_t = 0);
	void force (std::shared_ptr<btcnew::block>);
	void wait_write ();
	bool should_log (bool);
	bool have_blocks ();
	void process_blocks ();
	btcnew::process_return process_one (btcnew::write_transaction const &, btcnew::unchecked_info, const bool = false);
	btcnew::process_return process_one (btcnew::write_transaction const &, std::shared_ptr<btcnew::block>, const bool = false);
	btcnew::vote_generator generator;
	// Delay required for average network propagartion before requesting confirmation
	static std::chrono::milliseconds constexpr confirmation_request_delay{ 1500 };

private:
	void queue_unchecked (btcnew::write_transaction const &, btcnew::block_hash const &);
	void verify_state_blocks (btcnew::unique_lock<std::mutex> &, size_t = std::numeric_limits<size_t>::max ());
	void process_batch (btcnew::unique_lock<std::mutex> &);
	void process_live (btcnew::block_hash const &, std::shared_ptr<btcnew::block>, const bool = false);
	void requeue_invalid (btcnew::block_hash const &, btcnew::unchecked_info const &);
	bool stopped;
	bool active;
	bool awaiting_write{ false };
	std::chrono::steady_clock::time_point next_log;
	std::deque<btcnew::unchecked_info> state_blocks;
	std::deque<btcnew::unchecked_info> blocks;
	std::deque<std::shared_ptr<btcnew::block>> forced;
	btcnew::block_hash filter_item (btcnew::block_hash const &, btcnew::signature const &);
	std::unordered_set<btcnew::block_hash> blocks_filter;
	boost::multi_index_container<
	btcnew::rolled_hash,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<btcnew::rolled_hash, std::chrono::steady_clock::time_point, &btcnew::rolled_hash::time>>,
	boost::multi_index::hashed_unique<boost::multi_index::member<btcnew::rolled_hash, btcnew::block_hash, &btcnew::rolled_hash::hash>>>>
	rolled_back;
	static size_t const rolled_back_max = 1024;
	btcnew::condition_variable condition;
	btcnew::node & node;
	btcnew::write_database_queue & write_database_queue;
	std::mutex mutex;

	friend std::unique_ptr<seq_con_info_component> collect_seq_con_info (block_processor & block_processor, const std::string & name);
};
}

#pragma once

#include <btcnew/lib/config.hpp>
#include <btcnew/lib/numbers.hpp>
#include <btcnew/lib/utility.hpp>
#include <btcnew/secure/common.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/thread.hpp>

#include <condition_variable>
#include <deque>
#include <mutex>

namespace btcnew
{
class node;
class vote_generator final
{
public:
	vote_generator (btcnew::node &);
	void add (btcnew::block_hash const &);
	void stop ();

private:
	void run ();
	void send (btcnew::unique_lock<std::mutex> &);
	btcnew::node & node;
	std::mutex mutex;
	btcnew::condition_variable condition;
	std::deque<btcnew::block_hash> hashes;
	btcnew::network_params network_params;
	bool stopped{ false };
	bool started{ false };
	boost::thread thread;

	friend std::unique_ptr<seq_con_info_component> collect_seq_con_info (vote_generator & vote_generator, const std::string & name);
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (vote_generator & vote_generator, const std::string & name);
class cached_votes final
{
public:
	std::chrono::steady_clock::time_point time;
	btcnew::block_hash hash;
	std::vector<std::shared_ptr<btcnew::vote>> votes;
};
class votes_cache final
{
public:
	void add (std::shared_ptr<btcnew::vote> const &);
	std::vector<std::shared_ptr<btcnew::vote>> find (btcnew::block_hash const &);
	void remove (btcnew::block_hash const &);

private:
	std::mutex cache_mutex;
	boost::multi_index_container<
	btcnew::cached_votes,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<btcnew::cached_votes, std::chrono::steady_clock::time_point, &btcnew::cached_votes::time>>,
	boost::multi_index::hashed_unique<boost::multi_index::member<btcnew::cached_votes, btcnew::block_hash, &btcnew::cached_votes::hash>>>>
	cache;
	btcnew::network_params network_params;
	friend std::unique_ptr<seq_con_info_component> collect_seq_con_info (votes_cache & votes_cache, const std::string & name);
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (votes_cache & votes_cache, const std::string & name);
}

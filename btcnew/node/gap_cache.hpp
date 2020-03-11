#pragma once

#include <btcnew/lib/numbers.hpp>
#include <btcnew/lib/utility.hpp>
#include <btcnew/secure/common.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>

#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

namespace btcnew
{
class node;
class transaction;

/** For each gap in account chains, track arrival time and voters */
class gap_information final
{
public:
	std::chrono::steady_clock::time_point arrival;
	btcnew::block_hash hash;
	std::vector<btcnew::account> voters;
	bool confirmed{ false };
};

/** Maintains voting and arrival information for gaps (missing source or previous blocks in account chains) */
class gap_cache final
{
public:
	explicit gap_cache (btcnew::node &);
	void add (btcnew::block_hash const &, std::chrono::steady_clock::time_point = std::chrono::steady_clock::now ());
	void erase (btcnew::block_hash const & hash_a);
	void vote (std::shared_ptr<btcnew::vote>);
	bool bootstrap_check (std::vector<btcnew::account> const &, btcnew::block_hash const &);
	btcnew::uint128_t bootstrap_threshold ();
	size_t size ();
	boost::multi_index_container<
	btcnew::gap_information,
	boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<boost::multi_index::member<gap_information, std::chrono::steady_clock::time_point, &gap_information::arrival>>,
	boost::multi_index::hashed_unique<boost::multi_index::member<gap_information, btcnew::block_hash, &gap_information::hash>>>>
	blocks;
	size_t const max = 256;
	std::mutex mutex;
	btcnew::node & node;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (gap_cache & gap_cache, const std::string & name);
}

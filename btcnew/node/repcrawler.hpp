#pragma once

#include <btcnew/boost/asio.hpp>
#include <btcnew/node/common.hpp>
#include <btcnew/node/transport/transport.hpp>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>

#include <chrono>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace mi = boost::multi_index;

namespace btcnew
{
class node;

/**
 * A representative picked up during repcrawl.
 */
class representative
{
public:
	representative () = default;
	representative (btcnew::account account_a, btcnew::amount weight_a, std::shared_ptr<btcnew::transport::channel> channel_a) :
	account (account_a), weight (weight_a), channel (channel_a)
	{
	}
	std::reference_wrapper<btcnew::transport::channel const> channel_ref () const
	{
		return *channel;
	};
	bool operator== (btcnew::representative const & other_a) const
	{
		return account == other_a.account;
	}
	btcnew::account account{ 0 };
	btcnew::amount weight{ 0 };
	std::shared_ptr<btcnew::transport::channel> channel;
	std::chrono::steady_clock::time_point last_request{ std::chrono::steady_clock::time_point () };
	std::chrono::steady_clock::time_point last_response{ std::chrono::steady_clock::time_point () };
};

/**
 * Crawls the network for representatives. Queries are performed by requesting confirmation of a
 * random block and observing the corresponding vote.
 */
class rep_crawler
{
	friend std::unique_ptr<seq_con_info_component> collect_seq_con_info (rep_crawler & rep_crawler, const std::string & name);

	// clang-format off
	class tag_account {};
	class tag_channel_ref {};
	class tag_last_request {};
	class tag_weight {};

	using probably_rep_t = boost::multi_index_container<representative,
	mi::indexed_by<
		mi::hashed_unique<mi::member<representative, btcnew::account, &representative::account>>,
		mi::random_access<>,
		mi::ordered_non_unique<mi::tag<tag_last_request>,
			mi::member<representative, std::chrono::steady_clock::time_point, &representative::last_request>>,
		mi::ordered_non_unique<mi::tag<tag_weight>,
			mi::member<representative, btcnew::amount, &representative::weight>, std::greater<btcnew::amount>>,
		mi::hashed_non_unique<mi::tag<tag_channel_ref>,
			mi::const_mem_fun<representative, std::reference_wrapper<btcnew::transport::channel const>, &representative::channel_ref>>>>;
	// clang-format on

public:
	rep_crawler (btcnew::node & node_a);

	/** Start crawling */
	void start ();

	/** Add block hash to list of active rep queries */
	void add (btcnew::block_hash const &);

	/** Remove block hash from list of active rep queries */
	void remove (btcnew::block_hash const &);

	/** Check if block hash is in the list of active rep queries */
	bool exists (btcnew::block_hash const &);

	/** Attempt to determine if the peer manages one or more representative accounts */
	void query (std::vector<std::shared_ptr<btcnew::transport::channel>> const & channels_a);

	/** Attempt to determine if the peer manages one or more representative accounts */
	void query (std::shared_ptr<btcnew::transport::channel> channel_a);

	/**
	 * Called when a non-replay vote on a block previously sent by query() is received. This indiciates
	 * with high probability that the endpoint is a representative node.
	 * @return True if the rep entry was updated with new information due to increase in weight.
	 */
	bool response (std::shared_ptr<btcnew::transport::channel> channel_a, btcnew::account const & rep_account_a, btcnew::amount const & weight_a);

	/** Get total available weight from representatives */
	btcnew::uint128_t total_weight () const;

	/** Request a list of the top \p count_a known representatives in descending order of weight. */
	std::vector<representative> representatives (size_t count_a = std::numeric_limits<size_t>::max ());

	/** Request a list of the top \p count_a known representative endpoints. */
	std::vector<std::shared_ptr<btcnew::transport::channel>> representative_endpoints (size_t count_a);

	/** Total number of representatives */
	size_t representative_count ();

private:
	btcnew::node & node;

	/** Protects the active-hash container */
	std::mutex active_mutex;

	/** We have solicted votes for these random blocks */
	std::unordered_set<btcnew::block_hash> active;

	/** Called continuously to crawl for representatives */
	void ongoing_crawl ();

	/** Returns a list of endpoints to crawl. The total weight is passed in to avoid computing it twice. */
	std::vector<std::shared_ptr<btcnew::transport::channel>> get_crawl_targets (btcnew::uint128_t total_weight_a);

	/** When a rep request is made, this is called to update the last-request timestamp. */
	void on_rep_request (std::shared_ptr<btcnew::transport::channel> channel_a);

	/** Clean representatives with inactive channels */
	void cleanup_reps ();

	/** Update representatives weights from ledger */
	void update_weights ();

	/** Protects the probable_reps container */
	mutable std::mutex probable_reps_mutex;

	/** Probable representatives */
	probably_rep_t probable_reps;
};
}

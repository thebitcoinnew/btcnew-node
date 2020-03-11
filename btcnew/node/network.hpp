#pragma once

#include <btcnew/boost/asio.hpp>
#include <btcnew/node/common.hpp>
#include <btcnew/node/transport/tcp.hpp>
#include <btcnew/node/transport/udp.hpp>

#include <boost/thread/thread.hpp>

#include <memory>
#include <queue>

namespace btcnew
{
class channel;
class node;
class stats;
class transaction;
class message_buffer final
{
public:
	uint8_t * buffer{ nullptr };
	size_t size{ 0 };
	btcnew::endpoint endpoint;
};
/**
  * A circular buffer for servicing btcnew realtime messages.
  * This container follows a producer/consumer model where the operating system is producing data in to
  * buffers which are serviced by internal threads.
  * If buffers are not serviced fast enough they're internally dropped.
  * This container has a maximum space to hold N buffers of M size and will allocate them in round-robin order.
  * All public methods are thread-safe
*/
class message_buffer_manager final
{
public:
	// Stats - Statistics
	// Size - Size of each individual buffer
	// Count - Number of buffers to allocate
	message_buffer_manager (btcnew::stat & stats, size_t, size_t);
	// Return a buffer where message data can be put
	// Method will attempt to return the first free buffer
	// If there are no free buffers, an unserviced buffer will be dequeued and returned
	// Function will block if there are no free or unserviced buffers
	// Return nullptr if the container has stopped
	btcnew::message_buffer * allocate ();
	// Queue a buffer that has been filled with message data and notify servicing threads
	void enqueue (btcnew::message_buffer *);
	// Return a buffer that has been filled with message data
	// Function will block until a buffer has been added
	// Return nullptr if the container has stopped
	btcnew::message_buffer * dequeue ();
	// Return a buffer to the freelist after is has been serviced
	void release (btcnew::message_buffer *);
	// Stop container and notify waiting threads
	void stop ();

private:
	btcnew::stat & stats;
	std::mutex mutex;
	btcnew::condition_variable condition;
	boost::circular_buffer<btcnew::message_buffer *> free;
	boost::circular_buffer<btcnew::message_buffer *> full;
	std::vector<uint8_t> slab;
	std::vector<btcnew::message_buffer> entries;
	bool stopped;
};
/**
  * Node ID cookies for node ID handshakes
*/
class syn_cookies final
{
public:
	void purge (std::chrono::steady_clock::time_point const &);
	// Returns boost::none if the IP is rate capped on syn cookie requests,
	// or if the endpoint already has a syn cookie query
	boost::optional<btcnew::uint256_union> assign (btcnew::endpoint const &);
	// Returns false if valid, true if invalid (true on error convention)
	// Also removes the syn cookie from the store if valid
	bool validate (btcnew::endpoint const &, btcnew::account const &, btcnew::signature const &);
	std::unique_ptr<seq_con_info_component> collect_seq_con_info (std::string const &);

private:
	class syn_cookie_info final
	{
	public:
		btcnew::uint256_union cookie;
		std::chrono::steady_clock::time_point created_at;
	};
	mutable std::mutex syn_cookie_mutex;
	std::unordered_map<btcnew::endpoint, syn_cookie_info> cookies;
	std::unordered_map<boost::asio::ip::address, unsigned> cookies_per_ip;
};
class network final
{
public:
	network (btcnew::node &, uint16_t);
	~network ();
	void start ();
	void stop ();
	void flood_message (btcnew::message const &, bool const = true);
	void flood_keepalive ()
	{
		btcnew::keepalive message;
		random_fill (message.peers);
		flood_message (message);
	}
	void flood_vote (std::shared_ptr<btcnew::vote> vote_a)
	{
		btcnew::confirm_ack message (vote_a);
		flood_message (message);
	}
	void flood_block (std::shared_ptr<btcnew::block> block_a, bool const is_droppable_a = true)
	{
		btcnew::publish publish (block_a);
		flood_message (publish, is_droppable_a);
	}

	void flood_block_many (std::deque<std::shared_ptr<btcnew::block>>, std::function<void ()> = nullptr, unsigned = broadcast_interval_ms);
	void merge_peers (std::array<btcnew::endpoint, 8> const &);
	void merge_peer (btcnew::endpoint const &);
	void send_keepalive (std::shared_ptr<btcnew::transport::channel>);
	void send_keepalive_self (std::shared_ptr<btcnew::transport::channel>);
	void send_node_id_handshake (std::shared_ptr<btcnew::transport::channel>, boost::optional<btcnew::uint256_union> const & query, boost::optional<btcnew::uint256_union> const & respond_to);
	void send_confirm_req (std::shared_ptr<btcnew::transport::channel>, std::shared_ptr<btcnew::block>);
	void broadcast_confirm_req (std::shared_ptr<btcnew::block>);
	void broadcast_confirm_req_base (std::shared_ptr<btcnew::block>, std::shared_ptr<std::vector<std::shared_ptr<btcnew::transport::channel>>>, unsigned, bool = false);
	void broadcast_confirm_req_batched_many (std::unordered_map<std::shared_ptr<btcnew::transport::channel>, std::deque<std::pair<btcnew::block_hash, btcnew::root>>>, std::function<void ()> = nullptr, unsigned = broadcast_interval_ms, bool = false);
	void broadcast_confirm_req_many (std::deque<std::pair<std::shared_ptr<btcnew::block>, std::shared_ptr<std::vector<std::shared_ptr<btcnew::transport::channel>>>>>, std::function<void ()> = nullptr, unsigned = broadcast_interval_ms);
	void confirm_hashes (btcnew::transaction const &, std::shared_ptr<btcnew::transport::channel>, std::vector<btcnew::block_hash>);
	bool send_votes_cache (std::shared_ptr<btcnew::transport::channel>, btcnew::block_hash const &);
	std::shared_ptr<btcnew::transport::channel> find_node_id (btcnew::account const &);
	std::shared_ptr<btcnew::transport::channel> find_channel (btcnew::endpoint const &);
	void process_message (btcnew::message const &, std::shared_ptr<btcnew::transport::channel>);
	bool not_a_peer (btcnew::endpoint const &, bool);
	// Should we reach out to this endpoint with a keepalive message
	bool reachout (btcnew::endpoint const &, bool = false);
	std::deque<std::shared_ptr<btcnew::transport::channel>> list (size_t);
	// A list of random peers sized for the configured rebroadcast fanout
	std::deque<std::shared_ptr<btcnew::transport::channel>> list_fanout ();
	void random_fill (std::array<btcnew::endpoint, 8> &) const;
	std::unordered_set<std::shared_ptr<btcnew::transport::channel>> random_set (size_t) const;
	// Get the next peer for attempting a tcp bootstrap connection
	btcnew::tcp_endpoint bootstrap_peer (bool = false);
	btcnew::endpoint endpoint ();
	void cleanup (std::chrono::steady_clock::time_point const &);
	void ongoing_cleanup ();
	// Node ID cookies cleanup
	btcnew::syn_cookies syn_cookies;
	void ongoing_syn_cookie_cleanup ();
	void ongoing_keepalive ();
	size_t size () const;
	size_t size_sqrt () const;
	bool empty () const;
	btcnew::message_buffer_manager buffer_container;
	boost::asio::ip::udp::resolver resolver;
	std::vector<boost::thread> packet_processing_threads;
	btcnew::node & node;
	btcnew::transport::udp_channels udp_channels;
	btcnew::transport::tcp_channels tcp_channels;
	std::function<void ()> disconnect_observer;
	// Called when a new channel is observed
	std::function<void (std::shared_ptr<btcnew::transport::channel>)> channel_observer;
	std::atomic<bool> stopped{ false };
	static unsigned const broadcast_interval_ms = 10;
	static size_t const buffer_size = 512;
	static size_t const confirm_req_hashes_max = 7;
};
}

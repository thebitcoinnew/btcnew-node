#pragma once

#include <btcnew/node/common.hpp>
#include <btcnew/node/socket.hpp>

namespace btcnew
{
class transaction;
class bootstrap_client;
class frontier_req_client final : public std::enable_shared_from_this<btcnew::frontier_req_client>
{
public:
	explicit frontier_req_client (std::shared_ptr<btcnew::bootstrap_client>);
	~frontier_req_client ();
	void run ();
	void receive_frontier ();
	void received_frontier (boost::system::error_code const &, size_t);
	void unsynced (btcnew::block_hash const &, btcnew::block_hash const &);
	void next (btcnew::transaction const &);
	std::shared_ptr<btcnew::bootstrap_client> connection;
	btcnew::account current;
	btcnew::block_hash frontier;
	unsigned count;
	btcnew::account landing;
	btcnew::account faucet;
	std::chrono::steady_clock::time_point start_time;
	std::promise<bool> promise;
	/** A very rough estimate of the cost of `bulk_push`ing missing blocks */
	uint64_t bulk_push_cost;
	std::deque<std::pair<btcnew::account, btcnew::block_hash>> accounts;
	static size_t constexpr size_frontier = sizeof (btcnew::account) + sizeof (btcnew::block_hash);
};
class bootstrap_server;
class frontier_req;
class frontier_req_server final : public std::enable_shared_from_this<btcnew::frontier_req_server>
{
public:
	frontier_req_server (std::shared_ptr<btcnew::bootstrap_server> const &, std::unique_ptr<btcnew::frontier_req>);
	void send_next ();
	void sent_action (boost::system::error_code const &, size_t);
	void send_finished ();
	void no_block_sent (boost::system::error_code const &, size_t);
	void next ();
	std::shared_ptr<btcnew::bootstrap_server> connection;
	btcnew::account current;
	btcnew::block_hash frontier;
	std::unique_ptr<btcnew::frontier_req> request;
	size_t count;
	std::deque<std::pair<btcnew::account, btcnew::block_hash>> accounts;
};
}
#pragma once

#include <btcnew/node/common.hpp>
#include <btcnew/node/socket.hpp>

namespace btcnew
{
class transaction;
class bootstrap_client;
class bulk_push_client final : public std::enable_shared_from_this<btcnew::bulk_push_client>
{
public:
	explicit bulk_push_client (std::shared_ptr<btcnew::bootstrap_client> const &);
	~bulk_push_client ();
	void start ();
	void push (btcnew::transaction const &);
	void push_block (btcnew::block const &);
	void send_finished ();
	std::shared_ptr<btcnew::bootstrap_client> connection;
	std::promise<bool> promise;
	std::pair<btcnew::block_hash, btcnew::block_hash> current_target;
};
class bootstrap_server;
class bulk_push_server final : public std::enable_shared_from_this<btcnew::bulk_push_server>
{
public:
	explicit bulk_push_server (std::shared_ptr<btcnew::bootstrap_server> const &);
	void throttled_receive ();
	void receive ();
	void received_type ();
	void received_block (boost::system::error_code const &, size_t, btcnew::block_type);
	std::shared_ptr<std::vector<uint8_t>> receive_buffer;
	std::shared_ptr<btcnew::bootstrap_server> connection;
};
}

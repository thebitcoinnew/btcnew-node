#pragma once

#include <btcnew/node/common.hpp>
#include <btcnew/node/socket.hpp>

#include <atomic>
#include <queue>

namespace btcnew
{
class bootstrap_server;
class bootstrap_listener final
{
public:
	bootstrap_listener (uint16_t, btcnew::node &);
	void start ();
	void stop ();
	void accept_action (boost::system::error_code const &, std::shared_ptr<btcnew::socket>);
	size_t connection_count ();

	std::mutex mutex;
	std::unordered_map<btcnew::bootstrap_server *, std::weak_ptr<btcnew::bootstrap_server>> connections;
	btcnew::tcp_endpoint endpoint ();
	btcnew::node & node;
	std::shared_ptr<btcnew::server_socket> listening_socket;
	bool on;
	std::atomic<size_t> bootstrap_count{ 0 };
	std::atomic<size_t> realtime_count{ 0 };

private:
	uint16_t port;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (bootstrap_listener & bootstrap_listener, const std::string & name);

class message;
enum class bootstrap_server_type
{
	undefined,
	bootstrap,
	realtime,
	realtime_response_server // special type for tcp channel response server
};
class bootstrap_server final : public std::enable_shared_from_this<btcnew::bootstrap_server>
{
public:
	bootstrap_server (std::shared_ptr<btcnew::socket>, std::shared_ptr<btcnew::node>);
	~bootstrap_server ();
	void stop ();
	void receive ();
	void receive_header_action (boost::system::error_code const &, size_t);
	void receive_bulk_pull_action (boost::system::error_code const &, size_t, btcnew::message_header const &);
	void receive_bulk_pull_account_action (boost::system::error_code const &, size_t, btcnew::message_header const &);
	void receive_frontier_req_action (boost::system::error_code const &, size_t, btcnew::message_header const &);
	void receive_keepalive_action (boost::system::error_code const &, size_t, btcnew::message_header const &);
	void receive_publish_action (boost::system::error_code const &, size_t, btcnew::message_header const &);
	void receive_confirm_req_action (boost::system::error_code const &, size_t, btcnew::message_header const &);
	void receive_confirm_ack_action (boost::system::error_code const &, size_t, btcnew::message_header const &);
	void receive_node_id_handshake_action (boost::system::error_code const &, size_t, btcnew::message_header const &);
	void add_request (std::unique_ptr<btcnew::message>);
	void finish_request ();
	void finish_request_async ();
	void timeout ();
	void run_next ();
	bool is_bootstrap_connection ();
	bool is_realtime_connection ();
	std::shared_ptr<std::vector<uint8_t>> receive_buffer;
	std::shared_ptr<btcnew::socket> socket;
	std::shared_ptr<btcnew::node> node;
	std::mutex mutex;
	std::queue<std::unique_ptr<btcnew::message>> requests;
	std::atomic<bool> stopped{ false };
	std::atomic<btcnew::bootstrap_server_type> type{ btcnew::bootstrap_server_type::undefined };
	// Remote enpoint used to remove response channel even after socket closing
	btcnew::tcp_endpoint remote_endpoint{ boost::asio::ip::address_v6::any (), 0 };
	btcnew::account remote_node_id{ 0 };
};
}

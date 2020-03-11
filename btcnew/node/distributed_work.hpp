#pragma once

#include <btcnew/boost/asio.hpp>
#include <btcnew/boost/beast.hpp>
#include <btcnew/lib/numbers.hpp>
#include <btcnew/lib/timer.hpp>

#include <boost/optional.hpp>

#include <unordered_map>

using request_type = boost::beast::http::request<boost::beast::http::string_body>;

namespace btcnew
{
class node;

class work_peer_request final
{
public:
	work_peer_request (boost::asio::io_context & io_ctx_a, boost::asio::ip::address address_a, uint16_t port_a) :
	address (address_a),
	port (port_a),
	socket (io_ctx_a)
	{
	}
	std::shared_ptr<request_type> get_prepared_json_request (std::string const &) const;
	boost::asio::ip::address address;
	uint16_t port;
	boost::beast::flat_buffer buffer;
	boost::beast::http::response<boost::beast::http::string_body> response;
	boost::asio::ip::tcp::socket socket;
};

/**
 * distributed_work cancels local and peer work requests when going out of scope
 */
class distributed_work final : public std::enable_shared_from_this<btcnew::distributed_work>
{
public:
	distributed_work (btcnew::node &, btcnew::root const &, std::vector<std::pair<std::string, uint16_t>> const & peers_a, unsigned int, std::function<void (boost::optional<uint64_t>)> const &, uint64_t, boost::optional<btcnew::account> const & = boost::none);
	~distributed_work ();
	void start ();
	void start_work ();
	void cancel_connection (std::shared_ptr<btcnew::work_peer_request>);
	void success (std::string const &, boost::asio::ip::address const &, uint16_t const);
	void stop_once (bool const);
	void set_once (uint64_t, std::string const & source_a = "local");
	void cancel_once ();
	void failure (boost::asio::ip::address const &);
	void handle_failure (bool const);
	bool remove (boost::asio::ip::address const &);
	void add_bad_peer (boost::asio::ip::address const &, uint16_t const);

	std::function<void (boost::optional<uint64_t>)> callback;
	unsigned int backoff; // in seconds
	btcnew::node & node;
	btcnew::root root;
	boost::optional<btcnew::account> const account;
	std::mutex mutex;
	std::map<boost::asio::ip::address, uint16_t> outstanding;
	std::vector<std::weak_ptr<btcnew::work_peer_request>> connections;
	std::vector<std::pair<std::string, uint16_t>> const peers;
	std::vector<std::pair<std::string, uint16_t>> need_resolve;
	uint64_t difficulty;
	uint64_t work_result{ 0 };
	std::atomic<bool> completed{ false };
	std::atomic<bool> cancelled{ false };
	std::atomic<bool> stopped{ false };
	std::atomic<bool> local_generation_started{ false };
	btcnew::timer<std::chrono::milliseconds> elapsed; // logging only
	std::vector<std::string> bad_peers; // websocket
	std::string winner; // websocket
};

class distributed_work_factory final
{
public:
	distributed_work_factory (btcnew::node &);
	~distributed_work_factory ();
	bool make (btcnew::root const &, std::vector<std::pair<std::string, uint16_t>> const &, std::function<void (boost::optional<uint64_t>)> const &, uint64_t, boost::optional<btcnew::account> const & = boost::none);
	bool make (unsigned int, btcnew::root const &, std::vector<std::pair<std::string, uint16_t>> const &, std::function<void (boost::optional<uint64_t>)> const &, uint64_t, boost::optional<btcnew::account> const & = boost::none);
	void cancel (btcnew::root const &, bool const local_stop = false);
	void cleanup_finished ();
	void stop ();

	btcnew::node & node;
	std::unordered_map<btcnew::root, std::vector<std::weak_ptr<btcnew::distributed_work>>> items;
	std::mutex mutex;
	std::atomic<bool> stopped{ false };
};

class seq_con_info_component;
std::unique_ptr<seq_con_info_component> collect_seq_con_info (distributed_work_factory & distributed_work, const std::string & name);
}
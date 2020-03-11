#pragma once

#include <btcnew/rpc/rpc_handler.hpp>

#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>

#include <atomic>

/* Boost v1.70 introduced breaking changes; the conditional compilation allows 1.6x to be supported as well. */
#if BOOST_VERSION < 107000
using socket_type = boost::asio::ip::tcp::socket;
#else
using socket_type = boost::asio::basic_stream_socket<boost::asio::ip::tcp, boost::asio::io_context::executor_type>;
#endif

namespace btcnew
{
class logger_mt;
class rpc_config;
class rpc_handler_interface;

class rpc_connection : public std::enable_shared_from_this<btcnew::rpc_connection>
{
public:
	rpc_connection (btcnew::rpc_config const & rpc_config, boost::asio::io_context & io_ctx, btcnew::logger_mt & logger, btcnew::rpc_handler_interface & rpc_handler_interface_a);
	virtual ~rpc_connection () = default;
	virtual void parse_connection ();
	virtual void write_completion_handler (std::shared_ptr<btcnew::rpc_connection> rpc_connection);
	void prepare_head (unsigned version, boost::beast::http::status status = boost::beast::http::status::ok);
	void write_result (std::string body, unsigned version, boost::beast::http::status status = boost::beast::http::status::ok);
	void parse_request (std::shared_ptr<boost::beast::http::request_parser<boost::beast::http::empty_body>> header_parser);

	void read ();

	socket_type socket;
	boost::beast::flat_buffer buffer;
	boost::beast::http::response<boost::beast::http::string_body> res;
	boost::asio::strand<boost::asio::io_context::executor_type> strand;
	std::atomic_flag responded;
	boost::asio::io_context & io_ctx;
	btcnew::logger_mt & logger;
	btcnew::rpc_config const & rpc_config;
	btcnew::rpc_handler_interface & rpc_handler_interface;
};
}

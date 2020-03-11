#pragma once

#include <btcnew/boost/asio.hpp>
#include <btcnew/lib/logger_mt.hpp>
#include <btcnew/lib/rpc_handler_interface.hpp>
#include <btcnew/lib/rpcconfig.hpp>

namespace btcnew
{
class rpc_handler_interface;

class rpc
{
public:
	rpc (boost::asio::io_context & io_ctx_a, btcnew::rpc_config const & config_a, btcnew::rpc_handler_interface & rpc_handler_interface_a);
	virtual ~rpc ();
	void start ();
	virtual void accept ();
	void stop ();

	btcnew::rpc_config config;
	boost::asio::ip::tcp::acceptor acceptor;
	btcnew::logger_mt logger;
	boost::asio::io_context & io_ctx;
	btcnew::rpc_handler_interface & rpc_handler_interface;
	bool stopped{ false };
};

/** Returns the correct RPC implementation based on TLS configuration */
std::unique_ptr<btcnew::rpc> get_rpc (boost::asio::io_context & io_ctx_a, btcnew::rpc_config const & config_a, btcnew::rpc_handler_interface & rpc_handler_interface_a);
}

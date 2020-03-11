#pragma once

#include <btcnew/rpc/rpc_connection.hpp>

#include <boost/asio/ssl/stream.hpp>

namespace btcnew
{
/**
 * Specialization of btcnew::rpc_connection for establishing TLS connections.
 * Handshakes with client certificates are supported.
 */
class rpc_connection_secure : public rpc_connection
{
public:
	rpc_connection_secure (btcnew::rpc_config const & rpc_config, boost::asio::io_context & io_ctx, btcnew::logger_mt & logger, btcnew::rpc_handler_interface & rpc_handler_interface_a, boost::asio::ssl::context & ssl_context);
	void parse_connection () override;
	void write_completion_handler (std::shared_ptr<btcnew::rpc_connection> rpc) override;
	/** The TLS handshake callback */
	void handle_handshake (const boost::system::error_code & error);
	/** The TLS async shutdown callback */
	void on_shutdown (const boost::system::error_code & error);

private:
	boost::asio::ssl::stream<socket_type &> stream;
};
}

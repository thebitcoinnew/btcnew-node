#include <btcnew/rpc/rpc_connection_secure.hpp>
#include <btcnew/rpc/rpc_secure.hpp>

#include <boost/polymorphic_pointer_cast.hpp>

btcnew::rpc_connection_secure::rpc_connection_secure (btcnew::rpc_config const & rpc_config, boost::asio::io_context & io_ctx, btcnew::logger_mt & logger, btcnew::rpc_handler_interface & rpc_handler_interface, boost::asio::ssl::context & ssl_context) :
btcnew::rpc_connection (rpc_config, io_ctx, logger, rpc_handler_interface),
stream (socket, ssl_context)
{
}

void btcnew::rpc_connection_secure::parse_connection ()
{
	// Perform the SSL handshake
	auto this_l = std::static_pointer_cast<btcnew::rpc_connection_secure> (shared_from_this ());
	stream.async_handshake (boost::asio::ssl::stream_base::server,
	[this_l] (auto & ec) {
		this_l->handle_handshake (ec);
	});
}

void btcnew::rpc_connection_secure::on_shutdown (const boost::system::error_code & error)
{
	// No-op. We initiate the shutdown (since the RPC server kills the connection after each request)
	// and we'll thus get an expected EOF error. If the client disconnects, a short-read error will be expected.
}

void btcnew::rpc_connection_secure::handle_handshake (const boost::system::error_code & error)
{
	if (!error)
	{
		read ();
	}
	else
	{
		logger.always_log ("TLS: Handshake error: ", error.message ());
	}
}

void btcnew::rpc_connection_secure::write_completion_handler (std::shared_ptr<btcnew::rpc_connection> rpc)
{
	auto rpc_connection_secure = boost::polymorphic_pointer_downcast<btcnew::rpc_connection_secure> (rpc);
	rpc_connection_secure->stream.async_shutdown (boost::asio::bind_executor (rpc->strand, [rpc_connection_secure] (auto const & ec_shutdown) {
		rpc_connection_secure->on_shutdown (ec_shutdown);
	}));
}

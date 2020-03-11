#pragma once

#include <btcnew/boost/asio.hpp>
#include <btcnew/lib/errors.hpp>
#include <btcnew/lib/ipc.hpp>
#include <btcnew/lib/utility.hpp>

#include <boost/property_tree/ptree.hpp>

#include <atomic>
#include <string>
#include <vector>

namespace btcnew
{
class shared_const_buffer;
namespace ipc
{
	class ipc_client_impl
	{
	public:
		virtual ~ipc_client_impl () = default;
	};

	/** IPC client */
	class ipc_client
	{
	public:
		ipc_client (boost::asio::io_context & io_ctx_a);
		ipc_client (ipc_client && ipc_client) = default;
		virtual ~ipc_client () = default;

		/** Connect to a domain socket */
		btcnew::error connect (std::string const & path);

		/** Connect to a tcp socket synchronously */
		btcnew::error connect (std::string const & host, uint16_t port);

		/** Connect to a tcp socket asynchronously */
		void async_connect (std::string const & host, uint16_t port, std::function<void (btcnew::error)> callback);

		/** Write buffer asynchronously */
		void async_write (btcnew::shared_const_buffer const & buffer_a, std::function<void (btcnew::error, size_t)> callback_a);

		/** Read \p size_a bytes asynchronously */
		void async_read (std::shared_ptr<std::vector<uint8_t>> buffer_a, size_t size_a, std::function<void (btcnew::error, size_t)> callback_a);

	private:
		boost::asio::io_context & io_ctx;

		// PIMPL pattern to hide implementation details
		std::unique_ptr<ipc_client_impl> impl;
	};

	/** Convenience function for making synchronous IPC calls. The client must be connected */
	std::string request (btcnew::ipc::ipc_client & ipc_client, std::string const & rpc_action_a);

	/**
  	 * Returns a buffer with an IPC preamble for the given \p encoding_a followed by the payload. Depending on encoding,
	 * the buffer may contain a payload length or end sentinel.
	 */
	btcnew::shared_const_buffer prepare_request (btcnew::ipc::payload_encoding encoding_a, std::string const & payload_a);
}
}

#pragma once

#include <btcnew/lib/ipc.hpp>
#include <btcnew/lib/numbers.hpp>
#include <btcnew/node/node_rpc_config.hpp>

#include <atomic>

namespace btcnew
{
class node;

namespace ipc
{
	/** The IPC server accepts connections on one or more configured transports */
	class ipc_server
	{
	public:
		ipc_server (btcnew::node & node_a, btcnew::node_rpc_config const & node_rpc_config);

		virtual ~ipc_server ();
		void stop ();

		btcnew::node & node;
		btcnew::node_rpc_config const & node_rpc_config;

		/** Unique counter/id shared across sessions */
		std::atomic<uint64_t> id_dispenser{ 0 };

	private:
		std::unique_ptr<dsock_file_remover> file_remover;
		std::vector<std::shared_ptr<btcnew::ipc::transport>> transports;
	};
}
}

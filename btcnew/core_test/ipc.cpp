#include <btcnew/core_test/testutil.hpp>
#include <btcnew/lib/ipc_client.hpp>
#include <btcnew/node/ipc.hpp>
#include <btcnew/node/testing.hpp>
#include <btcnew/rpc/rpc.hpp>

#include <gtest/gtest.h>

#include <boost/property_tree/json_parser.hpp>

#include <chrono>
#include <memory>
#include <sstream>
#include <vector>

using namespace std::chrono_literals;

TEST (ipc, asynchronous)
{
	btcnew::system system (24000, 1);
	system.nodes[0]->config.ipc_config.transport_tcp.enabled = true;
	system.nodes[0]->config.ipc_config.transport_tcp.port = 24077;
	btcnew::node_rpc_config node_rpc_config;
	btcnew::ipc::ipc_server ipc (*system.nodes[0], node_rpc_config);
	btcnew::ipc::ipc_client client (system.nodes[0]->io_ctx);

	auto req (btcnew::ipc::prepare_request (btcnew::ipc::payload_encoding::json_legacy, std::string (R"({"action": "block_count"})")));
	auto res (std::make_shared<std::vector<uint8_t>> ());
	std::atomic<bool> call_completed{ false };
	client.async_connect ("::1", 24077, [&client, &req, &res, &call_completed] (btcnew::error err) {
		client.async_write (req, [&client, &req, &res, &call_completed] (btcnew::error err_a, size_t size_a) {
			ASSERT_NO_ERROR (static_cast<std::error_code> (err_a));
			ASSERT_EQ (size_a, req.size ());
			// Read length
			client.async_read (res, sizeof (uint32_t), [&client, &res, &call_completed] (btcnew::error err_read_a, size_t size_read_a) {
				ASSERT_NO_ERROR (static_cast<std::error_code> (err_read_a));
				ASSERT_EQ (size_read_a, sizeof (uint32_t));
				uint32_t payload_size_l = boost::endian::big_to_native (*reinterpret_cast<uint32_t *> (res->data ()));
				// Read json payload
				client.async_read (res, payload_size_l, [&res, &call_completed] (btcnew::error err_read_a, size_t size_read_a) {
					std::string payload (res->begin (), res->end ());
					std::stringstream ss;
					ss << payload;

					// Make sure the response is valid json
					boost::property_tree::ptree blocks;
					boost::property_tree::read_json (ss, blocks);
					ASSERT_EQ (blocks.get<int> ("count"), 1);
					call_completed = true;
				});
			});
		});
	});
	system.deadline_set (5s);
	while (!call_completed)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (ipc, synchronous)
{
	btcnew::system system (24000, 1);
	system.nodes[0]->config.ipc_config.transport_tcp.enabled = true;
	system.nodes[0]->config.ipc_config.transport_tcp.port = 24077;
	btcnew::node_rpc_config node_rpc_config;
	btcnew::ipc::ipc_server ipc (*system.nodes[0], node_rpc_config);
	btcnew::ipc::ipc_client client (system.nodes[0]->io_ctx);

	// Start blocking IPC client in a separate thread
	std::atomic<bool> call_completed{ false };
	std::thread client_thread ([&client, &call_completed] () {
		client.connect ("::1", 24077);
		std::string response (btcnew::ipc::request (client, std::string (R"({"action": "block_count"})")));
		std::stringstream ss;
		ss << response;
		// Make sure the response is valid json
		boost::property_tree::ptree blocks;
		boost::property_tree::read_json (ss, blocks);
		ASSERT_EQ (blocks.get<int> ("count"), 1);

		call_completed = true;
	});
	client_thread.detach ();

	system.deadline_set (5s);
	while (!call_completed)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (ipc, config_upgrade_v0_v1)
{
	auto path1 (btcnew::unique_path ());
	auto path2 (btcnew::unique_path ());
	btcnew::ipc::ipc_config config1;
	btcnew::ipc::ipc_config config2;
	btcnew::jsonconfig tree;
	config1.serialize_json (tree);
	btcnew::jsonconfig local = tree.get_required_child ("local");
	local.erase ("version");
	local.erase ("allow_unsafe");
	bool upgraded (false);
	ASSERT_FALSE (config2.deserialize_json (upgraded, tree));
	btcnew::jsonconfig local2 = tree.get_required_child ("local");
	ASSERT_TRUE (upgraded);
	ASSERT_LE (1, local2.get<int> ("version"));
	ASSERT_FALSE (local2.get<bool> ("allow_unsafe"));
}

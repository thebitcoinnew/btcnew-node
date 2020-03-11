#include <btcnew/core_test/testutil.hpp>
#include <btcnew/lib/jsonconfig.hpp>
#include <btcnew/node/testing.hpp>
#include <btcnew/node/transport/udp.hpp>
#include <btcnew/secure/working.hpp>

#include <gtest/gtest.h>

#include <boost/make_shared.hpp>
#include <boost/polymorphic_cast.hpp>

#include <numeric>

using namespace std::chrono_literals;

namespace
{
void add_required_children_node_config_tree (btcnew::jsonconfig & tree);
}

TEST (node, stop)
{
	btcnew::system system (24000, 1);
	ASSERT_NE (system.nodes[0]->wallets.items.end (), system.nodes[0]->wallets.items.begin ());
	system.nodes[0]->stop ();
	system.io_ctx.run ();
	ASSERT_TRUE (true);
}

TEST (node, block_store_path_failure)
{
	auto service (boost::make_shared<boost::asio::io_context> ());
	btcnew::alarm alarm (*service);
	auto path (btcnew::unique_path ());
	btcnew::logging logging;
	logging.init (path);
	btcnew::work_pool work (std::numeric_limits<unsigned>::max ());
	auto node (std::make_shared<btcnew::node> (*service, 24000, path, alarm, logging, work));
	ASSERT_TRUE (node->wallets.items.empty ());
	node->stop ();
}

TEST (node, password_fanout)
{
	auto service (boost::make_shared<boost::asio::io_context> ());
	btcnew::alarm alarm (*service);
	auto path (btcnew::unique_path ());
	btcnew::node_config config;
	config.peering_port = 24000;
	config.logging.init (path);
	btcnew::work_pool work (std::numeric_limits<unsigned>::max ());
	config.password_fanout = 10;
	auto node (std::make_shared<btcnew::node> (*service, path, alarm, config, work));
	auto wallet (node->wallets.create (100));
	ASSERT_EQ (10, wallet->store.password.values.size ());
	node->stop ();
}

TEST (node, balance)
{
	btcnew::system system (24000, 1);
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	auto transaction (system.nodes[0]->store.tx_begin_write ());
	ASSERT_EQ (std::numeric_limits<btcnew::uint128_t>::max (), system.nodes[0]->ledger.account_balance (transaction, btcnew::test_genesis_key.pub));
}

TEST (node, representative)
{
	btcnew::system system (24000, 1);
	auto block1 (system.nodes[0]->rep_block (btcnew::test_genesis_key.pub));
	{
		auto transaction (system.nodes[0]->store.tx_begin_read ());
		ASSERT_TRUE (system.nodes[0]->ledger.store.block_exists (transaction, block1));
	}
	btcnew::keypair key;
	ASSERT_TRUE (system.nodes[0]->rep_block (key.pub).is_zero ());
}

TEST (node, send_unkeyed)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key2;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	system.wallet (0)->store.password.value_set (btcnew::keypair ().prv);
	ASSERT_EQ (nullptr, system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
}

TEST (node, send_self)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key2;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key2.prv);
	ASSERT_NE (nullptr, system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	system.deadline_set (10s);
	while (system.nodes[0]->balance (key2.pub).is_zero ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_EQ (std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number (), system.nodes[0]->balance (btcnew::test_genesis_key.pub));
}

TEST (node, send_single)
{
	btcnew::system system (24000, 2);
	btcnew::keypair key2;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	system.wallet (1)->insert_adhoc (key2.prv);
	ASSERT_NE (nullptr, system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	ASSERT_EQ (std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number (), system.nodes[0]->balance (btcnew::test_genesis_key.pub));
	ASSERT_TRUE (system.nodes[0]->balance (key2.pub).is_zero ());
	system.deadline_set (10s);
	while (system.nodes[0]->balance (key2.pub).is_zero ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (node, send_single_observing_peer)
{
	btcnew::system system (24000, 3);
	btcnew::keypair key2;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	system.wallet (1)->insert_adhoc (key2.prv);
	ASSERT_NE (nullptr, system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	ASSERT_EQ (std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number (), system.nodes[0]->balance (btcnew::test_genesis_key.pub));
	ASSERT_TRUE (system.nodes[0]->balance (key2.pub).is_zero ());
	system.deadline_set (10s);
	while (std::any_of (system.nodes.begin (), system.nodes.end (), [&] (std::shared_ptr<btcnew::node> const & node_a) { return node_a->balance (key2.pub).is_zero (); }))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (node, send_single_many_peers)
{
	btcnew::system system (24000, 10);
	btcnew::keypair key2;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	system.wallet (1)->insert_adhoc (key2.prv);
	ASSERT_NE (nullptr, system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	ASSERT_EQ (std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number (), system.nodes[0]->balance (btcnew::test_genesis_key.pub));
	ASSERT_TRUE (system.nodes[0]->balance (key2.pub).is_zero ());
	system.deadline_set (3.5min);
	while (std::any_of (system.nodes.begin (), system.nodes.end (), [&] (std::shared_ptr<btcnew::node> const & node_a) { return node_a->balance (key2.pub).is_zero (); }))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	system.stop ();
	for (auto node : system.nodes)
	{
		ASSERT_TRUE (node->stopped);
		ASSERT_TRUE (node->network.tcp_channels.node_id_handhake_sockets_empty ());
	}
}

TEST (node, send_out_of_order)
{
	btcnew::system system (24000, 2);
	btcnew::keypair key2;
	btcnew::genesis genesis;
	btcnew::send_block send1 (genesis.hash (), key2.pub, std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number (), btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ()));
	btcnew::send_block send2 (send1.hash (), key2.pub, std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number () * 2, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (send1.hash ()));
	btcnew::send_block send3 (send2.hash (), key2.pub, std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number () * 3, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (send2.hash ()));
	system.nodes[0]->process_active (std::make_shared<btcnew::send_block> (send3));
	system.nodes[0]->process_active (std::make_shared<btcnew::send_block> (send2));
	system.nodes[0]->process_active (std::make_shared<btcnew::send_block> (send1));
	system.deadline_set (10s);
	while (std::any_of (system.nodes.begin (), system.nodes.end (), [&] (std::shared_ptr<btcnew::node> const & node_a) { return node_a->balance (btcnew::test_genesis_key.pub) != btcnew::genesis_amount - system.nodes[0]->config.receive_minimum.number () * 3; }))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (node, quick_confirm)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key;
	btcnew::block_hash previous (system.nodes[0]->latest (btcnew::test_genesis_key.pub));
	auto genesis_start_balance (system.nodes[0]->balance (btcnew::test_genesis_key.pub));
	system.wallet (0)->insert_adhoc (key.prv);
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	auto send (std::make_shared<btcnew::send_block> (previous, key.pub, system.nodes[0]->config.online_weight_minimum.number () + 1, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (previous)));
	system.nodes[0]->process_active (send);
	system.deadline_set (10s);
	while (system.nodes[0]->balance (key.pub).is_zero ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_EQ (system.nodes[0]->balance (btcnew::test_genesis_key.pub), system.nodes[0]->config.online_weight_minimum.number () + 1);
	ASSERT_EQ (system.nodes[0]->balance (key.pub), genesis_start_balance - (system.nodes[0]->config.online_weight_minimum.number () + 1));
}

TEST (node, node_receive_quorum)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key;
	btcnew::block_hash previous (system.nodes[0]->latest (btcnew::test_genesis_key.pub));
	system.wallet (0)->insert_adhoc (key.prv);
	auto send (std::make_shared<btcnew::send_block> (previous, key.pub, btcnew::genesis_amount - btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (previous)));
	system.nodes[0]->process_active (send);
	system.deadline_set (10s);
	while (!system.nodes[0]->ledger.block_exists (send->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	auto done (false);
	while (!done)
	{
		{
			btcnew::lock_guard<std::mutex> guard (system.nodes[0]->active.mutex);
			auto info (system.nodes[0]->active.roots.find (btcnew::qualified_root (previous, previous)));
			ASSERT_NE (system.nodes[0]->active.roots.end (), info);
			done = info->election->confirmation_request_count > 2;
		}
		ASSERT_NO_ERROR (system.poll ());
	}
	btcnew::system system2 (24001, 1);
	system2.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	ASSERT_TRUE (system.nodes[0]->balance (key.pub).is_zero ());
	auto channel (std::make_shared<btcnew::transport::channel_udp> (system.nodes[0]->network.udp_channels, system2.nodes[0]->network.endpoint (), system.nodes[0]->network_params.protocol.protocol_version));
	system.nodes[0]->network.send_keepalive (channel);
	while (system.nodes[0]->balance (key.pub).is_zero ())
	{
		ASSERT_NO_ERROR (system.poll ());
		ASSERT_NO_ERROR (system2.poll ());
	}
}

TEST (node, auto_bootstrap)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key2;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key2.prv);
	auto send1 (system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	ASSERT_NE (nullptr, send1);
	system.deadline_set (10s);
	while (system.nodes[0]->balance (key2.pub) != system.nodes[0]->config.receive_minimum.number ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	auto node1 (std::make_shared<btcnew::node> (system.io_ctx, 24001, btcnew::unique_path (), system.alarm, system.logging, system.work));
	ASSERT_FALSE (node1->init_error ());
	auto channel (std::make_shared<btcnew::transport::channel_udp> (node1->network.udp_channels, system.nodes[0]->network.endpoint (), node1->network_params.protocol.protocol_version));
	node1->network.send_keepalive (channel);
	node1->start ();
	system.nodes.push_back (node1);
	system.deadline_set (10s);
	while (!node1->bootstrap_initiator.in_progress ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	system.deadline_set (10s);
	while (node1->balance (key2.pub) != system.nodes[0]->config.receive_minimum.number ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	system.deadline_set (10s);
	while (node1->bootstrap_initiator.in_progress ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_TRUE (node1->ledger.block_exists (send1->hash ()));
	// Wait block receive
	system.deadline_set (5s);
	while (node1->ledger.block_count_cache < 3)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	// Confirmation for all blocks
	system.deadline_set (5s);
	while (node1->ledger.cemented_count < 3)
	{
		ASSERT_NO_ERROR (system.poll ());
	}

	node1->stop ();
}

TEST (node, auto_bootstrap_reverse)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key2;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key2.prv);
	auto node1 (std::make_shared<btcnew::node> (system.io_ctx, 24001, btcnew::unique_path (), system.alarm, system.logging, system.work));
	ASSERT_FALSE (node1->init_error ());
	ASSERT_NE (nullptr, system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	auto channel (std::make_shared<btcnew::transport::channel_udp> (system.nodes[0]->network.udp_channels, node1->network.endpoint (), system.nodes[0]->network_params.protocol.protocol_version));
	system.nodes[0]->network.send_keepalive (channel);
	node1->start ();
	system.nodes.push_back (node1);
	system.deadline_set (10s);
	while (node1->balance (key2.pub) != system.nodes[0]->config.receive_minimum.number ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	node1->stop ();
}

TEST (node, receive_gap)
{
	btcnew::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	ASSERT_EQ (0, node1.gap_cache.size ());
	auto block (std::make_shared<btcnew::send_block> (5, 1, 2, btcnew::keypair ().prv, 4, 0));
	node1.work_generate_blocking (*block);
	btcnew::publish message (block);
	node1.network.process_message (message, node1.network.udp_channels.create (node1.network.endpoint ()));
	node1.block_processor.flush ();
	ASSERT_EQ (1, node1.gap_cache.size ());
}

TEST (node, merge_peers)
{
	btcnew::system system (24000, 1);
	std::array<btcnew::endpoint, 8> endpoints;
	endpoints.fill (btcnew::endpoint (boost::asio::ip::address_v6::loopback (), 24000));
	endpoints[0] = btcnew::endpoint (boost::asio::ip::address_v6::loopback (), 24001);
	system.nodes[0]->network.merge_peers (endpoints);
	ASSERT_EQ (0, system.nodes[0]->network.size ());
}

TEST (node, search_pending)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key2;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	ASSERT_NE (nullptr, system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	system.wallet (0)->insert_adhoc (key2.prv);
	auto node (system.nodes[0]);
	ASSERT_FALSE (system.wallet (0)->search_pending ());
	system.deadline_set (10s);
	while (system.nodes[0]->balance (key2.pub).is_zero ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (node, search_pending_same)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key2;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	ASSERT_NE (nullptr, system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	ASSERT_NE (nullptr, system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	system.wallet (0)->insert_adhoc (key2.prv);
	auto node (system.nodes[0]);
	ASSERT_FALSE (system.wallet (0)->search_pending ());
	system.deadline_set (10s);
	while (system.nodes[0]->balance (key2.pub) != 2 * system.nodes[0]->config.receive_minimum.number ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (node, search_pending_multiple)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key2;
	btcnew::keypair key3;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key3.prv);
	ASSERT_NE (nullptr, system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key3.pub, system.nodes[0]->config.receive_minimum.number ()));
	system.deadline_set (10s);
	while (system.nodes[0]->balance (key3.pub).is_zero ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_NE (nullptr, system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	ASSERT_NE (nullptr, system.wallet (0)->send_action (key3.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	system.wallet (0)->insert_adhoc (key2.prv);
	auto node (system.nodes[0]);
	ASSERT_FALSE (system.wallet (0)->search_pending ());
	system.deadline_set (10s);
	while (system.nodes[0]->balance (key2.pub) != 2 * system.nodes[0]->config.receive_minimum.number ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (node, search_pending_confirmed)
{
	btcnew::system system;
	btcnew::node_config node_config (24000, system.logging);
	node_config.frontiers_confirmation = btcnew::frontiers_confirmation_mode::disabled;
	auto node = system.add_node (node_config);
	btcnew::keypair key2;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	auto send1 (system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, node->config.receive_minimum.number ()));
	ASSERT_NE (nullptr, send1);
	auto send2 (system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, node->config.receive_minimum.number ()));
	ASSERT_NE (nullptr, send2);
	system.deadline_set (10s);
	while (!node->active.empty ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	bool confirmed (false);
	system.deadline_set (5s);
	while (!confirmed)
	{
		auto transaction (node->store.tx_begin_read ());
		confirmed = node->ledger.block_confirmed (transaction, send2->hash ());
		ASSERT_NO_ERROR (system.poll ());
	}
	{
		auto transaction (node->wallets.tx_begin_write ());
		system.wallet (0)->store.erase (transaction, btcnew::test_genesis_key.pub);
	}
	system.wallet (0)->insert_adhoc (key2.prv);
	ASSERT_FALSE (system.wallet (0)->search_pending ());
	{
		btcnew::lock_guard<std::mutex> guard (node->active.mutex);
		auto existing1 (node->active.blocks.find (send1->hash ()));
		ASSERT_EQ (node->active.blocks.end (), existing1);
		auto existing2 (node->active.blocks.find (send2->hash ()));
		ASSERT_EQ (node->active.blocks.end (), existing2);
	}
	system.deadline_set (10s);
	while (node->balance (key2.pub) != 2 * node->config.receive_minimum.number ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (node, unlock_search)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key2;
	btcnew::uint128_t balance (system.nodes[0]->balance (btcnew::test_genesis_key.pub));
	{
		auto transaction (system.wallet (0)->wallets.tx_begin_write ());
		system.wallet (0)->store.rekey (transaction, "");
	}
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	ASSERT_NE (nullptr, system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	system.deadline_set (10s);
	while (system.nodes[0]->balance (btcnew::test_genesis_key.pub) == balance)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	while (!system.nodes[0]->active.empty ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	system.wallet (0)->insert_adhoc (key2.prv);
	{
		btcnew::lock_guard<std::recursive_mutex> lock (system.wallet (0)->store.mutex);
		system.wallet (0)->store.password.value_set (btcnew::keypair ().prv);
	}
	auto node (system.nodes[0]);
	{
		auto transaction (system.wallet (0)->wallets.tx_begin_write ());
		ASSERT_FALSE (system.wallet (0)->enter_password (transaction, ""));
	}
	system.deadline_set (10s);
	while (system.nodes[0]->balance (key2.pub).is_zero ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (node, connect_after_junk)
{
	btcnew::system system (24000, 1);
	auto node1 (std::make_shared<btcnew::node> (system.io_ctx, 24001, btcnew::unique_path (), system.alarm, system.logging, system.work));
	std::vector<uint8_t> junk_buffer;
	junk_buffer.push_back (0);
	auto channel1 (std::make_shared<btcnew::transport::channel_udp> (node1->network.udp_channels, system.nodes[0]->network.endpoint (), node1->network_params.protocol.protocol_version));
	channel1->send_buffer (btcnew::shared_const_buffer (std::move (junk_buffer)), btcnew::stat::detail::bulk_pull, [] (boost::system::error_code const &, size_t) {});
	system.deadline_set (10s);
	while (system.nodes[0]->stats.count (btcnew::stat::type::error) == 0)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	node1->start ();
	system.nodes.push_back (node1);
	auto channel2 (std::make_shared<btcnew::transport::channel_udp> (node1->network.udp_channels, system.nodes[0]->network.endpoint (), node1->network_params.protocol.protocol_version));
	node1->network.send_keepalive (channel2);
	system.deadline_set (10s);
	while (node1->network.empty ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	node1->stop ();
}

TEST (node, working)
{
	auto path (btcnew::working_path ());
	ASSERT_FALSE (path.empty ());
}

TEST (node, price)
{
	btcnew::system system (24000, 1);
	auto price1 (system.nodes[0]->price (btcnew::Gbtcnew_ratio, 1));
	ASSERT_EQ (btcnew::node::price_max * 100.0, price1);
	auto price2 (system.nodes[0]->price (btcnew::Gbtcnew_ratio * int (btcnew::node::free_cutoff + 1), 1));
	ASSERT_EQ (0, price2);
	auto price3 (system.nodes[0]->price (btcnew::Gbtcnew_ratio * int (btcnew::node::free_cutoff + 2) / 2, 1));
	ASSERT_EQ (btcnew::node::price_max * 100.0 / 2, price3);
	auto price4 (system.nodes[0]->price (btcnew::Gbtcnew_ratio * int (btcnew::node::free_cutoff) * 2, 1));
	ASSERT_EQ (0, price4);
}

TEST (node, confirm_locked)
{
	btcnew::system system (24000, 1);
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	auto transaction (system.wallet (0)->wallets.tx_begin_read ());
	system.wallet (0)->enter_password (transaction, "1");
	auto block (std::make_shared<btcnew::send_block> (0, 0, 0, btcnew::keypair ().prv, 0, 0));
	system.nodes[0]->network.flood_block (block);
}

TEST (node_config, serialization)
{
	auto path (btcnew::unique_path ());
	btcnew::logging logging1;
	logging1.init (path);
	btcnew::node_config config1 (100, logging1);
	config1.bootstrap_fraction_numerator = 10;
	config1.receive_minimum = 10;
	config1.online_weight_minimum = 10;
	config1.online_weight_quorum = 10;
	config1.password_fanout = 20;
	config1.enable_voting = false;
	config1.callback_address = "test";
	config1.callback_port = 10;
	config1.callback_target = "test";
	config1.lmdb_max_dbs = 256;
	btcnew::jsonconfig tree;
	config1.serialize_json (tree);
	btcnew::logging logging2;
	logging2.init (path);
	logging2.node_lifetime_tracing_value = !logging2.node_lifetime_tracing_value;
	btcnew::node_config config2 (50, logging2);
	ASSERT_NE (config2.bootstrap_fraction_numerator, config1.bootstrap_fraction_numerator);
	ASSERT_NE (config2.peering_port, config1.peering_port);
	ASSERT_NE (config2.logging.node_lifetime_tracing_value, config1.logging.node_lifetime_tracing_value);
	ASSERT_NE (config2.online_weight_minimum, config1.online_weight_minimum);
	ASSERT_NE (config2.online_weight_quorum, config1.online_weight_quorum);
	ASSERT_NE (config2.password_fanout, config1.password_fanout);
	ASSERT_NE (config2.enable_voting, config1.enable_voting);
	ASSERT_NE (config2.callback_address, config1.callback_address);
	ASSERT_NE (config2.callback_port, config1.callback_port);
	ASSERT_NE (config2.callback_target, config1.callback_target);
	ASSERT_NE (config2.lmdb_max_dbs, config1.lmdb_max_dbs);

	ASSERT_FALSE (tree.get_optional<std::string> ("epoch_block_link"));
	ASSERT_FALSE (tree.get_optional<std::string> ("epoch_block_signer"));

	bool upgraded (false);
	ASSERT_FALSE (config2.deserialize_json (upgraded, tree));
	ASSERT_FALSE (upgraded);
	ASSERT_EQ (config2.bootstrap_fraction_numerator, config1.bootstrap_fraction_numerator);
	ASSERT_EQ (config2.peering_port, config1.peering_port);
	ASSERT_EQ (config2.logging.node_lifetime_tracing_value, config1.logging.node_lifetime_tracing_value);
	ASSERT_EQ (config2.online_weight_minimum, config1.online_weight_minimum);
	ASSERT_EQ (config2.online_weight_quorum, config1.online_weight_quorum);
	ASSERT_EQ (config2.password_fanout, config1.password_fanout);
	ASSERT_EQ (config2.enable_voting, config1.enable_voting);
	ASSERT_EQ (config2.callback_address, config1.callback_address);
	ASSERT_EQ (config2.callback_port, config1.callback_port);
	ASSERT_EQ (config2.callback_target, config1.callback_target);
	ASSERT_EQ (config2.lmdb_max_dbs, config1.lmdb_max_dbs);
}

TEST (node_config, v1_v2_upgrade)
{
	auto path (btcnew::unique_path ());
	btcnew::logging logging1;
	logging1.init (path);
	btcnew::jsonconfig tree;
	tree.put ("peering_port", std::to_string (0));
	tree.put ("packet_delay_microseconds", std::to_string (0));
	tree.put ("bootstrap_fraction_numerator", std::to_string (0));
	tree.put ("creation_rebroadcast", std::to_string (0));
	tree.put ("rebroadcast_delay", std::to_string (0));
	tree.put ("receive_minimum", btcnew::amount (0).to_string_dec ());
	btcnew::jsonconfig logging_l;
	logging1.serialize_json (logging_l);
	tree.put_child ("logging", logging_l);
	btcnew::jsonconfig preconfigured_peers_l;
	tree.put_child ("preconfigured_peers", preconfigured_peers_l);
	btcnew::jsonconfig preconfigured_representatives_l;
	tree.put_child ("preconfigured_representatives", preconfigured_representatives_l);
	bool upgraded (false);
	btcnew::node_config config1;
	config1.logging.init (path);
	ASSERT_FALSE (tree.get_optional_child ("work_peers"));
	config1.deserialize_json (upgraded, tree);
	ASSERT_TRUE (upgraded);
	ASSERT_TRUE (!!tree.get_optional_child ("work_peers"));
}

TEST (node_config, v2_v3_upgrade)
{
	btcnew::jsonconfig tree;
	add_required_children_node_config_tree (tree);
	tree.put ("peering_port", std::to_string (0));
	tree.put ("packet_delay_microseconds", std::to_string (0));
	tree.put ("bootstrap_fraction_numerator", std::to_string (0));
	tree.put ("creation_rebroadcast", std::to_string (0));
	tree.put ("rebroadcast_delay", std::to_string (0));
	tree.put ("receive_minimum", btcnew::amount (0).to_string_dec ());
	tree.put ("version", "2");

	btcnew::jsonconfig preconfigured_representatives_l;
	preconfigured_representatives_l.push ("TR6ZJ4pdp6HC76xMRpVDny5x2s8AEbrhFue3NKVxYYdmKuTEib");
	tree.replace_child ("preconfigured_representatives", preconfigured_representatives_l);

	bool upgraded (false);
	btcnew::node_config config1;
	auto path (btcnew::unique_path ());
	config1.logging.init (path);
	ASSERT_FALSE (tree.get_optional<std::string> ("inactive_supply"));
	ASSERT_FALSE (tree.get_optional<std::string> ("password_fanout"));
	ASSERT_FALSE (tree.get_optional<std::string> ("io_threads"));
	ASSERT_FALSE (tree.get_optional<std::string> ("work_threads"));
	config1.deserialize_json (upgraded, tree);
	//ASSERT_EQ (btcnew::uint128_union (0).to_string_dec (), tree.get<std::string> ("inactive_supply"));
	ASSERT_EQ ("1024", tree.get<std::string> ("password_fanout"));
	ASSERT_NE (0, std::stoul (tree.get<std::string> ("password_fanout")));
	ASSERT_TRUE (upgraded);
	auto version (tree.get<std::string> ("version"));
	ASSERT_GT (std::stoull (version), 2);
}

TEST (node_config, v15_v16_upgrade)
{
	auto test_upgrade = [] (auto old_preconfigured_peers_url, auto new_preconfigured_peers_url) {
		auto path (btcnew::unique_path ());
		btcnew::jsonconfig tree;
		add_required_children_node_config_tree (tree);
		tree.put ("version", "15");

		const char * dummy_peer = "127.5.2.1";
		btcnew::jsonconfig preconfigured_peers_json;
		preconfigured_peers_json.push (old_preconfigured_peers_url);
		preconfigured_peers_json.push (dummy_peer);
		tree.replace_child ("preconfigured_peers", preconfigured_peers_json);

		auto upgraded (false);
		btcnew::node_config config;
		config.logging.init (path);
		// These config options should not be present at version 15
		ASSERT_FALSE (tree.get_optional_child ("allow_local_peers"));
		ASSERT_FALSE (tree.get_optional_child ("signature_checker_threads"));
		ASSERT_FALSE (tree.get_optional_child ("vote_minimum"));
		config.deserialize_json (upgraded, tree);
		// The config options should be added after the upgrade
		ASSERT_TRUE (!!tree.get_optional_child ("allow_local_peers"));
		ASSERT_TRUE (!!tree.get_optional_child ("signature_checker_threads"));
		ASSERT_TRUE (!!tree.get_optional_child ("vote_minimum"));

		ASSERT_TRUE (upgraded);
		auto version (tree.get<std::string> ("version"));

		auto read_preconfigured_peers_json (tree.get_required_child ("preconfigured_peers"));
		std::vector<std::string> preconfigured_peers;
		read_preconfigured_peers_json.array_entries<std::string> ([&preconfigured_peers] (const auto & entry) {
			preconfigured_peers.push_back (entry);
		});

		// Check that the new peer is updated while the other peer is untouched
		ASSERT_EQ (preconfigured_peers.size (), 2);
		ASSERT_EQ (preconfigured_peers.front (), new_preconfigured_peers_url);
		ASSERT_EQ (preconfigured_peers.back (), dummy_peer);

		// Check version is updated
		ASSERT_GT (std::stoull (version), 15);
	};

	// Check that upgrades work with both
	test_upgrade ("rai.raiblocks.net", "peering.btcnew.org");
	test_upgrade ("rai-beta.raiblocks.net", "peering-beta.btcnew.org");
}

TEST (node_config, v16_values)
{
	btcnew::jsonconfig tree;
	add_required_children_node_config_tree (tree);

	auto path (btcnew::unique_path ());
	auto upgraded (false);
	btcnew::node_config config;
	config.logging.init (path);

	// Check config is correct
	tree.put ("allow_local_peers", false);
	tree.put ("signature_checker_threads", 1);
	tree.put ("vote_minimum", btcnew::Gbtcnew_ratio.convert_to<std::string> ());
	config.deserialize_json (upgraded, tree);
	ASSERT_FALSE (upgraded);
	ASSERT_FALSE (config.allow_local_peers);
	ASSERT_EQ (config.signature_checker_threads, 1);
	ASSERT_EQ (config.vote_minimum.number (), btcnew::Gbtcnew_ratio);

	// Check config is correct with other values
	tree.put ("allow_local_peers", true);
	tree.put ("signature_checker_threads", 4);
	tree.put ("vote_minimum", (std::numeric_limits<btcnew::uint128_t>::max () - 100).convert_to<std::string> ());
	upgraded = false;
	config.deserialize_json (upgraded, tree);
	ASSERT_FALSE (upgraded);
	ASSERT_TRUE (config.allow_local_peers);
	ASSERT_EQ (config.signature_checker_threads, 4);
	ASSERT_EQ (config.vote_minimum.number (), std::numeric_limits<btcnew::uint128_t>::max () - 100);
}

TEST (node_config, v16_v17_upgrade)
{
	auto path (btcnew::unique_path ());
	btcnew::jsonconfig tree;
	add_required_children_node_config_tree (tree);
	tree.put ("version", "16");

	auto upgraded (false);
	btcnew::node_config config;
	config.logging.init (path);
	// These config options should not be present
	ASSERT_FALSE (tree.get_optional_child ("tcp_io_timeout"));
	ASSERT_FALSE (tree.get_optional_child ("pow_sleep_interval"));
	ASSERT_FALSE (tree.get_optional_child ("external_address"));
	ASSERT_FALSE (tree.get_optional_child ("external_port"));
	ASSERT_FALSE (tree.get_optional_child ("tcp_incoming_connections_max"));
	ASSERT_FALSE (tree.get_optional_child ("vote_generator_delay"));
	ASSERT_FALSE (tree.get_optional_child ("vote_generator_threshold"));
	ASSERT_FALSE (tree.get_optional_child ("diagnostics"));
	ASSERT_FALSE (tree.get_optional_child ("use_memory_pools"));
	ASSERT_FALSE (tree.get_optional_child ("confirmation_history_size"));
	ASSERT_FALSE (tree.get_optional_child ("active_elections_size"));
	ASSERT_FALSE (tree.get_optional_child ("bandwidth_limit"));
	ASSERT_FALSE (tree.get_optional_child ("conf_height_processor_batch_min_time"));

	config.deserialize_json (upgraded, tree);
	// The config options should be added after the upgrade
	ASSERT_TRUE (!!tree.get_optional_child ("tcp_io_timeout"));
	ASSERT_TRUE (!!tree.get_optional_child ("pow_sleep_interval"));
	ASSERT_TRUE (!!tree.get_optional_child ("external_address"));
	ASSERT_TRUE (!!tree.get_optional_child ("external_port"));
	ASSERT_TRUE (!!tree.get_optional_child ("tcp_incoming_connections_max"));
	ASSERT_TRUE (!!tree.get_optional_child ("vote_generator_delay"));
	ASSERT_TRUE (!!tree.get_optional_child ("vote_generator_threshold"));
	ASSERT_TRUE (!!tree.get_optional_child ("diagnostics"));
	ASSERT_TRUE (!!tree.get_optional_child ("use_memory_pools"));
	ASSERT_TRUE (!!tree.get_optional_child ("confirmation_history_size"));
	ASSERT_TRUE (!!tree.get_optional_child ("active_elections_size"));
	ASSERT_TRUE (!!tree.get_optional_child ("bandwidth_limit"));
	ASSERT_TRUE (!!tree.get_optional_child ("conf_height_processor_batch_min_time"));

	ASSERT_TRUE (upgraded);
	auto version (tree.get<std::string> ("version"));

	// Check version is updated
	ASSERT_GT (std::stoull (version), 16);
}

TEST (node_config, v17_values)
{
	btcnew::jsonconfig tree;
	add_required_children_node_config_tree (tree);

	auto path (btcnew::unique_path ());
	auto upgraded (false);
	btcnew::node_config config;
	config.logging.init (path);

	// Check config is correct
	{
		tree.put ("tcp_io_timeout", 1);
		tree.put ("pow_sleep_interval", 0);
		tree.put ("external_address", "::1");
		tree.put ("external_port", 0);
		tree.put ("tcp_incoming_connections_max", 1);
		tree.put ("vote_generator_delay", 50);
		tree.put ("vote_generator_threshold", 3);
		btcnew::jsonconfig txn_tracking_l;
		txn_tracking_l.put ("enable", false);
		txn_tracking_l.put ("min_read_txn_time", 0);
		txn_tracking_l.put ("min_write_txn_time", 0);
		txn_tracking_l.put ("ignore_writes_below_block_processor_max_time", true);
		btcnew::jsonconfig diagnostics_l;
		diagnostics_l.put_child ("txn_tracking", txn_tracking_l);
		tree.put_child ("diagnostics", diagnostics_l);
		tree.put ("use_memory_pools", true);
		tree.put ("confirmation_history_size", 2048);
		tree.put ("active_elections_size", 50000);
		tree.put ("bandwidth_limit", 5242880);
		tree.put ("conf_height_processor_batch_min_time", 0);
	}

	config.deserialize_json (upgraded, tree);
	ASSERT_FALSE (upgraded);
	ASSERT_EQ (config.tcp_io_timeout.count (), 1);
	ASSERT_EQ (config.pow_sleep_interval.count (), 0);
	ASSERT_EQ (config.external_address, boost::asio::ip::address_v6::from_string ("::1"));
	ASSERT_EQ (config.external_port, 0);
	ASSERT_EQ (config.tcp_incoming_connections_max, 1);
	ASSERT_FALSE (config.diagnostics_config.txn_tracking.enable);
	ASSERT_EQ (config.diagnostics_config.txn_tracking.min_read_txn_time.count (), 0);
	ASSERT_EQ (config.diagnostics_config.txn_tracking.min_write_txn_time.count (), 0);
	ASSERT_TRUE (config.diagnostics_config.txn_tracking.ignore_writes_below_block_processor_max_time);
	ASSERT_TRUE (config.use_memory_pools);
	ASSERT_EQ (config.confirmation_history_size, 2048);
	ASSERT_EQ (config.active_elections_size, 50000);
	ASSERT_EQ (config.bandwidth_limit, 5242880);
	ASSERT_EQ (config.conf_height_processor_batch_min_time.count (), 0);

	// Check config is correct with other values
	tree.put ("tcp_io_timeout", std::numeric_limits<unsigned long>::max () - 100);
	tree.put ("pow_sleep_interval", std::numeric_limits<unsigned long>::max () - 100);
	tree.put ("external_address", "::ffff:192.168.1.1");
	tree.put ("external_port", std::numeric_limits<uint16_t>::max () - 1);
	tree.put ("tcp_incoming_connections_max", std::numeric_limits<unsigned>::max ());
	tree.put ("vote_generator_delay", std::numeric_limits<unsigned long>::max () - 100);
	tree.put ("vote_generator_threshold", 10);
	btcnew::jsonconfig txn_tracking_l;
	txn_tracking_l.put ("enable", true);
	txn_tracking_l.put ("min_read_txn_time", 1234);
	txn_tracking_l.put ("min_write_txn_time", std::numeric_limits<unsigned>::max ());
	txn_tracking_l.put ("ignore_writes_below_block_processor_max_time", false);
	btcnew::jsonconfig diagnostics_l;
	diagnostics_l.replace_child ("txn_tracking", txn_tracking_l);
	tree.replace_child ("diagnostics", diagnostics_l);
	tree.put ("use_memory_pools", false);
	tree.put ("confirmation_history_size", std::numeric_limits<unsigned long long>::max ());
	tree.put ("active_elections_size", std::numeric_limits<unsigned long long>::max ());
	tree.put ("bandwidth_limit", std::numeric_limits<size_t>::max ());
	tree.put ("conf_height_processor_batch_min_time", 500);

	upgraded = false;
	config.deserialize_json (upgraded, tree);
	ASSERT_FALSE (upgraded);
	ASSERT_EQ (config.tcp_io_timeout.count (), std::numeric_limits<unsigned long>::max () - 100);
	ASSERT_EQ (config.pow_sleep_interval.count (), std::numeric_limits<unsigned long>::max () - 100);
	ASSERT_EQ (config.external_address, boost::asio::ip::address_v6::from_string ("::ffff:192.168.1.1"));
	ASSERT_EQ (config.external_port, std::numeric_limits<uint16_t>::max () - 1);
	ASSERT_EQ (config.tcp_incoming_connections_max, std::numeric_limits<unsigned>::max ());
	ASSERT_EQ (config.vote_generator_delay.count (), std::numeric_limits<unsigned long>::max () - 100);
	ASSERT_EQ (config.vote_generator_threshold, 10);
	ASSERT_TRUE (config.diagnostics_config.txn_tracking.enable);
	ASSERT_EQ (config.diagnostics_config.txn_tracking.min_read_txn_time.count (), 1234);
	ASSERT_EQ (config.tcp_incoming_connections_max, std::numeric_limits<unsigned>::max ());
	ASSERT_EQ (config.diagnostics_config.txn_tracking.min_write_txn_time.count (), std::numeric_limits<unsigned>::max ());
	ASSERT_FALSE (config.diagnostics_config.txn_tracking.ignore_writes_below_block_processor_max_time);
	ASSERT_FALSE (config.use_memory_pools);
	ASSERT_EQ (config.confirmation_history_size, std::numeric_limits<unsigned long long>::max ());
	ASSERT_EQ (config.active_elections_size, std::numeric_limits<unsigned long long>::max ());
	ASSERT_EQ (config.bandwidth_limit, std::numeric_limits<size_t>::max ());
	ASSERT_EQ (config.conf_height_processor_batch_min_time.count (), 500);
}

TEST (node_config, v17_v18_upgrade)
{
	auto path (btcnew::unique_path ());
	btcnew::jsonconfig tree;
	add_required_children_node_config_tree (tree);
	tree.put ("version", "17");

	auto upgraded (false);
	btcnew::node_config config;
	config.logging.init (path);

	// Initial values for configs that should be upgraded
	config.active_elections_size = 50000;
	config.vote_generator_delay = 500ms;

	// These config options should not be present
	ASSERT_FALSE (tree.get_optional_child ("backup_before_upgrade"));
	ASSERT_FALSE (tree.get_optional_child ("work_watcher_period"));

	config.deserialize_json (upgraded, tree);

	// These configs should have been upgraded
	ASSERT_EQ (100, tree.get<unsigned> ("vote_generator_delay"));
	ASSERT_EQ (10000, tree.get<unsigned long long> ("active_elections_size"));

	// The config options should be added after the upgrade
	ASSERT_TRUE (!!tree.get_optional_child ("backup_before_upgrade"));
	ASSERT_TRUE (!!tree.get_optional_child ("work_watcher_period"));

	ASSERT_TRUE (upgraded);
	auto version (tree.get<std::string> ("version"));

	// Check version is updated
	ASSERT_GT (std::stoull (version), 17);
}

TEST (node_config, v18_values)
{
	btcnew::jsonconfig tree;
	add_required_children_node_config_tree (tree);

	auto path (btcnew::unique_path ());
	auto upgraded (false);
	btcnew::node_config config;
	config.logging.init (path);

	// Check config is correct
	{
		tree.put ("active_elections_size", 10000);
		tree.put ("vote_generator_delay", 100);
		tree.put ("backup_before_upgrade", true);
		tree.put ("work_watcher_period", 5);
	}

	config.deserialize_json (upgraded, tree);
	ASSERT_FALSE (upgraded);
	ASSERT_EQ (config.active_elections_size, 10000);
	ASSERT_EQ (config.vote_generator_delay.count (), 100);
	ASSERT_EQ (config.backup_before_upgrade, true);
	ASSERT_EQ (config.work_watcher_period.count (), 5);

	// Check config is correct with other values
	tree.put ("active_elections_size", 5);
	tree.put ("vote_generator_delay", std::numeric_limits<unsigned long>::max () - 100);
	tree.put ("backup_before_upgrade", false);
	tree.put ("work_watcher_period", 999);

	upgraded = false;
	config.deserialize_json (upgraded, tree);
	ASSERT_FALSE (upgraded);
	ASSERT_EQ (config.active_elections_size, 5);
	ASSERT_EQ (config.vote_generator_delay.count (), std::numeric_limits<unsigned long>::max () - 100);
	ASSERT_EQ (config.backup_before_upgrade, false);
	ASSERT_EQ (config.work_watcher_period.count (), 999);
}

// Regression test to ensure that deserializing includes changes node via get_required_child
TEST (node_config, required_child)
{
	auto path (btcnew::unique_path ());
	btcnew::logging logging1;
	btcnew::logging logging2;
	logging1.init (path);
	btcnew::jsonconfig tree;

	btcnew::jsonconfig logging_l;
	logging1.serialize_json (logging_l);
	tree.put_child ("logging", logging_l);
	auto child_l (tree.get_required_child ("logging"));
	child_l.put<bool> ("flush", !logging1.flush);
	bool upgraded (false);
	logging2.deserialize_json (upgraded, child_l);

	ASSERT_NE (logging1.flush, logging2.flush);
}

TEST (node_config, random_rep)
{
	auto path (btcnew::unique_path ());
	btcnew::logging logging1;
	logging1.init (path);
	btcnew::node_config config1 (100, logging1);
	auto rep (config1.random_representative ());
	ASSERT_NE (config1.preconfigured_representatives.end (), std::find (config1.preconfigured_representatives.begin (), config1.preconfigured_representatives.end (), rep));
}

class json_initial_value_test final
{
public:
	explicit json_initial_value_test (std::string const & text_a) :
	text (text_a)
	{
	}
	btcnew::error serialize_json (btcnew::jsonconfig & json)
	{
		json.put ("thing", text);
		return json.get_error ();
	}
	std::string text;
};

class json_upgrade_test final
{
public:
	btcnew::error deserialize_json (bool & upgraded, btcnew::jsonconfig & json)
	{
		if (!json.empty ())
		{
			auto text_l (json.get<std::string> ("thing"));
			if (text_l == "junktest" || text_l == "created")
			{
				upgraded = true;
				text_l = "changed";
				json.put ("thing", text_l);
			}
			if (text_l == "error")
			{
				json.get_error () = btcnew::error_common::generic;
			}
			text = text_l;
		}
		else
		{
			upgraded = true;
			text = "created";
			json.put ("thing", text);
		}
		return json.get_error ();
	}
	std::string text;
};

/** Both create and upgrade via read_and_update() */
TEST (json, create_and_upgrade)
{
	auto path (btcnew::unique_path ());
	btcnew::jsonconfig json;
	json_upgrade_test object1;
	ASSERT_FALSE (json.read_and_update (object1, path));
	ASSERT_EQ ("created", object1.text);

	btcnew::jsonconfig json2;
	json_upgrade_test object2;
	ASSERT_FALSE (json2.read_and_update (object2, path));
	ASSERT_EQ ("changed", object2.text);
}

/** Create config manually, then upgrade via read_and_update() with multiple calls to test idempotence */
TEST (json, upgrade_from_existing)
{
	auto path (btcnew::unique_path ());
	btcnew::jsonconfig json;
	json_initial_value_test junktest ("junktest");
	junktest.serialize_json (json);
	json.write (path);
	json_upgrade_test object1;
	ASSERT_FALSE (json.read_and_update (object1, path));
	ASSERT_EQ ("changed", object1.text);
	ASSERT_FALSE (json.read_and_update (object1, path));
	ASSERT_EQ ("changed", object1.text);
}

/** Test that backups are made only when there is an upgrade */
TEST (json, backup)
{
	auto dir (btcnew::unique_path ());
	namespace fs = boost::filesystem;
	fs::create_directory (dir);
	auto path = dir / dir.leaf ();

	// Create json file
	btcnew::jsonconfig json;
	json_upgrade_test object1;
	ASSERT_FALSE (json.read_and_update (object1, path));
	ASSERT_EQ ("created", object1.text);

	/** Returns 'dir' if backup file cannot be found */
	// clang-format off
	auto get_backup_path = [&dir]() {
		for (fs::directory_iterator itr (dir); itr != fs::directory_iterator (); ++itr)
		{
			if (itr->path ().filename ().string ().find ("_backup_") != std::string::npos)
			{
				return itr->path ();
			}
		}
		return dir;
	};

	auto get_file_count = [&dir]() {
		return std::count_if (boost::filesystem::directory_iterator (dir), boost::filesystem::directory_iterator (), static_cast<bool (*) (const boost::filesystem::path &)> (boost::filesystem::is_regular_file));
	};
	// clang-format on

	// There should only be the original file in this directory
	ASSERT_EQ (get_file_count (), 1);
	ASSERT_EQ (get_backup_path (), dir);

	// Upgrade, check that there is a backup which matches the first object
	ASSERT_FALSE (json.read_and_update (object1, path));
	ASSERT_EQ (get_file_count (), 2);
	ASSERT_NE (get_backup_path (), path);

	// Check there is a backup which has the same contents as the original file
	btcnew::jsonconfig json1;
	ASSERT_FALSE (json1.read (get_backup_path ()));
	ASSERT_EQ (json1.get<std::string> ("thing"), "created");

	// Try and upgrade an already upgraded file, should not create any backups
	ASSERT_FALSE (json.read_and_update (object1, path));
	ASSERT_EQ (get_file_count (), 2);
}

TEST (node_flags, disable_tcp_realtime)
{
	btcnew::system system (24000, 1);
	auto node1 = system.nodes[0];
	btcnew::node_flags node_flags;
	node_flags.disable_tcp_realtime = true;
	auto node2 = system.add_node (btcnew::node_config (24001, system.logging), node_flags);
	ASSERT_EQ (1, node1->network.size ());
	auto list1 (node1->network.list (2));
	ASSERT_EQ (node2->network.endpoint (), list1[0]->get_endpoint ());
	ASSERT_EQ (btcnew::transport::transport_type::udp, list1[0]->get_type ());
	ASSERT_EQ (1, node2->network.size ());
	auto list2 (node2->network.list (2));
	ASSERT_EQ (node1->network.endpoint (), list2[0]->get_endpoint ());
	ASSERT_EQ (btcnew::transport::transport_type::udp, list2[0]->get_type ());
}

TEST (node_flags, disable_udp)
{
	btcnew::system system (24000, 1);
	auto node1 = system.nodes[0];
	btcnew::node_flags node_flags;
	node_flags.disable_udp = true;
	auto node2 (std::make_shared<btcnew::node> (system.io_ctx, btcnew::unique_path (), system.alarm, btcnew::node_config (24001, system.logging), system.work, node_flags));
	system.nodes.push_back (node2);
	node2->start ();
	// Send UDP message
	auto channel (std::make_shared<btcnew::transport::channel_udp> (node1->network.udp_channels, node2->network.endpoint (), node2->network_params.protocol.protocol_version));
	node1->network.send_keepalive (channel);
	std::this_thread::sleep_for (std::chrono::milliseconds (500));
	// Check empty network
	ASSERT_EQ (0, node1->network.size ());
	ASSERT_EQ (0, node2->network.size ());
	// Send TCP handshake
	node1->network.merge_peer (node2->network.endpoint ());
	system.deadline_set (5s);
	while (node1->bootstrap.realtime_count != 1 || node2->bootstrap.realtime_count != 1)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_EQ (1, node1->network.size ());
	auto list1 (node1->network.list (2));
	ASSERT_EQ (node2->network.endpoint (), list1[0]->get_endpoint ());
	ASSERT_EQ (btcnew::transport::transport_type::tcp, list1[0]->get_type ());
	ASSERT_EQ (1, node2->network.size ());
	auto list2 (node2->network.list (2));
	ASSERT_EQ (node1->network.endpoint (), list2[0]->get_endpoint ());
	ASSERT_EQ (btcnew::transport::transport_type::tcp, list2[0]->get_type ());
	node2->stop ();
}

TEST (node, fork_publish)
{
	std::weak_ptr<btcnew::node> node0;
	{
		btcnew::system system (24000, 1);
		node0 = system.nodes[0];
		auto & node1 (*system.nodes[0]);
		system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
		btcnew::keypair key1;
		btcnew::genesis genesis;
		auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key1.pub, btcnew::genesis_amount - 100, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
		node1.work_generate_blocking (*send1);
		btcnew::keypair key2;
		auto send2 (std::make_shared<btcnew::send_block> (genesis.hash (), key2.pub, btcnew::genesis_amount - 100, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
		node1.work_generate_blocking (*send2);
		node1.process_active (send1);
		node1.block_processor.flush ();
		ASSERT_EQ (1, node1.active.size ());
		btcnew::unique_lock<std::mutex> lock (node1.active.mutex);
		auto existing (node1.active.roots.find (send1->qualified_root ()));
		ASSERT_NE (node1.active.roots.end (), existing);
		auto election (existing->election);
		lock.unlock ();
		system.deadline_set (1s);
		// Wait until the genesis rep activated & makes vote
		while (election->last_votes_size () != 2)
		{
			node1.block_processor.generator.add (send1->hash ());
			node1.vote_processor.flush ();
			ASSERT_NO_ERROR (system.poll ());
		}
		node1.process_active (send2);
		node1.block_processor.flush ();
		lock.lock ();
		auto existing1 (election->last_votes.find (btcnew::test_genesis_key.pub));
		ASSERT_NE (election->last_votes.end (), existing1);
		ASSERT_EQ (send1->hash (), existing1->second.hash);
		auto transaction (node1.store.tx_begin_read ());
		auto winner (*election->tally ().begin ());
		ASSERT_EQ (*send1, *winner.second);
		ASSERT_EQ (btcnew::genesis_amount - 100, winner.first);
	}
	ASSERT_TRUE (node0.expired ());
}

TEST (node, fork_keep)
{
	btcnew::system system (24000, 2);
	auto & node1 (*system.nodes[0]);
	auto & node2 (*system.nodes[1]);
	ASSERT_EQ (1, node1.network.size ());
	btcnew::keypair key1;
	btcnew::keypair key2;
	btcnew::genesis genesis;
	// send1 and send2 fork to different accounts
	auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key1.pub, btcnew::genesis_amount - 100, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	auto send2 (std::make_shared<btcnew::send_block> (genesis.hash (), key2.pub, btcnew::genesis_amount - 100, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	node1.process_active (send1);
	node1.block_processor.flush ();
	node2.process_active (send1);
	node2.block_processor.flush ();
	ASSERT_EQ (1, node1.active.size ());
	ASSERT_EQ (1, node2.active.size ());
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	node1.process_active (send2);
	node1.block_processor.flush ();
	node2.process_active (send2);
	node2.block_processor.flush ();
	btcnew::unique_lock<std::mutex> lock (node2.active.mutex);
	auto conflict (node2.active.roots.find (btcnew::qualified_root (genesis.hash (), genesis.hash ())));
	ASSERT_NE (node2.active.roots.end (), conflict);
	auto votes1 (conflict->election);
	ASSERT_NE (nullptr, votes1);
	ASSERT_EQ (1, votes1->last_votes.size ());
	lock.unlock ();
	{
		auto transaction0 (system.nodes[0]->store.tx_begin_read ());
		auto transaction1 (system.nodes[1]->store.tx_begin_read ());
		ASSERT_TRUE (system.nodes[0]->store.block_exists (transaction0, send1->hash ()));
		ASSERT_TRUE (system.nodes[1]->store.block_exists (transaction1, send1->hash ()));
	}
	system.deadline_set (1.5min);
	// Wait until the genesis rep makes a vote
	while (votes1->last_votes_size () == 1)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	auto transaction0 (system.nodes[0]->store.tx_begin_read ());
	auto transaction1 (system.nodes[1]->store.tx_begin_read ());
	// The vote should be in agreement with what we already have.
	lock.lock ();
	auto winner (*votes1->tally ().begin ());
	ASSERT_EQ (*send1, *winner.second);
	ASSERT_EQ (btcnew::genesis_amount - 100, winner.first);
	ASSERT_TRUE (system.nodes[0]->store.block_exists (transaction0, send1->hash ()));
	ASSERT_TRUE (system.nodes[1]->store.block_exists (transaction1, send1->hash ()));
}

TEST (node, fork_flip)
{
	btcnew::system system (24000, 2);
	auto & node1 (*system.nodes[0]);
	auto & node2 (*system.nodes[1]);
	ASSERT_EQ (1, node1.network.size ());
	btcnew::keypair key1;
	btcnew::genesis genesis;
	auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key1.pub, btcnew::genesis_amount - 100, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	btcnew::publish publish1 (send1);
	btcnew::keypair key2;
	auto send2 (std::make_shared<btcnew::send_block> (genesis.hash (), key2.pub, btcnew::genesis_amount - 100, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	btcnew::publish publish2 (send2);
	auto channel1 (node1.network.udp_channels.create (node1.network.endpoint ()));
	node1.network.process_message (publish1, channel1);
	node1.block_processor.flush ();
	auto channel2 (node2.network.udp_channels.create (node1.network.endpoint ()));
	node2.network.process_message (publish2, channel2);
	node2.block_processor.flush ();
	ASSERT_EQ (1, node1.active.size ());
	ASSERT_EQ (1, node2.active.size ());
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	node1.network.process_message (publish2, channel1);
	node1.block_processor.flush ();
	node2.network.process_message (publish1, channel2);
	node2.block_processor.flush ();
	btcnew::unique_lock<std::mutex> lock (node2.active.mutex);
	auto conflict (node2.active.roots.find (btcnew::qualified_root (genesis.hash (), genesis.hash ())));
	ASSERT_NE (node2.active.roots.end (), conflict);
	auto votes1 (conflict->election);
	ASSERT_NE (nullptr, votes1);
	ASSERT_EQ (1, votes1->last_votes.size ());
	lock.unlock ();
	{
		auto transaction (system.nodes[0]->store.tx_begin_read ());
		ASSERT_TRUE (node1.store.block_exists (transaction, publish1.block->hash ()));
	}
	{
		auto transaction (system.nodes[1]->store.tx_begin_read ());
		ASSERT_TRUE (node2.store.block_exists (transaction, publish2.block->hash ()));
	}
	system.deadline_set (10s);
	auto done (false);
	while (!done)
	{
		ASSERT_NO_ERROR (system.poll ());
		done = node2.ledger.block_exists (publish1.block->hash ());
	}
	auto transaction1 (system.nodes[0]->store.tx_begin_read ());
	auto transaction2 (system.nodes[1]->store.tx_begin_read ());
	lock.lock ();
	auto winner (*votes1->tally ().begin ());
	ASSERT_EQ (*publish1.block, *winner.second);
	ASSERT_EQ (btcnew::genesis_amount - 100, winner.first);
	ASSERT_TRUE (node1.store.block_exists (transaction1, publish1.block->hash ()));
	ASSERT_TRUE (node2.store.block_exists (transaction2, publish1.block->hash ()));
	ASSERT_FALSE (node2.store.block_exists (transaction2, publish2.block->hash ()));
}

TEST (node, fork_multi_flip)
{
	btcnew::system system (24000, 2);
	auto & node1 (*system.nodes[0]);
	auto & node2 (*system.nodes[1]);
	ASSERT_EQ (1, node1.network.size ());
	btcnew::keypair key1;
	btcnew::genesis genesis;
	auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key1.pub, btcnew::genesis_amount - 100, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	btcnew::publish publish1 (send1);
	btcnew::keypair key2;
	auto send2 (std::make_shared<btcnew::send_block> (genesis.hash (), key2.pub, btcnew::genesis_amount - 100, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	btcnew::publish publish2 (send2);
	auto send3 (std::make_shared<btcnew::send_block> (publish2.block->hash (), key2.pub, btcnew::genesis_amount - 100, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (publish2.block->hash ())));
	btcnew::publish publish3 (send3);
	node1.network.process_message (publish1, node1.network.udp_channels.create (node1.network.endpoint ()));
	node1.block_processor.flush ();
	node2.network.process_message (publish2, node2.network.udp_channels.create (node2.network.endpoint ()));
	node2.network.process_message (publish3, node2.network.udp_channels.create (node2.network.endpoint ()));
	node2.block_processor.flush ();
	ASSERT_EQ (1, node1.active.size ());
	ASSERT_EQ (2, node2.active.size ());
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	node1.network.process_message (publish2, node1.network.udp_channels.create (node1.network.endpoint ()));
	node1.network.process_message (publish3, node1.network.udp_channels.create (node1.network.endpoint ()));
	node1.block_processor.flush ();
	node2.network.process_message (publish1, node2.network.udp_channels.create (node2.network.endpoint ()));
	node2.block_processor.flush ();
	btcnew::unique_lock<std::mutex> lock (node2.active.mutex);
	auto conflict (node2.active.roots.find (btcnew::qualified_root (genesis.hash (), genesis.hash ())));
	ASSERT_NE (node2.active.roots.end (), conflict);
	auto votes1 (conflict->election);
	ASSERT_NE (nullptr, votes1);
	ASSERT_EQ (1, votes1->last_votes.size ());
	lock.unlock ();
	{
		auto transaction (system.nodes[0]->store.tx_begin_read ());
		ASSERT_TRUE (node1.store.block_exists (transaction, publish1.block->hash ()));
	}
	{
		auto transaction (system.nodes[1]->store.tx_begin_read ());
		ASSERT_TRUE (node2.store.block_exists (transaction, publish2.block->hash ()));
		ASSERT_TRUE (node2.store.block_exists (transaction, publish3.block->hash ()));
	}
	system.deadline_set (10s);
	auto done (false);
	while (!done)
	{
		ASSERT_NO_ERROR (system.poll ());
		done = node2.ledger.block_exists (publish1.block->hash ());
	}
	auto transaction1 (system.nodes[0]->store.tx_begin_read ());
	auto transaction2 (system.nodes[1]->store.tx_begin_read ());
	lock.lock ();
	auto winner (*votes1->tally ().begin ());
	ASSERT_EQ (*publish1.block, *winner.second);
	ASSERT_EQ (btcnew::genesis_amount - 100, winner.first);
	ASSERT_TRUE (node1.store.block_exists (transaction1, publish1.block->hash ()));
	ASSERT_TRUE (node2.store.block_exists (transaction2, publish1.block->hash ()));
	ASSERT_FALSE (node2.store.block_exists (transaction2, publish2.block->hash ()));
	ASSERT_FALSE (node2.store.block_exists (transaction2, publish3.block->hash ()));
}

// Blocks that are no longer actively being voted on should be able to be evicted through bootstrapping.
// This could happen if a fork wasn't resolved before the process previously shut down
TEST (node, fork_bootstrap_flip)
{
	btcnew::system system0 (24000, 1);
	btcnew::system system1 (24001, 1);
	auto & node1 (*system0.nodes[0]);
	auto & node2 (*system1.nodes[0]);
	system0.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	btcnew::block_hash latest (system0.nodes[0]->latest (btcnew::test_genesis_key.pub));
	btcnew::keypair key1;
	auto send1 (std::make_shared<btcnew::send_block> (latest, key1.pub, btcnew::genesis_amount - btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system0.work.generate (latest)));
	btcnew::keypair key2;
	auto send2 (std::make_shared<btcnew::send_block> (latest, key2.pub, btcnew::genesis_amount - btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system0.work.generate (latest)));
	// Insert but don't rebroadcast, simulating settled blocks
	node1.block_processor.add (send1, btcnew::seconds_since_epoch ());
	node1.block_processor.flush ();
	node2.block_processor.add (send2, btcnew::seconds_since_epoch ());
	node2.block_processor.flush ();
	{
		auto transaction (node2.store.tx_begin_read ());
		ASSERT_TRUE (node2.store.block_exists (transaction, send2->hash ()));
	}
	auto channel (std::make_shared<btcnew::transport::channel_udp> (node1.network.udp_channels, node2.network.endpoint (), node2.network_params.protocol.protocol_version));
	node1.network.send_keepalive (channel);
	system1.deadline_set (50s);
	while (node2.network.empty ())
	{
		ASSERT_NO_ERROR (system0.poll ());
		ASSERT_NO_ERROR (system1.poll ());
	}
	node2.bootstrap_initiator.bootstrap (node1.network.endpoint ());
	auto again (true);
	system1.deadline_set (50s);
	while (again)
	{
		ASSERT_NO_ERROR (system0.poll ());
		ASSERT_NO_ERROR (system1.poll ());
		auto transaction (node2.store.tx_begin_read ());
		again = !node2.store.block_exists (transaction, send1->hash ());
	}
}

TEST (node, fork_open)
{
	btcnew::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	btcnew::keypair key1;
	btcnew::genesis genesis;
	auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key1.pub, 0, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	btcnew::publish publish1 (send1);
	auto channel1 (node1.network.udp_channels.create (node1.network.endpoint ()));
	node1.network.process_message (publish1, channel1);
	node1.block_processor.flush ();
	auto open1 (std::make_shared<btcnew::open_block> (publish1.block->hash (), 1, key1.pub, key1.prv, key1.pub, *system.work.generate (key1.pub)));
	btcnew::publish publish2 (open1);
	node1.network.process_message (publish2, channel1);
	node1.block_processor.flush ();
	auto open2 (std::make_shared<btcnew::open_block> (publish1.block->hash (), 2, key1.pub, key1.prv, key1.pub, *system.work.generate (key1.pub)));
	btcnew::publish publish3 (open2);
	ASSERT_EQ (2, node1.active.size ());
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	node1.network.process_message (publish3, channel1);
	node1.block_processor.flush ();
}

TEST (node, fork_open_flip)
{
	btcnew::system system (24000, 2);
	auto & node1 (*system.nodes[0]);
	auto & node2 (*system.nodes[1]);
	ASSERT_EQ (1, node1.network.size ());
	btcnew::keypair key1;
	btcnew::genesis genesis;
	btcnew::keypair rep1;
	btcnew::keypair rep2;
	auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key1.pub, btcnew::genesis_amount - 1, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	node1.process_active (send1);
	node2.process_active (send1);
	// We should be keeping this block
	auto open1 (std::make_shared<btcnew::open_block> (send1->hash (), rep1.pub, key1.pub, key1.prv, key1.pub, *system.work.generate (key1.pub)));
	// This block should be evicted
	auto open2 (std::make_shared<btcnew::open_block> (send1->hash (), rep2.pub, key1.pub, key1.prv, key1.pub, *system.work.generate (key1.pub)));
	ASSERT_FALSE (*open1 == *open2);
	// node1 gets copy that will remain
	node1.process_active (open1);
	node1.block_processor.flush ();
	// node2 gets copy that will be evicted
	node2.process_active (open2);
	node2.block_processor.flush ();
	ASSERT_EQ (2, node1.active.size ());
	ASSERT_EQ (2, node2.active.size ());
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	// Notify both nodes that a fork exists
	node1.process_active (open2);
	node1.block_processor.flush ();
	node2.process_active (open1);
	node2.block_processor.flush ();
	btcnew::unique_lock<std::mutex> lock (node2.active.mutex);
	auto conflict (node2.active.roots.find (open1->qualified_root ()));
	ASSERT_NE (node2.active.roots.end (), conflict);
	auto votes1 (conflict->election);
	ASSERT_NE (nullptr, votes1);
	ASSERT_EQ (1, votes1->last_votes.size ());
	lock.unlock ();
	ASSERT_TRUE (node1.block (open1->hash ()) != nullptr);
	ASSERT_TRUE (node2.block (open2->hash ()) != nullptr);
	system.deadline_set (10s);
	// Node2 should eventually settle on open1
	while (node2.block (open1->hash ()) == nullptr)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	node2.block_processor.flush ();
	auto transaction1 (system.nodes[0]->store.tx_begin_read ());
	auto transaction2 (system.nodes[1]->store.tx_begin_read ());
	lock.lock ();
	auto winner (*votes1->tally ().begin ());
	ASSERT_EQ (*open1, *winner.second);
	ASSERT_EQ (btcnew::genesis_amount - 1, winner.first);
	ASSERT_TRUE (node1.store.block_exists (transaction1, open1->hash ()));
	ASSERT_TRUE (node2.store.block_exists (transaction2, open1->hash ()));
	ASSERT_FALSE (node2.store.block_exists (transaction2, open2->hash ()));
}

TEST (node, coherent_observer)
{
	btcnew::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	node1.observers.blocks.add ([&node1] (btcnew::election_status const & status_a, btcnew::account const &, btcnew::uint128_t const &, bool) {
		auto transaction (node1.store.tx_begin_read ());
		ASSERT_TRUE (node1.store.block_exists (transaction, status_a.winner->hash ()));
	});
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	btcnew::keypair key;
	system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key.pub, 1);
}

TEST (node, fork_no_vote_quorum)
{
	btcnew::system system (24000, 3);
	auto & node1 (*system.nodes[0]);
	auto & node2 (*system.nodes[1]);
	auto & node3 (*system.nodes[2]);
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	auto key4 (system.wallet (0)->deterministic_insert ());
	system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key4, btcnew::genesis_amount / 4);
	auto key1 (system.wallet (1)->deterministic_insert ());
	{
		auto transaction (system.wallet (1)->wallets.tx_begin_write ());
		system.wallet (1)->store.representative_set (transaction, key1);
	}
	auto block (system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key1, node1.config.receive_minimum.number ()));
	ASSERT_NE (nullptr, block);
	system.deadline_set (30s);
	while (node3.balance (key1) != node1.config.receive_minimum.number () || node2.balance (key1) != node1.config.receive_minimum.number () || node1.balance (key1) != node1.config.receive_minimum.number ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_EQ (node1.config.receive_minimum.number (), node1.weight (key1));
	ASSERT_EQ (node1.config.receive_minimum.number (), node2.weight (key1));
	ASSERT_EQ (node1.config.receive_minimum.number (), node3.weight (key1));
	btcnew::state_block send1 (btcnew::test_genesis_key.pub, block->hash (), btcnew::test_genesis_key.pub, (btcnew::genesis_amount / 4) - (node1.config.receive_minimum.number () * 2), key1, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (block->hash ()));
	ASSERT_EQ (btcnew::process_result::progress, node1.process (send1).code);
	ASSERT_EQ (btcnew::process_result::progress, node2.process (send1).code);
	ASSERT_EQ (btcnew::process_result::progress, node3.process (send1).code);
	auto key2 (system.wallet (2)->deterministic_insert ());
	auto send2 (std::make_shared<btcnew::send_block> (block->hash (), key2, (btcnew::genesis_amount / 4) - (node1.config.receive_minimum.number () * 2), btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (block->hash ())));
	btcnew::raw_key key3;
	auto transaction (system.wallet (1)->wallets.tx_begin_read ());
	ASSERT_FALSE (system.wallet (1)->store.fetch (transaction, key1, key3));
	auto vote (std::make_shared<btcnew::vote> (key1, key3, 0, send2));
	btcnew::confirm_ack confirm (vote);
	std::vector<uint8_t> buffer;
	{
		btcnew::vectorstream stream (buffer);
		confirm.serialize (stream);
	}
	btcnew::transport::channel_udp channel (node2.network.udp_channels, node3.network.endpoint (), node1.network_params.protocol.protocol_version);
	channel.send_buffer (btcnew::shared_const_buffer (std::move (buffer)), btcnew::stat::detail::confirm_ack);
	while (node3.stats.count (btcnew::stat::type::message, btcnew::stat::detail::confirm_ack, btcnew::stat::dir::in) < 3)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_TRUE (node1.latest (btcnew::test_genesis_key.pub) == send1.hash ());
	ASSERT_TRUE (node2.latest (btcnew::test_genesis_key.pub) == send1.hash ());
	ASSERT_TRUE (node3.latest (btcnew::test_genesis_key.pub) == send1.hash ());
}

// Disabled because it sometimes takes way too long (but still eventually finishes)
TEST (node, DISABLED_fork_pre_confirm)
{
	btcnew::system system (24000, 3);
	auto & node0 (*system.nodes[0]);
	auto & node1 (*system.nodes[1]);
	auto & node2 (*system.nodes[2]);
	btcnew::genesis genesis;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	btcnew::keypair key1;
	system.wallet (1)->insert_adhoc (key1.prv);
	{
		auto transaction (system.wallet (1)->wallets.tx_begin_write ());
		system.wallet (1)->store.representative_set (transaction, key1.pub);
	}
	btcnew::keypair key2;
	system.wallet (2)->insert_adhoc (key2.prv);
	{
		auto transaction (system.wallet (2)->wallets.tx_begin_write ());
		system.wallet (2)->store.representative_set (transaction, key2.pub);
	}
	system.deadline_set (30s);
	auto block0 (system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key1.pub, btcnew::genesis_amount / 3));
	ASSERT_NE (nullptr, block0);
	while (node0.balance (key1.pub) == 0)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	auto block1 (system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, btcnew::genesis_amount / 3));
	ASSERT_NE (nullptr, block1);
	while (node0.balance (key2.pub) == 0)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	btcnew::keypair key3;
	btcnew::keypair key4;
	auto block2 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, node0.latest (btcnew::test_genesis_key.pub), key3.pub, node0.balance (btcnew::test_genesis_key.pub), 0, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	auto block3 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, node0.latest (btcnew::test_genesis_key.pub), key4.pub, node0.balance (btcnew::test_genesis_key.pub), 0, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node0.work_generate_blocking (*block2);
	node0.work_generate_blocking (*block3);
	node0.process_active (block2);
	node1.process_active (block2);
	node2.process_active (block3);
	auto done (false);
	// Extend deadline; we must finish within a total of 100 seconds
	system.deadline_set (70s);
	while (!done)
	{
		done |= node0.latest (btcnew::test_genesis_key.pub) == block2->hash () && node1.latest (btcnew::test_genesis_key.pub) == block2->hash () && node2.latest (btcnew::test_genesis_key.pub) == block2->hash ();
		done |= node0.latest (btcnew::test_genesis_key.pub) == block3->hash () && node1.latest (btcnew::test_genesis_key.pub) == block3->hash () && node2.latest (btcnew::test_genesis_key.pub) == block3->hash ();
		ASSERT_NO_ERROR (system.poll ());
	}
}

// Sometimes hangs on the bootstrap_initiator.bootstrap call
TEST (node, DISABLED_fork_stale)
{
	btcnew::system system1 (24000, 1);
	system1.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	btcnew::system system2 (24001, 1);
	auto & node1 (*system1.nodes[0]);
	auto & node2 (*system2.nodes[0]);
	node2.bootstrap_initiator.bootstrap (node1.network.endpoint ());
	auto channel (std::make_shared<btcnew::transport::channel_udp> (node2.network.udp_channels, node1.network.endpoint (), node2.network_params.protocol.protocol_version));
	node2.rep_crawler.response (channel, btcnew::test_genesis_key.pub, btcnew::genesis_amount);
	btcnew::genesis genesis;
	btcnew::keypair key1;
	btcnew::keypair key2;
	auto send3 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - btcnew::Mbtcnew_ratio, key1.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send3);
	node1.process_active (send3);
	system2.deadline_set (10s);
	while (node2.block (send3->hash ()) == nullptr)
	{
		system1.poll ();
		ASSERT_NO_ERROR (system2.poll ());
	}
	auto send1 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send3->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 2 * btcnew::Mbtcnew_ratio, key1.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send1);
	auto send2 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send3->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 2 * btcnew::Mbtcnew_ratio, key2.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send2);
	{
		auto transaction1 (node1.store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, node1.ledger.process (transaction1, *send1).code);
		auto transaction2 (node2.store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, node2.ledger.process (transaction2, *send2).code);
	}
	node1.process_active (send1);
	node1.process_active (send2);
	node2.process_active (send1);
	node2.process_active (send2);
	node2.bootstrap_initiator.bootstrap (node1.network.endpoint ());
	while (node2.block (send1->hash ()) == nullptr)
	{
		system1.poll ();
		ASSERT_NO_ERROR (system2.poll ());
	}
}

TEST (node, broadcast_elected)
{
	std::vector<btcnew::transport::transport_type> types{ btcnew::transport::transport_type::tcp, btcnew::transport::transport_type::udp };
	for (auto & type : types)
	{
		btcnew::system system (24000, 3, type);
		auto node0 (system.nodes[0]);
		auto node1 (system.nodes[1]);
		auto node2 (system.nodes[2]);
		btcnew::keypair rep_big;
		btcnew::keypair rep_small;
		btcnew::keypair rep_other;
		//std::cerr << "Big: " << rep_big.pub.to_account () << std::endl;
		//std::cerr << "Small: " << rep_small.pub.to_account () << std::endl;
		//std::cerr << "Other: " << rep_other.pub.to_account () << std::endl;
		{
			auto transaction0 (node0->store.tx_begin_write ());
			auto transaction1 (node1->store.tx_begin_write ());
			auto transaction2 (node2->store.tx_begin_write ());
			btcnew::send_block fund_big (node0->ledger.latest (transaction0, btcnew::test_genesis_key.pub), rep_big.pub, btcnew::Gbtcnew_ratio * 5, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0);
			btcnew::open_block open_big (fund_big.hash (), rep_big.pub, rep_big.pub, rep_big.prv, rep_big.pub, 0);
			btcnew::send_block fund_small (fund_big.hash (), rep_small.pub, btcnew::Gbtcnew_ratio * 2, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0);
			btcnew::open_block open_small (fund_small.hash (), rep_small.pub, rep_small.pub, rep_small.prv, rep_small.pub, 0);
			btcnew::send_block fund_other (fund_small.hash (), rep_other.pub, btcnew::Gbtcnew_ratio * 1, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0);
			btcnew::open_block open_other (fund_other.hash (), rep_other.pub, rep_other.pub, rep_other.prv, rep_other.pub, 0);
			node0->work_generate_blocking (fund_big);
			node0->work_generate_blocking (open_big);
			node0->work_generate_blocking (fund_small);
			node0->work_generate_blocking (open_small);
			node0->work_generate_blocking (fund_other);
			node0->work_generate_blocking (open_other);
			ASSERT_EQ (btcnew::process_result::progress, node0->ledger.process (transaction0, fund_big).code);
			ASSERT_EQ (btcnew::process_result::progress, node1->ledger.process (transaction1, fund_big).code);
			ASSERT_EQ (btcnew::process_result::progress, node2->ledger.process (transaction2, fund_big).code);
			ASSERT_EQ (btcnew::process_result::progress, node0->ledger.process (transaction0, open_big).code);
			ASSERT_EQ (btcnew::process_result::progress, node1->ledger.process (transaction1, open_big).code);
			ASSERT_EQ (btcnew::process_result::progress, node2->ledger.process (transaction2, open_big).code);
			ASSERT_EQ (btcnew::process_result::progress, node0->ledger.process (transaction0, fund_small).code);
			ASSERT_EQ (btcnew::process_result::progress, node1->ledger.process (transaction1, fund_small).code);
			ASSERT_EQ (btcnew::process_result::progress, node2->ledger.process (transaction2, fund_small).code);
			ASSERT_EQ (btcnew::process_result::progress, node0->ledger.process (transaction0, open_small).code);
			ASSERT_EQ (btcnew::process_result::progress, node1->ledger.process (transaction1, open_small).code);
			ASSERT_EQ (btcnew::process_result::progress, node2->ledger.process (transaction2, open_small).code);
			ASSERT_EQ (btcnew::process_result::progress, node0->ledger.process (transaction0, fund_other).code);
			ASSERT_EQ (btcnew::process_result::progress, node1->ledger.process (transaction1, fund_other).code);
			ASSERT_EQ (btcnew::process_result::progress, node2->ledger.process (transaction2, fund_other).code);
			ASSERT_EQ (btcnew::process_result::progress, node0->ledger.process (transaction0, open_other).code);
			ASSERT_EQ (btcnew::process_result::progress, node1->ledger.process (transaction1, open_other).code);
			ASSERT_EQ (btcnew::process_result::progress, node2->ledger.process (transaction2, open_other).code);
		}
		system.wallet (0)->insert_adhoc (rep_big.prv);
		system.wallet (1)->insert_adhoc (rep_small.prv);
		system.wallet (2)->insert_adhoc (rep_other.prv);
		auto fork0 (std::make_shared<btcnew::send_block> (node2->latest (btcnew::test_genesis_key.pub), rep_small.pub, 0, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
		node0->work_generate_blocking (*fork0);
		node0->process_active (fork0);
		node1->process_active (fork0);
		auto fork1 (std::make_shared<btcnew::send_block> (node2->latest (btcnew::test_genesis_key.pub), rep_big.pub, 0, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
		node0->work_generate_blocking (*fork1);
		system.wallet (2)->insert_adhoc (rep_small.prv);
		node2->process_active (fork1);
		//std::cerr << "fork0: " << fork_hash.to_string () << std::endl;
		//std::cerr << "fork1: " << fork1.hash ().to_string () << std::endl;
		system.deadline_set (10s);
		while (!node0->ledger.block_exists (fork0->hash ()) || !node1->ledger.block_exists (fork0->hash ()))
		{
			ASSERT_NO_ERROR (system.poll ());
		}
		system.deadline_set (50s);
		while (!node2->ledger.block_exists (fork0->hash ()))
		{
			auto ec = system.poll ();
			ASSERT_TRUE (node0->ledger.block_exists (fork0->hash ()));
			ASSERT_TRUE (node1->ledger.block_exists (fork0->hash ()));
			ASSERT_NO_ERROR (ec);
		}
		system.deadline_set (5s);
		while (node1->stats.count (btcnew::stat::type::observer, btcnew::stat::detail::observer_confirmation_inactive, btcnew::stat::dir::out) == 0)
		{
			ASSERT_NO_ERROR (system.poll ());
		}
	}
}

TEST (node, rep_self_vote)
{
	btcnew::system system;
	btcnew::node_config node_config (24000, system.logging);
	node_config.online_weight_minimum = std::numeric_limits<btcnew::uint128_t>::max ();
	node_config.frontiers_confirmation = btcnew::frontiers_confirmation_mode::disabled;
	auto node0 = system.add_node (node_config);
	btcnew::keypair rep_big;
	{
		auto transaction0 (node0->store.tx_begin_write ());
		btcnew::send_block fund_big (node0->ledger.latest (transaction0, btcnew::test_genesis_key.pub), rep_big.pub, btcnew::uint128_t ("0xb0000000000000000000000000000000"), btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0);
		btcnew::open_block open_big (fund_big.hash (), rep_big.pub, rep_big.pub, rep_big.prv, rep_big.pub, 0);
		node0->work_generate_blocking (fund_big);
		node0->work_generate_blocking (open_big);
		ASSERT_EQ (btcnew::process_result::progress, node0->ledger.process (transaction0, fund_big).code);
		ASSERT_EQ (btcnew::process_result::progress, node0->ledger.process (transaction0, open_big).code);
	}
	system.wallet (0)->insert_adhoc (rep_big.prv);
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	ASSERT_EQ (system.wallet (0)->wallets.reps_count, 2);
	auto block0 (std::make_shared<btcnew::send_block> (node0->latest (btcnew::test_genesis_key.pub), rep_big.pub, btcnew::uint128_t ("0x60000000000000000000000000000000"), btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node0->work_generate_blocking (*block0);
	ASSERT_EQ (btcnew::process_result::progress, node0->process (*block0).code);
	auto & active (node0->active);
	active.start (block0);
	btcnew::unique_lock<std::mutex> lock (active.mutex);
	auto existing (active.roots.find (block0->qualified_root ()));
	ASSERT_NE (active.roots.end (), existing);
	auto election (existing->election);
	lock.unlock ();
	system.deadline_set (1s);
	// Wait until representatives are activated & make vote
	while (election->last_votes_size () != 3)
	{
		lock.lock ();
		auto transaction (node0->store.tx_begin_read ());
		election->compute_rep_votes (transaction);
		lock.unlock ();
		node0->vote_processor.flush ();
		ASSERT_NO_ERROR (system.poll ());
	}
	lock.lock ();
	auto & rep_votes (election->last_votes);
	ASSERT_NE (rep_votes.end (), rep_votes.find (btcnew::test_genesis_key.pub));
	ASSERT_NE (rep_votes.end (), rep_votes.find (rep_big.pub));
}

// Bootstrapping shouldn't republish the blocks to the network.
TEST (node, DISABLED_bootstrap_no_publish)
{
	btcnew::system system0 (24000, 1);
	btcnew::system system1 (24001, 1);
	auto node0 (system0.nodes[0]);
	auto node1 (system1.nodes[0]);
	btcnew::keypair key0;
	// node0 knows about send0 but node1 doesn't.
	btcnew::send_block send0 (system0.nodes[0]->latest (btcnew::test_genesis_key.pub), key0.pub, 500, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0);
	{
		auto transaction (node0->store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, system0.nodes[0]->ledger.process (transaction, send0).code);
	}
	ASSERT_FALSE (node1->bootstrap_initiator.in_progress ());
	node1->bootstrap_initiator.bootstrap (node0->network.endpoint ());
	ASSERT_TRUE (node1->active.empty ());
	system1.deadline_set (10s);
	while (node1->block (send0.hash ()) == nullptr)
	{
		// Poll until the TCP connection is torn down and in_progress goes false
		system0.poll ();
		auto ec = system1.poll ();
		// There should never be an active transaction because the only activity is bootstrapping 1 block which shouldn't be publishing.
		ASSERT_TRUE (node1->active.empty ());
		ASSERT_NO_ERROR (ec);
	}
}

// Check that an outgoing bootstrap request can push blocks
TEST (node, bootstrap_bulk_push)
{
	btcnew::system system0 (24000, 1);
	btcnew::system system1 (24001, 1);
	auto node0 (system0.nodes[0]);
	auto node1 (system1.nodes[0]);
	btcnew::keypair key0;
	// node0 knows about send0 but node1 doesn't.
	btcnew::send_block send0 (system0.nodes[0]->latest (btcnew::test_genesis_key.pub), key0.pub, 500, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0);
	node0->work_generate_blocking (send0);
	{
		auto transaction (node0->store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, system0.nodes[0]->ledger.process (transaction, send0).code);
	}
	ASSERT_FALSE (node0->bootstrap_initiator.in_progress ());
	ASSERT_FALSE (node1->bootstrap_initiator.in_progress ());
	ASSERT_TRUE (node1->active.empty ());
	node0->bootstrap_initiator.bootstrap (node1->network.endpoint (), false);
	system1.deadline_set (10s);
	while (node1->block (send0.hash ()) == nullptr)
	{
		ASSERT_NO_ERROR (system0.poll ());
		ASSERT_NO_ERROR (system1.poll ());
	}
	// since this uses bulk_push, the new block should be republished
	ASSERT_FALSE (node1->active.empty ());
}

// Bootstrapping a forked open block should succeed.
TEST (node, bootstrap_fork_open)
{
	btcnew::system system0;
	btcnew::node_config node_config (24000, system0.logging);
	node_config.frontiers_confirmation = btcnew::frontiers_confirmation_mode::disabled;
	auto node0 = system0.add_node (node_config);
	node_config.peering_port = 24001;
	auto node1 = system0.add_node (node_config);
	system0.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	btcnew::keypair key0;
	btcnew::send_block send0 (system0.nodes[0]->latest (btcnew::test_genesis_key.pub), key0.pub, btcnew::genesis_amount - 500, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0);
	btcnew::open_block open0 (send0.hash (), 1, key0.pub, key0.prv, key0.pub, 0);
	btcnew::open_block open1 (send0.hash (), 2, key0.pub, key0.prv, key0.pub, 0);
	node0->work_generate_blocking (send0);
	node0->work_generate_blocking (open0);
	node0->work_generate_blocking (open1);
	{
		auto transaction0 (node0->store.tx_begin_write ());
		auto transaction1 (node1->store.tx_begin_write ());
		// Both know about send0
		ASSERT_EQ (btcnew::process_result::progress, node0->ledger.process (transaction0, send0).code);
		ASSERT_EQ (btcnew::process_result::progress, node1->ledger.process (transaction1, send0).code);
		// They disagree about open0/open1
		ASSERT_EQ (btcnew::process_result::progress, node0->ledger.process (transaction0, open0).code);
		ASSERT_EQ (btcnew::process_result::progress, node1->ledger.process (transaction1, open1).code);
	}
	ASSERT_FALSE (node1->bootstrap_initiator.in_progress ());
	node1->bootstrap_initiator.bootstrap (node0->network.endpoint ());
	ASSERT_TRUE (node1->active.empty ());
	system0.deadline_set (10s);
	while (node1->ledger.block_exists (open1.hash ()))
	{
		// Poll until the outvoted block is evicted.
		ASSERT_NO_ERROR (system0.poll ());
	}
}

// Unconfirmed blocks from bootstrap should be confirmed
TEST (node, bootstrap_confirm_frontiers)
{
	btcnew::system system0 (24000, 1);
	btcnew::system system1 (24001, 1);
	auto node0 (system0.nodes[0]);
	auto node1 (system0.nodes[0]);
	system0.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	btcnew::keypair key0;
	// node0 knows about send0 but node1 doesn't.
	btcnew::send_block send0 (node0->latest (btcnew::test_genesis_key.pub), key0.pub, btcnew::genesis_amount - 500, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0);
	node0->work_generate_blocking (send0);
	{
		auto transaction (node0->store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, node0->ledger.process (transaction, send0).code);
	}
	ASSERT_FALSE (node0->bootstrap_initiator.in_progress ());
	ASSERT_FALSE (node1->bootstrap_initiator.in_progress ());
	ASSERT_TRUE (node1->active.empty ());
	node1->bootstrap_initiator.bootstrap (node0->network.endpoint ());
	system1.deadline_set (10s);
	while (node1->block (send0.hash ()) == nullptr)
	{
		ASSERT_NO_ERROR (system0.poll ());
		ASSERT_NO_ERROR (system1.poll ());
	}
	// Wait for election start
	system1.deadline_set (10s);
	while (node1->active.empty ())
	{
		ASSERT_NO_ERROR (system0.poll ());
		ASSERT_NO_ERROR (system1.poll ());
	}
	{
		btcnew::lock_guard<std::mutex> guard (node1->active.mutex);
		auto existing1 (node1->active.blocks.find (send0.hash ()));
		ASSERT_NE (node1->active.blocks.end (), existing1);
	}
	// Wait for confirmation height update
	system1.deadline_set (10s);
	bool done (false);
	while (!done)
	{
		{
			auto transaction (node1->store.tx_begin_read ());
			done = node1->ledger.block_confirmed (transaction, send0.hash ());
		}
		ASSERT_NO_ERROR (system0.poll ());
		ASSERT_NO_ERROR (system1.poll ());
	}
}

// Test that if we create a block that isn't confirmed, we sync.
TEST (node, DISABLED_unconfirmed_send)
{
	btcnew::system system (24000, 2);
	auto & node0 (*system.nodes[0]);
	auto & node1 (*system.nodes[1]);
	auto wallet0 (system.wallet (0));
	auto wallet1 (system.wallet (1));
	btcnew::keypair key0;
	wallet1->insert_adhoc (key0.prv);
	wallet0->insert_adhoc (btcnew::test_genesis_key.prv);
	auto send1 (wallet0->send_action (btcnew::genesis_account, key0.pub, 2 * btcnew::Mbtcnew_ratio));
	system.deadline_set (10s);
	while (node1.balance (key0.pub) != 2 * btcnew::Mbtcnew_ratio || node1.bootstrap_initiator.in_progress ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	auto latest (node1.latest (key0.pub));
	btcnew::state_block send2 (key0.pub, latest, btcnew::genesis_account, btcnew::Mbtcnew_ratio, btcnew::genesis_account, key0.prv, key0.pub, *node0.work_generate_blocking (latest));
	{
		auto transaction (node1.store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, node1.ledger.process (transaction, send2).code);
	}
	auto send3 (wallet1->send_action (key0.pub, btcnew::genesis_account, btcnew::Mbtcnew_ratio));
	system.deadline_set (10s);
	while (node0.balance (btcnew::genesis_account) != btcnew::genesis_amount)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

// Test that nodes can track nodes that have rep weight for priority broadcasting
TEST (node, rep_list)
{
	btcnew::system system (24000, 2);
	auto & node1 (*system.nodes[1]);
	auto wallet0 (system.wallet (0));
	auto wallet1 (system.wallet (1));
	// Node0 has a rep
	wallet0->insert_adhoc (btcnew::test_genesis_key.prv);
	btcnew::keypair key1;
	// Broadcast a confirm so others should know this is a rep node
	wallet0->send_action (btcnew::test_genesis_key.pub, key1.pub, btcnew::Mbtcnew_ratio);
	ASSERT_EQ (0, node1.rep_crawler.representatives (1).size ());
	system.deadline_set (10s);
	auto done (false);
	while (!done)
	{
		auto reps (node1.rep_crawler.representatives (1));
		if (!reps.empty ())
		{
			if (!reps[0].weight.is_zero ())
			{
				done = true;
			}
		}
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (node, rep_weight)
{
	btcnew::system system (24000, 1);
	auto & node (*system.nodes[0]);

	node.network.udp_channels.insert (btcnew::endpoint (boost::asio::ip::address_v6::loopback (), 24001), 0);
	ASSERT_TRUE (node.rep_crawler.representatives (1).empty ());
	btcnew::endpoint endpoint0 (boost::asio::ip::address_v6::loopback (), 24000);
	btcnew::endpoint endpoint1 (boost::asio::ip::address_v6::loopback (), 24002);
	btcnew::endpoint endpoint2 (boost::asio::ip::address_v6::loopback (), 24003);
	auto channel0 (std::make_shared<btcnew::transport::channel_udp> (node.network.udp_channels, endpoint0, node.network_params.protocol.protocol_version));
	auto channel1 (std::make_shared<btcnew::transport::channel_udp> (node.network.udp_channels, endpoint1, node.network_params.protocol.protocol_version));
	auto channel2 (std::make_shared<btcnew::transport::channel_udp> (node.network.udp_channels, endpoint2, node.network_params.protocol.protocol_version));
	btcnew::amount amount100 (100);
	btcnew::amount amount50 (50);
	node.network.udp_channels.insert (endpoint2, node.network_params.protocol.protocol_version);
	node.network.udp_channels.insert (endpoint0, node.network_params.protocol.protocol_version);
	node.network.udp_channels.insert (endpoint1, node.network_params.protocol.protocol_version);
	btcnew::keypair keypair1;
	btcnew::keypair keypair2;
	node.rep_crawler.response (channel0, keypair1.pub, amount100);
	node.rep_crawler.response (channel1, keypair2.pub, amount50);
	ASSERT_EQ (2, node.rep_crawler.representative_count ());
	// Make sure we get the rep with the most weight first
	auto reps (node.rep_crawler.representatives (1));
	ASSERT_EQ (1, reps.size ());
	ASSERT_EQ (100, reps[0].weight.number ());
	ASSERT_EQ (keypair1.pub, reps[0].account);
	ASSERT_EQ (*channel0, reps[0].channel_ref ());
}

TEST (node, rep_remove)
{
	btcnew::system system (24000, 1);
	auto & node (*system.nodes[0]);
	// Add inactive UDP representative channel
	btcnew::endpoint endpoint0 (boost::asio::ip::address_v6::loopback (), 24001);
	auto channel0 (std::make_shared<btcnew::transport::channel_udp> (node.network.udp_channels, endpoint0, node.network_params.protocol.protocol_version));
	btcnew::amount amount100 (100);
	node.network.udp_channels.insert (endpoint0, node.network_params.protocol.protocol_version);
	btcnew::keypair keypair1;
	node.rep_crawler.response (channel0, keypair1.pub, amount100);
	ASSERT_EQ (1, node.rep_crawler.representative_count ());
	auto reps (node.rep_crawler.representatives (1));
	ASSERT_EQ (1, reps.size ());
	ASSERT_EQ (100, reps[0].weight.number ());
	ASSERT_EQ (keypair1.pub, reps[0].account);
	ASSERT_EQ (*channel0, reps[0].channel_ref ());
	// Add working representative
	auto node1 = system.add_node (btcnew::node_config (24002, system.logging));
	system.wallet (1)->insert_adhoc (btcnew::test_genesis_key.prv);
	auto channel1 (node.network.find_channel (node1->network.endpoint ()));
	ASSERT_NE (nullptr, channel1);
	node.rep_crawler.response (channel1, btcnew::test_genesis_key.pub, btcnew::genesis_amount);
	ASSERT_EQ (2, node.rep_crawler.representative_count ());
	// Add inactive TCP representative channel
	auto node2 (std::make_shared<btcnew::node> (system.io_ctx, btcnew::unique_path (), system.alarm, btcnew::node_config (24003, system.logging), system.work));
	std::atomic<bool> done{ false };
	std::weak_ptr<btcnew::node> node_w (node.shared ());
	node.network.tcp_channels.start_tcp (node2->network.endpoint (), [node_w, &done] (std::shared_ptr<btcnew::transport::channel> channel2) {
		if (auto node_l = node_w.lock ())
		{
			btcnew::keypair keypair2;
			node_l->rep_crawler.response (channel2, keypair2.pub, btcnew::Gbtcnew_ratio);
			ASSERT_EQ (3, node_l->rep_crawler.representative_count ());
			done = true;
		}
	});
	system.deadline_set (10s);
	while (!done)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	node2->stop ();
	// Remove inactive representatives
	system.deadline_set (10s);
	while (node.rep_crawler.representative_count () != 1)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	reps = node.rep_crawler.representatives (1);
	ASSERT_EQ (btcnew::test_genesis_key.pub, reps[0].account);
	ASSERT_EQ (1, node.network.size ());
	auto list (node.network.list (1));
	ASSERT_EQ (node1->network.endpoint (), list[0]->get_endpoint ());
}

// Test that nodes can disable representative voting
TEST (node, no_voting)
{
	btcnew::system system (24000, 1);
	auto & node0 (*system.nodes[0]);
	btcnew::node_config node_config (24001, system.logging);
	node_config.enable_voting = false;
	system.add_node (node_config);

	auto wallet0 (system.wallet (0));
	auto wallet1 (system.wallet (1));
	// Node1 has a rep
	wallet1->insert_adhoc (btcnew::test_genesis_key.prv);
	btcnew::keypair key1;
	wallet1->insert_adhoc (key1.prv);
	// Broadcast a confirm so others should know this is a rep node
	wallet1->send_action (btcnew::test_genesis_key.pub, key1.pub, btcnew::Mbtcnew_ratio);
	system.deadline_set (10s);
	while (!node0.active.empty ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_EQ (0, node0.stats.count (btcnew::stat::type::message, btcnew::stat::detail::confirm_ack, btcnew::stat::dir::in));
}

TEST (node, send_callback)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key2;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key2.prv);
	system.nodes[0]->config.callback_address = "localhost";
	system.nodes[0]->config.callback_port = 8010;
	system.nodes[0]->config.callback_target = "/";
	ASSERT_NE (nullptr, system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	system.deadline_set (10s);
	while (system.nodes[0]->balance (key2.pub).is_zero ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_EQ (std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number (), system.nodes[0]->balance (btcnew::test_genesis_key.pub));
}

// Check that votes get replayed back to nodes if they sent an old sequence number.
// This helps representatives continue from their last sequence number if their node is reinitialized and the old sequence number is lost
TEST (node, vote_replay)
{
	btcnew::system system (24000, 2);
	btcnew::keypair key;
	auto open (std::make_shared<btcnew::open_block> (0, 1, key.pub, key.prv, key.pub, 0));
	system.nodes[0]->work_generate_blocking (*open);
	for (auto i (0); i < 11000; ++i)
	{
		auto transaction (system.nodes[1]->store.tx_begin_read ());
		auto vote (system.nodes[1]->store.vote_generate (transaction, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, open));
	}
	{
		auto transaction (system.nodes[0]->store.tx_begin_read ());
		btcnew::lock_guard<std::mutex> lock (system.nodes[0]->store.get_cache_mutex ());
		auto vote (system.nodes[0]->store.vote_current (transaction, btcnew::test_genesis_key.pub));
		ASSERT_EQ (nullptr, vote);
	}
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	auto block (system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key.pub, btcnew::Gbtcnew_ratio));
	ASSERT_NE (nullptr, block);
	auto done (false);
	system.deadline_set (20s);
	while (!done)
	{
		auto ec = system.poll ();
		auto transaction (system.nodes[0]->store.tx_begin_read ());
		btcnew::lock_guard<std::mutex> lock (system.nodes[0]->store.get_cache_mutex ());
		auto vote (system.nodes[0]->store.vote_current (transaction, btcnew::test_genesis_key.pub));
		done = vote && (vote->sequence >= 10000);
		ASSERT_NO_ERROR (ec);
	}
}

TEST (node, balance_observer)
{
	btcnew::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	std::atomic<int> balances (0);
	btcnew::keypair key;
	node1.observers.account_balance.add ([&key, &balances] (btcnew::account const & account_a, bool is_pending) {
		if (key.pub == account_a && is_pending)
		{
			balances++;
		}
		else if (btcnew::test_genesis_key.pub == account_a && !is_pending)
		{
			balances++;
		}
	});
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key.pub, 1);
	system.deadline_set (10s);
	auto done (false);
	while (!done)
	{
		auto ec = system.poll ();
		done = balances.load () == 2;
		ASSERT_NO_ERROR (ec);
	}
}

// ASSERT_NE (nullptr, attempt) sometimes fails
TEST (node, DISABLED_bootstrap_connection_scaling)
{
	btcnew::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	node1.bootstrap_initiator.bootstrap ();
	auto attempt (node1.bootstrap_initiator.current_attempt ());
	ASSERT_NE (nullptr, attempt);
	ASSERT_EQ (34, attempt->target_connections (25000));
	ASSERT_EQ (4, attempt->target_connections (0));
	ASSERT_EQ (64, attempt->target_connections (50000));
	ASSERT_EQ (64, attempt->target_connections (10000000000));
	node1.config.bootstrap_connections = 128;
	ASSERT_EQ (64, attempt->target_connections (0));
	ASSERT_EQ (64, attempt->target_connections (50000));
	node1.config.bootstrap_connections_max = 256;
	ASSERT_EQ (128, attempt->target_connections (0));
	ASSERT_EQ (256, attempt->target_connections (50000));
	node1.config.bootstrap_connections_max = 0;
	ASSERT_EQ (1, attempt->target_connections (0));
	ASSERT_EQ (1, attempt->target_connections (50000));
}

// Test stat counting at both type and detail levels
TEST (node, stat_counting)
{
	btcnew::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	node1.stats.add (btcnew::stat::type::ledger, btcnew::stat::dir::in, 1);
	node1.stats.add (btcnew::stat::type::ledger, btcnew::stat::dir::in, 5);
	node1.stats.inc (btcnew::stat::type::ledger, btcnew::stat::dir::in);
	node1.stats.inc (btcnew::stat::type::ledger, btcnew::stat::detail::send, btcnew::stat::dir::in);
	node1.stats.inc (btcnew::stat::type::ledger, btcnew::stat::detail::send, btcnew::stat::dir::in);
	node1.stats.inc (btcnew::stat::type::ledger, btcnew::stat::detail::receive, btcnew::stat::dir::in);
	ASSERT_EQ (10, node1.stats.count (btcnew::stat::type::ledger, btcnew::stat::dir::in));
	ASSERT_EQ (2, node1.stats.count (btcnew::stat::type::ledger, btcnew::stat::detail::send, btcnew::stat::dir::in));
	ASSERT_EQ (1, node1.stats.count (btcnew::stat::type::ledger, btcnew::stat::detail::receive, btcnew::stat::dir::in));
}

TEST (node, online_reps)
{
	btcnew::system system (24000, 1);
	// 1 sample of minimum weight
	ASSERT_EQ (system.nodes[0]->config.online_weight_minimum, system.nodes[0]->online_reps.online_stake ());
	auto vote (std::make_shared<btcnew::vote> ());
	system.nodes[0]->online_reps.observe (btcnew::test_genesis_key.pub);
	// 1 minimum, 1 maximum
	system.nodes[0]->online_reps.sample ();
	ASSERT_EQ (btcnew::genesis_amount, system.nodes[0]->online_reps.online_stake ());
	// 2 minimum, 1 maximum
	system.nodes[0]->online_reps.sample ();
	ASSERT_EQ (system.nodes[0]->config.online_weight_minimum, system.nodes[0]->online_reps.online_stake ());
}

TEST (node, block_confirm)
{
	std::vector<btcnew::transport::transport_type> types{ btcnew::transport::transport_type::tcp, btcnew::transport::transport_type::udp };
	for (auto & type : types)
	{
		btcnew::system system (24000, 2, type);
		btcnew::genesis genesis;
		btcnew::keypair key;
		system.wallet (1)->insert_adhoc (btcnew::test_genesis_key.prv);
		auto send1 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - btcnew::Gbtcnew_ratio, key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.nodes[0]->work_generate_blocking (genesis.hash ())));
		system.nodes[0]->block_processor.add (send1, btcnew::seconds_since_epoch ());
		system.nodes[1]->block_processor.add (send1, btcnew::seconds_since_epoch ());
		system.deadline_set (std::chrono::seconds (5));
		while (!system.nodes[0]->ledger.block_exists (send1->hash ()) || !system.nodes[1]->ledger.block_exists (send1->hash ()))
		{
			ASSERT_NO_ERROR (system.poll ());
		}
		ASSERT_TRUE (system.nodes[0]->ledger.block_exists (send1->hash ()));
		ASSERT_TRUE (system.nodes[1]->ledger.block_exists (send1->hash ()));
		auto send2 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send1->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - btcnew::Gbtcnew_ratio * 2, key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.nodes[0]->work_generate_blocking (send1->hash ())));
		{
			auto transaction (system.nodes[0]->store.tx_begin_write ());
			ASSERT_EQ (btcnew::process_result::progress, system.nodes[0]->ledger.process (transaction, *send2).code);
		}
		{
			auto transaction (system.nodes[1]->store.tx_begin_write ());
			ASSERT_EQ (btcnew::process_result::progress, system.nodes[1]->ledger.process (transaction, *send2).code);
		}
		system.nodes[0]->block_confirm (send2);
		ASSERT_TRUE (system.nodes[0]->active.list_confirmed ().empty ());
		system.deadline_set (10s);
		while (system.nodes[0]->active.list_confirmed ().empty ())
		{
			ASSERT_NO_ERROR (system.poll ());
		}
	}
}

TEST (node, block_arrival)
{
	btcnew::system system (24000, 1);
	auto & node (*system.nodes[0]);
	ASSERT_EQ (0, node.block_arrival.arrival.size ());
	btcnew::block_hash hash1 (1);
	node.block_arrival.add (hash1);
	ASSERT_EQ (1, node.block_arrival.arrival.size ());
	node.block_arrival.add (hash1);
	ASSERT_EQ (1, node.block_arrival.arrival.size ());
	btcnew::block_hash hash2 (2);
	node.block_arrival.add (hash2);
	ASSERT_EQ (2, node.block_arrival.arrival.size ());
}

TEST (node, block_arrival_size)
{
	btcnew::system system (24000, 1);
	auto & node (*system.nodes[0]);
	auto time (std::chrono::steady_clock::now () - btcnew::block_arrival::arrival_time_min - std::chrono::seconds (5));
	btcnew::block_hash hash (0);
	for (auto i (0); i < btcnew::block_arrival::arrival_size_min * 2; ++i)
	{
		node.block_arrival.arrival.insert (btcnew::block_arrival_info{ time, hash });
		++hash.qwords[0];
	}
	ASSERT_EQ (btcnew::block_arrival::arrival_size_min * 2, node.block_arrival.arrival.size ());
	node.block_arrival.recent (0);
	ASSERT_EQ (btcnew::block_arrival::arrival_size_min, node.block_arrival.arrival.size ());
}

TEST (node, block_arrival_time)
{
	btcnew::system system (24000, 1);
	auto & node (*system.nodes[0]);
	auto time (std::chrono::steady_clock::now ());
	btcnew::block_hash hash (0);
	for (auto i (0); i < btcnew::block_arrival::arrival_size_min * 2; ++i)
	{
		node.block_arrival.arrival.insert (btcnew::block_arrival_info{ time, hash });
		++hash.qwords[0];
	}
	ASSERT_EQ (btcnew::block_arrival::arrival_size_min * 2, node.block_arrival.arrival.size ());
	node.block_arrival.recent (0);
	ASSERT_EQ (btcnew::block_arrival::arrival_size_min * 2, node.block_arrival.arrival.size ());
}

TEST (node, confirm_quorum)
{
	btcnew::system system (24000, 1);
	btcnew::genesis genesis;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	// Put greater than online_weight_minimum in pending so quorum can't be reached
	btcnew::amount new_balance (system.nodes[0]->config.online_weight_minimum.number () - btcnew::Gbtcnew_ratio);
	auto send1 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, new_balance, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.nodes[0]->work_generate_blocking (genesis.hash ())));
	{
		auto transaction (system.nodes[0]->store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, system.nodes[0]->ledger.process (transaction, *send1).code);
	}
	system.wallet (0)->send_action (btcnew::test_genesis_key.pub, btcnew::test_genesis_key.pub, new_balance.number ());
	system.deadline_set (10s);
	while (system.nodes[0]->active.empty ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	auto done (false);
	while (!done)
	{
		ASSERT_FALSE (system.nodes[0]->active.empty ());
		{
			btcnew::lock_guard<std::mutex> guard (system.nodes[0]->active.mutex);
			auto info (system.nodes[0]->active.roots.find (btcnew::qualified_root (send1->hash (), send1->hash ())));
			ASSERT_NE (system.nodes[0]->active.roots.end (), info);
			done = info->election->confirmation_request_count > 2;
		}
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_EQ (0, system.nodes[0]->balance (btcnew::test_genesis_key.pub));
}

TEST (node, local_votes_cache)
{
	btcnew::system system;
	btcnew::node_config node_config (24000, system.logging);
	node_config.frontiers_confirmation = btcnew::frontiers_confirmation_mode::disabled;
	auto & node (*system.add_node (node_config));
	btcnew::genesis genesis;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	auto send1 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *node.work_generate_blocking (genesis.hash ())));
	auto send2 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send1->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 2 * btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *node.work_generate_blocking (send1->hash ())));
	auto send3 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send2->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 3 * btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *node.work_generate_blocking (send2->hash ())));
	{
		auto transaction (node.store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, node.ledger.process (transaction, *send1).code);
		ASSERT_EQ (btcnew::process_result::progress, node.ledger.process (transaction, *send2).code);
	}
	btcnew::confirm_req message1 (send1);
	btcnew::confirm_req message2 (send2);
	auto channel (node.network.udp_channels.create (node.network.endpoint ()));
	for (auto i (0); i < 100; ++i)
	{
		node.network.process_message (message1, channel);
		node.network.process_message (message2, channel);
	}
	{
		btcnew::lock_guard<std::mutex> lock (node.store.get_cache_mutex ());
		auto transaction (node.store.tx_begin_read ());
		auto current_vote (node.store.vote_current (transaction, btcnew::test_genesis_key.pub));
		ASSERT_EQ (current_vote->sequence, 2);
	}
	// Max cache
	{
		auto transaction (node.store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, node.ledger.process (transaction, *send3).code);
	}
	btcnew::confirm_req message3 (send3);
	for (auto i (0); i < 100; ++i)
	{
		node.network.process_message (message3, channel);
	}
	{
		btcnew::lock_guard<std::mutex> lock (node.store.get_cache_mutex ());
		auto transaction (node.store.tx_begin_read ());
		auto current_vote (node.store.vote_current (transaction, btcnew::test_genesis_key.pub));
		ASSERT_EQ (current_vote->sequence, 3);
	}
	ASSERT_TRUE (node.votes_cache.find (send1->hash ()).empty ());
	ASSERT_FALSE (node.votes_cache.find (send2->hash ()).empty ());
	ASSERT_FALSE (node.votes_cache.find (send3->hash ()).empty ());
}

TEST (node, local_votes_cache_generate_new_vote)
{
	btcnew::system system;
	btcnew::node_config node_config (24000, system.logging);
	node_config.frontiers_confirmation = btcnew::frontiers_confirmation_mode::disabled;
	auto & node (*system.add_node (node_config));
	btcnew::genesis genesis;
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	auto send1 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *node.work_generate_blocking (genesis.hash ())));
	auto send2 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send1->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 2 * btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *node.work_generate_blocking (send1->hash ())));
	{
		auto transaction (node.store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, node.ledger.process (transaction, *send1).code);
	}
	// Repsond with cached vote
	btcnew::confirm_req message1 (send1);
	auto channel (node.network.udp_channels.create (node.network.endpoint ()));
	for (auto i (0); i < 100; ++i)
	{
		node.network.process_message (message1, channel);
	}
	auto votes1 (node.votes_cache.find (send1->hash ()));
	ASSERT_EQ (1, votes1.size ());
	ASSERT_EQ (1, votes1[0]->blocks.size ());
	ASSERT_EQ (send1->hash (), boost::get<btcnew::block_hash> (votes1[0]->blocks[0]));
	{
		btcnew::lock_guard<std::mutex> lock (node.store.get_cache_mutex ());
		auto transaction (node.store.tx_begin_read ());
		auto current_vote (node.store.vote_current (transaction, btcnew::test_genesis_key.pub));
		ASSERT_EQ (current_vote->sequence, 1);
		ASSERT_EQ (current_vote, votes1[0]);
	}
	{
		auto transaction (node.store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, node.ledger.process (transaction, *send2).code);
	}
	// Generate new vote for request with 2 hashes (one of hashes is cached)
	std::vector<std::pair<btcnew::block_hash, btcnew::root>> roots_hashes{ std::make_pair (send1->hash (), send1->root ()), std::make_pair (send2->hash (), send2->root ()) };
	btcnew::confirm_req message2 (roots_hashes);
	for (auto i (0); i < 100; ++i)
	{
		node.network.process_message (message2, channel);
	}
	auto votes2 (node.votes_cache.find (send1->hash ()));
	ASSERT_EQ (1, votes2.size ());
	ASSERT_EQ (2, votes2[0]->blocks.size ());
	{
		btcnew::lock_guard<std::mutex> lock (node.store.get_cache_mutex ());
		auto transaction (node.store.tx_begin_read ());
		auto current_vote (node.store.vote_current (transaction, btcnew::test_genesis_key.pub));
		ASSERT_EQ (current_vote->sequence, 2);
		ASSERT_EQ (current_vote, votes2[0]);
	}
	ASSERT_FALSE (node.votes_cache.find (send1->hash ()).empty ());
	ASSERT_FALSE (node.votes_cache.find (send2->hash ()).empty ());
}

TEST (node, vote_republish)
{
	btcnew::system system (24000, 2);
	btcnew::keypair key2;
	system.wallet (1)->insert_adhoc (key2.prv);
	btcnew::genesis genesis;
	auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key2.pub, std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number (), btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	auto send2 (std::make_shared<btcnew::send_block> (genesis.hash (), key2.pub, std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number () * 2, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	system.nodes[0]->process_active (send1);
	system.deadline_set (5s);
	while (!system.nodes[1]->block (send1->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	system.nodes[0]->active.publish (send2);
	auto vote (std::make_shared<btcnew::vote> (btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, 0, send2));
	ASSERT_TRUE (system.nodes[0]->active.active (*send1));
	ASSERT_TRUE (system.nodes[1]->active.active (*send1));
	system.nodes[0]->vote_processor.vote (vote, std::make_shared<btcnew::transport::channel_udp> (system.nodes[0]->network.udp_channels, system.nodes[0]->network.endpoint (), system.nodes[0]->network_params.protocol.protocol_version));
	while (!system.nodes[0]->block (send2->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	while (!system.nodes[1]->block (send2->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_FALSE (system.nodes[0]->block (send1->hash ()));
	ASSERT_FALSE (system.nodes[1]->block (send1->hash ()));
	system.deadline_set (5s);
	while (system.nodes[1]->balance (key2.pub) != system.nodes[0]->config.receive_minimum.number () * 2)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	while (system.nodes[0]->balance (key2.pub) != system.nodes[0]->config.receive_minimum.number () * 2)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (node, vote_by_hash_bundle)
{
	// Keep max_hashes above system to ensure it is kept in scope as votes can be added during system destruction
	std::atomic<size_t> max_hashes{ 0 };
	btcnew::system system (24000, 1);
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	btcnew::keypair key1;
	system.wallet (0)->insert_adhoc (key1.prv);

	system.nodes[0]->observers.vote.add ([&max_hashes] (std::shared_ptr<btcnew::vote> vote_a, std::shared_ptr<btcnew::transport::channel> channel_a) {
		if (vote_a->blocks.size () > max_hashes)
		{
			max_hashes = vote_a->blocks.size ();
		}
	});

	btcnew::genesis genesis;
	for (int i = 1; i <= 200; i++)
	{
		auto send (std::make_shared<btcnew::send_block> (genesis.hash (), key1.pub, std::numeric_limits<btcnew::uint128_t>::max () - (system.nodes[0]->config.receive_minimum.number () * i), btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
		system.nodes[0]->block_confirm (send);
	}

	// Verify that bundling occurs. While reaching 12 should be common on most hardware in release mode,
	// we set this low enough to allow the test to pass on CI/with santitizers.
	system.deadline_set (20s);
	while (max_hashes.load () < 3)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (node, vote_by_hash_republish)
{
	std::vector<btcnew::transport::transport_type> types{ btcnew::transport::transport_type::tcp, btcnew::transport::transport_type::udp };
	for (auto & type : types)
	{
		btcnew::system system (24000, 2, type);
		btcnew::keypair key2;
		system.wallet (1)->insert_adhoc (key2.prv);
		btcnew::genesis genesis;
		auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key2.pub, std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number (), btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
		auto send2 (std::make_shared<btcnew::send_block> (genesis.hash (), key2.pub, std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number () * 2, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
		system.nodes[0]->process_active (send1);
		system.deadline_set (5s);
		while (!system.nodes[1]->block (send1->hash ()))
		{
			ASSERT_NO_ERROR (system.poll ());
		}
		system.nodes[0]->active.publish (send2);
		std::vector<btcnew::block_hash> vote_blocks;
		vote_blocks.push_back (send2->hash ());
		auto vote (std::make_shared<btcnew::vote> (btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, 0, vote_blocks));
		ASSERT_TRUE (system.nodes[0]->active.active (*send1));
		ASSERT_TRUE (system.nodes[1]->active.active (*send1));
		system.nodes[0]->vote_processor.vote (vote, std::make_shared<btcnew::transport::channel_udp> (system.nodes[0]->network.udp_channels, system.nodes[0]->network.endpoint (), system.nodes[0]->network_params.protocol.protocol_version));
		while (!system.nodes[0]->block (send2->hash ()))
		{
			ASSERT_NO_ERROR (system.poll ());
		}
		while (!system.nodes[1]->block (send2->hash ()))
		{
			ASSERT_NO_ERROR (system.poll ());
		}
		ASSERT_FALSE (system.nodes[0]->block (send1->hash ()));
		ASSERT_FALSE (system.nodes[1]->block (send1->hash ()));
		system.deadline_set (5s);
		while (system.nodes[1]->balance (key2.pub) != system.nodes[0]->config.receive_minimum.number () * 2)
		{
			ASSERT_NO_ERROR (system.poll ());
		}
		while (system.nodes[0]->balance (key2.pub) != system.nodes[0]->config.receive_minimum.number () * 2)
		{
			ASSERT_NO_ERROR (system.poll ());
		}
	}
}

TEST (node, vote_by_hash_epoch_block_republish)
{
	btcnew::system system (24000, 2);
	btcnew::keypair key2;
	system.wallet (1)->insert_adhoc (key2.prv);
	btcnew::genesis genesis;
	auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key2.pub, std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number (), btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	auto epoch1 (std::make_shared<btcnew::state_block> (btcnew::genesis_account, genesis.hash (), btcnew::genesis_account, btcnew::genesis_amount, system.nodes[0]->ledger.epoch_link (btcnew::epoch::epoch_1), btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	system.nodes[0]->process_active (send1);
	system.deadline_set (5s);
	while (!system.nodes[1]->block (send1->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	system.nodes[0]->active.publish (epoch1);
	std::vector<btcnew::block_hash> vote_blocks;
	vote_blocks.push_back (epoch1->hash ());
	auto vote (std::make_shared<btcnew::vote> (btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, 0, vote_blocks));
	ASSERT_TRUE (system.nodes[0]->active.active (*send1));
	ASSERT_TRUE (system.nodes[1]->active.active (*send1));
	system.nodes[0]->vote_processor.vote (vote, std::make_shared<btcnew::transport::channel_udp> (system.nodes[0]->network.udp_channels, system.nodes[0]->network.endpoint (), system.nodes[0]->network_params.protocol.protocol_version));
	while (!system.nodes[0]->block (epoch1->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	while (!system.nodes[1]->block (epoch1->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_FALSE (system.nodes[0]->block (send1->hash ()));
	ASSERT_FALSE (system.nodes[1]->block (send1->hash ()));
}

TEST (node, epoch_conflict_confirm)
{
	btcnew::system system;
	btcnew::node_config node_config (24000, system.logging);
	node_config.frontiers_confirmation = btcnew::frontiers_confirmation_mode::disabled;
	auto node0 = system.add_node (node_config);
	node_config.peering_port = 24001;
	auto node1 = system.add_node (node_config);
	btcnew::keypair key;
	btcnew::genesis genesis;
	btcnew::keypair epoch_signer (btcnew::test_genesis_key);
	auto send (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 1, key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	auto open (std::make_shared<btcnew::state_block> (key.pub, 0, key.pub, 1, send->hash (), key.prv, key.pub, *system.work.generate (key.pub)));
	auto change (std::make_shared<btcnew::state_block> (key.pub, open->hash (), key.pub, 1, 0, key.prv, key.pub, *system.work.generate (open->hash ())));
	auto epoch (std::make_shared<btcnew::state_block> (change->root (), 0, 0, 0, node0->ledger.epoch_link (btcnew::epoch::epoch_1), epoch_signer.prv, epoch_signer.pub, *system.work.generate (open->hash ())));
	{
		auto transaction (node0->store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, node0->block_processor.process_one (transaction, send).code);
		ASSERT_EQ (btcnew::process_result::progress, node0->block_processor.process_one (transaction, open).code);
	}
	{
		auto transaction (node1->store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, node1->block_processor.process_one (transaction, send).code);
		ASSERT_EQ (btcnew::process_result::progress, node1->block_processor.process_one (transaction, open).code);
	}
	node0->process_active (change);
	node0->process_active (epoch);
	node0->block_processor.flush ();
	system.deadline_set (5s);
	while (!node0->block (change->hash ()) || !node0->block (epoch->hash ()) || !node1->block (change->hash ()) || !node1->block (epoch->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	system.deadline_set (5s);
	while (node0->active.size () != 2)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	{
		btcnew::lock_guard<std::mutex> lock (node0->active.mutex);
		ASSERT_TRUE (node0->active.blocks.find (change->hash ()) != node0->active.blocks.end ());
		ASSERT_TRUE (node0->active.blocks.find (epoch->hash ()) != node0->active.blocks.end ());
	}
	system.wallet (1)->insert_adhoc (btcnew::test_genesis_key.prv);
	system.deadline_set (5s);
	while (!node0->active.empty ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	{
		auto transaction (node0->store.tx_begin_read ());
		ASSERT_TRUE (node0->ledger.store.block_exists (transaction, change->hash ()));
		ASSERT_TRUE (node0->ledger.store.block_exists (transaction, epoch->hash ()));
	}
}

TEST (node, fork_invalid_block_signature)
{
	btcnew::system system (24000, 2);
	btcnew::keypair key2;
	btcnew::genesis genesis;
	auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key2.pub, std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number (), btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	auto send2 (std::make_shared<btcnew::send_block> (genesis.hash (), key2.pub, std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number () * 2, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	auto send2_corrupt (std::make_shared<btcnew::send_block> (*send2));
	send2_corrupt->signature = btcnew::signature (123);
	system.nodes[0]->process_active (send1);
	system.deadline_set (5s);
	while (!system.nodes[0]->block (send1->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	auto vote (std::make_shared<btcnew::vote> (btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, 0, send2));
	auto vote_corrupt (std::make_shared<btcnew::vote> (btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, 0, send2_corrupt));
	system.nodes[1]->network.flood_vote (vote_corrupt);
	ASSERT_NO_ERROR (system.poll ());
	system.nodes[1]->network.flood_vote (vote);
	while (system.nodes[0]->block (send1->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	while (!system.nodes[0]->block (send2->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_EQ (system.nodes[0]->block (send2->hash ())->block_signature (), send2->block_signature ());
}

TEST (node, fork_invalid_block_signature_vote_by_hash)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key2;
	btcnew::genesis genesis;
	auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key2.pub, std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number (), btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	auto send2 (std::make_shared<btcnew::send_block> (genesis.hash (), key2.pub, std::numeric_limits<btcnew::uint128_t>::max () - system.nodes[0]->config.receive_minimum.number () * 2, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	auto send2_corrupt (std::make_shared<btcnew::send_block> (*send2));
	send2_corrupt->signature = btcnew::signature (123);
	system.nodes[0]->process_active (send1);
	system.deadline_set (5s);
	while (!system.nodes[0]->block (send1->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	system.nodes[0]->active.publish (send2_corrupt);
	ASSERT_NO_ERROR (system.poll ());
	system.nodes[0]->active.publish (send2);
	std::vector<btcnew::block_hash> vote_blocks;
	vote_blocks.push_back (send2->hash ());
	auto vote (std::make_shared<btcnew::vote> (btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, 0, vote_blocks));
	{
		auto transaction (system.nodes[0]->store.tx_begin_read ());
		btcnew::unique_lock<std::mutex> lock (system.nodes[0]->active.mutex);
		system.nodes[0]->vote_processor.vote_blocking (transaction, vote, std::make_shared<btcnew::transport::channel_udp> (system.nodes[0]->network.udp_channels, system.nodes[0]->network.endpoint (), system.nodes[0]->network_params.protocol.protocol_version));
	}
	while (system.nodes[0]->block (send1->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	while (!system.nodes[0]->block (send2->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_EQ (system.nodes[0]->block (send2->hash ())->block_signature (), send2->block_signature ());
}

TEST (node, block_processor_signatures)
{
	btcnew::system system0 (24000, 1);
	auto & node1 (*system0.nodes[0]);
	system0.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	btcnew::block_hash latest (system0.nodes[0]->latest (btcnew::test_genesis_key.pub));
	btcnew::keypair key1;
	auto send1 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, latest, btcnew::test_genesis_key.pub, btcnew::genesis_amount - btcnew::Gbtcnew_ratio, key1.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send1);
	btcnew::keypair key2;
	auto send2 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send1->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 2 * btcnew::Gbtcnew_ratio, key2.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send2);
	btcnew::keypair key3;
	auto send3 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send2->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 3 * btcnew::Gbtcnew_ratio, key3.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send3);
	// Invalid signature bit
	auto send4 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send3->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 4 * btcnew::Gbtcnew_ratio, key3.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send4);
	send4->signature.bytes[32] ^= 0x1;
	// Invalid signature bit (force)
	auto send5 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send3->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 5 * btcnew::Gbtcnew_ratio, key3.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send5);
	send5->signature.bytes[31] ^= 0x1;
	// Invalid signature to unchecked
	{
		auto transaction (node1.store.tx_begin_write ());
		node1.store.unchecked_put (transaction, send5->previous (), send5);
	}
	auto receive1 (std::make_shared<btcnew::state_block> (key1.pub, 0, btcnew::test_genesis_key.pub, btcnew::Gbtcnew_ratio, send1->hash (), key1.prv, key1.pub, 0));
	node1.work_generate_blocking (*receive1);
	auto receive2 (std::make_shared<btcnew::state_block> (key2.pub, 0, btcnew::test_genesis_key.pub, btcnew::Gbtcnew_ratio, send2->hash (), key2.prv, key2.pub, 0));
	node1.work_generate_blocking (*receive2);
	// Invalid private key
	auto receive3 (std::make_shared<btcnew::state_block> (key3.pub, 0, btcnew::test_genesis_key.pub, btcnew::Gbtcnew_ratio, send3->hash (), key2.prv, key3.pub, 0));
	node1.work_generate_blocking (*receive3);
	node1.process_active (send1);
	node1.process_active (send2);
	node1.process_active (send3);
	node1.process_active (send4);
	node1.process_active (receive1);
	node1.process_active (receive2);
	node1.process_active (receive3);
	node1.block_processor.flush ();
	node1.block_processor.force (send5);
	node1.block_processor.flush ();
	auto transaction (node1.store.tx_begin_read ());
	ASSERT_TRUE (node1.store.block_exists (transaction, send1->hash ()));
	ASSERT_TRUE (node1.store.block_exists (transaction, send2->hash ()));
	ASSERT_TRUE (node1.store.block_exists (transaction, send3->hash ()));
	ASSERT_FALSE (node1.store.block_exists (transaction, send4->hash ()));
	ASSERT_FALSE (node1.store.block_exists (transaction, send5->hash ()));
	ASSERT_TRUE (node1.store.block_exists (transaction, receive1->hash ()));
	ASSERT_TRUE (node1.store.block_exists (transaction, receive2->hash ()));
	ASSERT_FALSE (node1.store.block_exists (transaction, receive3->hash ()));
}

/*
 *  State blocks go through a different signature path, ensure invalidly signed state blocks are rejected
 */
TEST (node, block_processor_reject_state)
{
	btcnew::system system (24000, 1);
	auto & node (*system.nodes[0]);
	btcnew::genesis genesis;
	auto send1 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node.work_generate_blocking (*send1);
	send1->signature.bytes[0] ^= 1;
	ASSERT_FALSE (node.ledger.block_exists (send1->hash ()));
	node.process_active (send1);
	node.block_processor.flush ();
	ASSERT_FALSE (node.ledger.block_exists (send1->hash ()));
	auto send2 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 2 * btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node.work_generate_blocking (*send2);
	node.process_active (send2);
	node.block_processor.flush ();
	ASSERT_TRUE (node.ledger.block_exists (send2->hash ()));
}

TEST (node, block_processor_reject_rolled_back)
{
	btcnew::system system;
	btcnew::node_config node_config (24000, system.logging);
	node_config.frontiers_confirmation = btcnew::frontiers_confirmation_mode::disabled;
	auto & node = *system.add_node (node_config);
	btcnew::genesis genesis;
	auto send1 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node.work_generate_blocking (*send1);
	node.block_processor.add (send1);
	node.block_processor.flush ();
	ASSERT_TRUE (node.ledger.block_exists (send1->hash ()));
	auto send2 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 2 * btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node.work_generate_blocking (*send2);
	// Force block send2 & rolling back block send1
	node.block_processor.force (send2);
	node.block_processor.flush ();
	ASSERT_FALSE (node.ledger.block_exists (send1->hash ()));
	ASSERT_TRUE (node.ledger.block_exists (send2->hash ()));
	ASSERT_TRUE (node.active.empty ());
	// Block send1 cannot be processed & start fork resolution election
	node.block_processor.add (send1);
	node.block_processor.flush ();
	ASSERT_FALSE (node.ledger.block_exists (send1->hash ()));
	ASSERT_TRUE (node.active.empty ());
}

TEST (node, block_processor_full)
{
	btcnew::system system;
	btcnew::node_flags node_flags;
	node_flags.block_processor_full_size = 2;
	auto & node = *system.add_node (btcnew::node_config (24000, system.logging), node_flags);
	btcnew::genesis genesis;
	auto send1 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node.work_generate_blocking (*send1);
	auto send2 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send1->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 2 * btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node.work_generate_blocking (*send2);
	auto send3 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send2->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 3 * btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node.work_generate_blocking (*send3);
	// The write guard prevents block processor doing any writes
	auto write_guard = node.write_database_queue.wait (btcnew::writer::confirmation_height);
	node.block_processor.add (send1);
	ASSERT_FALSE (node.block_processor.full ());
	node.block_processor.add (send2);
	ASSERT_FALSE (node.block_processor.full ());
	node.block_processor.add (send3);
	// Block processor may be not full during state blocks signatures verification
	system.deadline_set (2s);
	while (!node.block_processor.full ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (node, block_processor_half_full)
{
	btcnew::system system;
	btcnew::node_flags node_flags;
	node_flags.block_processor_full_size = 4;
	auto & node = *system.add_node (btcnew::node_config (24000, system.logging), node_flags);
	btcnew::genesis genesis;
	auto send1 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node.work_generate_blocking (*send1);
	auto send2 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send1->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 2 * btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node.work_generate_blocking (*send2);
	auto send3 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send2->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 3 * btcnew::Gbtcnew_ratio, btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node.work_generate_blocking (*send3);
	// The write guard prevents block processor doing any writes
	auto write_guard = node.write_database_queue.wait (btcnew::writer::confirmation_height);
	node.block_processor.add (send1);
	ASSERT_FALSE (node.block_processor.half_full ());
	node.block_processor.add (send2);
	ASSERT_FALSE (node.block_processor.half_full ());
	node.block_processor.add (send3);
	// Block processor may be not half_full during state blocks signatures verification
	system.deadline_set (2s);
	while (!node.block_processor.half_full ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	ASSERT_FALSE (node.block_processor.full ());
}

TEST (node, confirm_back)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key;
	auto & node (*system.nodes[0]);
	btcnew::genesis genesis;
	auto genesis_start_balance (node.balance (btcnew::test_genesis_key.pub));
	auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key.pub, genesis_start_balance - 1, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	auto open (std::make_shared<btcnew::state_block> (key.pub, 0, key.pub, 1, send1->hash (), key.prv, key.pub, *system.work.generate (key.pub)));
	auto send2 (std::make_shared<btcnew::state_block> (key.pub, open->hash (), key.pub, 0, btcnew::test_genesis_key.pub, key.prv, key.pub, *system.work.generate (open->hash ())));
	node.process_active (send1);
	node.process_active (open);
	node.process_active (send2);
	node.block_processor.flush ();
	ASSERT_EQ (3, node.active.size ());
	std::vector<btcnew::block_hash> vote_blocks;
	vote_blocks.push_back (send2->hash ());
	auto vote (std::make_shared<btcnew::vote> (btcnew::test_genesis_key.pub, btcnew::test_genesis_key.prv, 0, vote_blocks));
	{
		auto transaction (node.store.tx_begin_read ());
		btcnew::unique_lock<std::mutex> lock (node.active.mutex);
		node.vote_processor.vote_blocking (transaction, vote, std::make_shared<btcnew::transport::channel_udp> (node.network.udp_channels, node.network.endpoint (), node.network_params.protocol.protocol_version));
	}
	system.deadline_set (10s);
	while (!node.active.empty ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (node, peers)
{
	btcnew::system system (24000, 1);
	ASSERT_TRUE (system.nodes.front ()->network.empty ());

	auto node (std::make_shared<btcnew::node> (system.io_ctx, 24001, btcnew::unique_path (), system.alarm, system.logging, system.work));
	system.nodes.push_back (node);

	auto endpoint = system.nodes.front ()->network.endpoint ();
	btcnew::endpoint_key endpoint_key{ endpoint.address ().to_v6 ().to_bytes (), endpoint.port () };
	auto & store = system.nodes.back ()->store;
	{
		// Add a peer to the database
		auto transaction (store.tx_begin_write ());
		store.peer_put (transaction, endpoint_key);

		// Add a peer which is not contactable
		store.peer_put (transaction, btcnew::endpoint_key{ boost::asio::ip::address_v6::any ().to_bytes (), 55555 });
	}

	node->start ();
	system.deadline_set (10s);
	while (system.nodes.back ()->network.empty () || system.nodes.front ()->network.empty ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	// Wait to finish TCP node ID handshakes
	system.deadline_set (10s);
	while (system.nodes[0]->bootstrap.realtime_count == 0 || system.nodes[1]->bootstrap.realtime_count == 0)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	// Confirm that the peers match with the endpoints we are expecting
	ASSERT_EQ (1, system.nodes.front ()->network.size ());
	auto list1 (system.nodes[0]->network.list (2));
	ASSERT_EQ (system.nodes[1]->network.endpoint (), list1[0]->get_endpoint ());
	ASSERT_EQ (btcnew::transport::transport_type::tcp, list1[0]->get_type ());
	ASSERT_EQ (1, node->network.size ());
	auto list2 (system.nodes[1]->network.list (2));
	ASSERT_EQ (system.nodes[0]->network.endpoint (), list2[0]->get_endpoint ());
	ASSERT_EQ (btcnew::transport::transport_type::tcp, list2[0]->get_type ());
	// Stop the peer node and check that it is removed from the store
	system.nodes.front ()->stop ();

	system.deadline_set (10s);
	while (system.nodes.back ()->network.size () == 1)
	{
		ASSERT_NO_ERROR (system.poll ());
	}

	ASSERT_TRUE (system.nodes.back ()->network.empty ());

	// Uncontactable peer should not be stored
	auto transaction (store.tx_begin_read ());
	ASSERT_EQ (store.peer_count (transaction), 1);
	ASSERT_TRUE (store.peer_exists (transaction, endpoint_key));

	node->stop ();
}

TEST (node, peer_cache_restart)
{
	btcnew::system system (24000, 1);
	ASSERT_TRUE (system.nodes[0]->network.empty ());
	auto endpoint = system.nodes[0]->network.endpoint ();
	btcnew::endpoint_key endpoint_key{ endpoint.address ().to_v6 ().to_bytes (), endpoint.port () };
	auto path (btcnew::unique_path ());
	{
		auto node (std::make_shared<btcnew::node> (system.io_ctx, 24001, path, system.alarm, system.logging, system.work));
		system.nodes.push_back (node);
		auto & store = node->store;
		{
			// Add a peer to the database
			auto transaction (store.tx_begin_write ());
			store.peer_put (transaction, endpoint_key);
		}
		node->start ();
		system.deadline_set (10s);
		while (node->network.empty ())
		{
			ASSERT_NO_ERROR (system.poll ());
		}
		// Confirm that the peers match with the endpoints we are expecting
		auto list (node->network.list (2));
		ASSERT_EQ (system.nodes[0]->network.endpoint (), list[0]->get_endpoint ());
		ASSERT_EQ (1, node->network.size ());
		node->stop ();
	}
	// Restart node
	{
		btcnew::node_flags node_flags;
		node_flags.read_only = true;
		auto node (std::make_shared<btcnew::node> (system.io_ctx, 24002, path, system.alarm, system.logging, system.work, node_flags));
		system.nodes.push_back (node);
		// Check cached peers after restart
		node->network.start ();
		node->add_initial_peers ();

		auto & store = node->store;
		{
			auto transaction (store.tx_begin_read ());
			ASSERT_EQ (store.peer_count (transaction), 1);
			ASSERT_TRUE (store.peer_exists (transaction, endpoint_key));
		}
		system.deadline_set (10s);
		while (node->network.empty ())
		{
			ASSERT_NO_ERROR (system.poll ());
		}
		// Confirm that the peers match with the endpoints we are expecting
		auto list (node->network.list (2));
		ASSERT_EQ (system.nodes[0]->network.endpoint (), list[0]->get_endpoint ());
		ASSERT_EQ (1, node->network.size ());
		node->stop ();
	}
}

TEST (node, unchecked_cleanup)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key;
	auto & node (*system.nodes[0]);
	auto open (std::make_shared<btcnew::state_block> (key.pub, 0, key.pub, 1, key.pub, key.prv, key.pub, *system.work.generate (key.pub)));
	node.process_active (open);
	node.block_processor.flush ();
	node.config.unchecked_cutoff_time = std::chrono::seconds (2);
	{
		auto transaction (node.store.tx_begin_read ());
		auto unchecked_count (node.store.unchecked_count (transaction));
		ASSERT_EQ (unchecked_count, 1);
	}
	std::this_thread::sleep_for (std::chrono::seconds (1));
	node.unchecked_cleanup ();
	{
		auto transaction (node.store.tx_begin_read ());
		auto unchecked_count (node.store.unchecked_count (transaction));
		ASSERT_EQ (unchecked_count, 1);
	}
	std::this_thread::sleep_for (std::chrono::seconds (2));
	node.unchecked_cleanup ();
	{
		auto transaction (node.store.tx_begin_read ());
		auto unchecked_count (node.store.unchecked_count (transaction));
		ASSERT_EQ (unchecked_count, 0);
	}
}

/** This checks that a node can be opened (without being blocked) when a write lock is held elsewhere */
TEST (node, dont_write_lock_node)
{
	auto path = btcnew::unique_path ();

	std::promise<void> write_lock_held_promise;
	std::promise<void> finished_promise;
	// clang-format off
	std::thread ([&path, &write_lock_held_promise, &finished_promise]() {
		btcnew::logger_mt logger;
		auto store = btcnew::make_store (logger, path, false, true);
		{
			btcnew::genesis genesis;
			btcnew::rep_weights rep_weights;
			std::atomic<uint64_t> cemented_count{ 0 };
			std::atomic<uint64_t> block_count_cache{ 0 };
			auto transaction (store->tx_begin_write ());
			store->initialize (transaction, genesis, rep_weights, cemented_count, block_count_cache);
		}

		// Hold write lock open until main thread is done needing it
		auto transaction (store->tx_begin_write ());
		write_lock_held_promise.set_value ();
		finished_promise.get_future ().wait ();
	})
	.detach ();
	// clang-format off

	write_lock_held_promise.get_future ().wait ();

	// Check inactive node can finish executing while a write lock is open
	btcnew::inactive_node node (path);
	finished_promise.set_value ();
}

TEST (node, bidirectional_tcp)
{
	btcnew::system system;
	btcnew::node_flags node_flags;
	node_flags.disable_udp = true; // Disable UDP connections
	btcnew::node_config node_config (24000, system.logging);
	node_config.frontiers_confirmation = btcnew::frontiers_confirmation_mode::disabled;
	auto node1 = system.add_node (node_config, node_flags);
	node_config.peering_port = 24001;
	node_config.tcp_incoming_connections_max = 0; // Disable incoming TCP connections for node 2
	auto node2 = system.add_node (node_config, node_flags);
	// Check network connections
	ASSERT_EQ (1, node1->network.size ());
	ASSERT_EQ (1, node2->network.size ());
	auto list1 (node1->network.list (1));
	ASSERT_EQ (btcnew::transport::transport_type::tcp, list1[0]->get_type ());
	ASSERT_NE (node2->network.endpoint (), list1[0]->get_endpoint ()); // Ephemeral port
	ASSERT_EQ (node2->node_id.pub, list1[0]->get_node_id ());
	auto list2 (node2->network.list (1));
	ASSERT_EQ (btcnew::transport::transport_type::tcp, list2[0]->get_type ());
	ASSERT_EQ (node1->network.endpoint (), list2[0]->get_endpoint ());
	ASSERT_EQ (node1->node_id.pub, list2[0]->get_node_id ());
	// Test block propagation from node 1
	btcnew::genesis genesis;
	btcnew::keypair key;
	auto send1 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - btcnew::Gbtcnew_ratio, key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *node1->work_generate_blocking (genesis.hash ())));
	node1->process_active (send1);
	node1->block_processor.flush ();
	system.deadline_set (5s);
	while (!node1->ledger.block_exists (send1->hash ()) || !node2->ledger.block_exists (send1->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	// Test block confirmation from node 1
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	bool confirmed (false);
	system.deadline_set (10s);
	while (!confirmed)
	{
		auto transaction1 (node1->store.tx_begin_read ());
		auto transaction2 (node2->store.tx_begin_read ());
		confirmed = node1->ledger.block_confirmed (transaction1, send1->hash ()) && node2->ledger.block_confirmed (transaction2, send1->hash ());
		ASSERT_NO_ERROR (system.poll ());
	}
	// Test block propagation from node 2
	{
		auto transaction (system.wallet (0)->wallets.tx_begin_write ());
		system.wallet (0)->store.erase (transaction, btcnew::test_genesis_key.pub);
	}
	auto send2 (std::make_shared<btcnew::state_block> (btcnew::test_genesis_key.pub, send1->hash (), btcnew::test_genesis_key.pub, btcnew::genesis_amount - 2 * btcnew::Gbtcnew_ratio, key.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *node1->work_generate_blocking (send1->hash ())));
	node2->process_active (send2);
	node2->block_processor.flush ();
	system.deadline_set (5s);
	while (!node1->ledger.block_exists (send2->hash ()) || !node2->ledger.block_exists (send2->hash ()))
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	// Test block confirmation from node 2
	system.wallet (1)->insert_adhoc (btcnew::test_genesis_key.prv);
	confirmed = false;
	system.deadline_set (10s);
	while (!confirmed)
	{
		auto transaction1 (node1->store.tx_begin_read ());
		auto transaction2 (node2->store.tx_begin_read ());
		confirmed = node1->ledger.block_confirmed (transaction1, send2->hash ()) && node2->ledger.block_confirmed (transaction2, send2->hash ());
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (active_difficulty, recalculate_work)
{
	btcnew::system system;
	btcnew::node_config node_config (24000, system.logging);
	node_config.enable_voting = false;
	auto & node1 = *system.add_node (node_config);
	btcnew::genesis genesis;
	btcnew::keypair key1;
	ASSERT_EQ (node1.network_params.network.publish_threshold, node1.active.active_difficulty ());
	auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key1.pub, 0, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*send1);
	uint64_t difficulty1;
	btcnew::work_validate (*send1, &difficulty1);
	auto multiplier1 = btcnew::difficulty::to_multiplier (difficulty1, node1.network_params.network.publish_threshold);
	// Process as local block
	node1.process_active (send1);
	system.deadline_set (2s);
	while (node1.active.empty ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	auto sum (std::accumulate (node1.active.multipliers_cb.begin (), node1.active.multipliers_cb.end (), double(0)));
	ASSERT_EQ (node1.active.active_difficulty (), btcnew::difficulty::from_multiplier (sum / node1.active.multipliers_cb.size (), node1.network_params.network.publish_threshold));
	btcnew::unique_lock<std::mutex> lock (node1.active.mutex);
	// Fake history records to force work recalculation
	for (auto i (0); i < node1.active.multipliers_cb.size (); i++)
	{
		node1.active.multipliers_cb.push_back (multiplier1 * (1 + i / 100.));
	}
	node1.work_generate_blocking (*send1);
	uint64_t difficulty2;
	btcnew::work_validate (*send1, &difficulty2);
	node1.process_active (send1);
	node1.active.update_active_difficulty (lock);
	sum = std::accumulate (node1.active.multipliers_cb.begin (), node1.active.multipliers_cb.end (), double(0));
	ASSERT_EQ (node1.active.trended_active_difficulty, btcnew::difficulty::from_multiplier (sum / node1.active.multipliers_cb.size (), node1.network_params.network.publish_threshold));
	lock.unlock ();
}

namespace
{
void add_required_children_node_config_tree (btcnew::jsonconfig & tree)
{
	btcnew::logging logging1;
	btcnew::jsonconfig logging_l;
	logging1.serialize_json (logging_l);
	tree.put_child ("logging", logging_l);
	btcnew::jsonconfig preconfigured_peers_l;
	tree.put_child ("preconfigured_peers", preconfigured_peers_l);
	btcnew::jsonconfig preconfigured_representatives_l;
	tree.put_child ("preconfigured_representatives", preconfigured_representatives_l);
	btcnew::jsonconfig work_peers_l;
	tree.put_child ("work_peers", work_peers_l);
	tree.put ("version", std::to_string (btcnew::node_config::json_version ()));
}
}

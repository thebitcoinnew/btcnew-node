#include <btcnew/core_test/testutil.hpp>
#include <btcnew/node/node.hpp>
#include <btcnew/node/testing.hpp>
#include <btcnew/secure/versioning.hpp>

#include <gtest/gtest.h>

#include <boost/polymorphic_cast.hpp>

using namespace std::chrono_literals;

TEST (wallets, open_create)
{
	btcnew::system system (24000, 1);
	bool error (false);
	btcnew::wallets wallets (error, *system.nodes[0]);
	ASSERT_FALSE (error);
	ASSERT_EQ (1, wallets.items.size ()); // it starts out with a default wallet
	auto id = btcnew::random_wallet_id ();
	ASSERT_EQ (nullptr, wallets.open (id));
	auto wallet (wallets.create (id));
	ASSERT_NE (nullptr, wallet);
	ASSERT_EQ (wallet, wallets.open (id));
}

TEST (wallets, open_existing)
{
	btcnew::system system (24000, 1);
	auto id (btcnew::random_wallet_id ());
	{
		bool error (false);
		btcnew::wallets wallets (error, *system.nodes[0]);
		ASSERT_FALSE (error);
		ASSERT_EQ (1, wallets.items.size ());
		auto wallet (wallets.create (id));
		ASSERT_NE (nullptr, wallet);
		ASSERT_EQ (wallet, wallets.open (id));
		btcnew::raw_key password;
		password.data.clear ();
		system.deadline_set (10s);
		while (password.data == 0)
		{
			ASSERT_NO_ERROR (system.poll ());
			wallet->store.password.value (password);
		}
	}
	{
		bool error (false);
		btcnew::wallets wallets (error, *system.nodes[0]);
		ASSERT_FALSE (error);
		ASSERT_EQ (2, wallets.items.size ());
		ASSERT_NE (nullptr, wallets.open (id));
	}
}

TEST (wallets, remove)
{
	btcnew::system system (24000, 1);
	btcnew::wallet_id one (1);
	{
		bool error (false);
		btcnew::wallets wallets (error, *system.nodes[0]);
		ASSERT_FALSE (error);
		ASSERT_EQ (1, wallets.items.size ());
		auto wallet (wallets.create (one));
		ASSERT_NE (nullptr, wallet);
		ASSERT_EQ (2, wallets.items.size ());
		wallets.destroy (one);
		ASSERT_EQ (1, wallets.items.size ());
	}
	{
		bool error (false);
		btcnew::wallets wallets (error, *system.nodes[0]);
		ASSERT_FALSE (error);
		ASSERT_EQ (1, wallets.items.size ());
	}
}

TEST (wallets, upgrade)
{
	// Don't test this in rocksdb mode
	static btcnew::network_constants network_constants;
	auto use_rocksdb_str = std::getenv ("TEST_USE_ROCKSDB");
	if (use_rocksdb_str && boost::lexical_cast<int> (use_rocksdb_str) == 1)
	{
		return;
	}

	btcnew::system system;
	btcnew::node_config node_config (24000, system.logging);
	node_config.frontiers_confirmation = btcnew::frontiers_confirmation_mode::disabled;
	system.add_node (node_config);
	auto path (btcnew::unique_path ());
	auto id = btcnew::random_wallet_id ();
	btcnew::node_config node_config1 (24001, system.logging);
	node_config1.frontiers_confirmation = btcnew::frontiers_confirmation_mode::disabled;
	{
		auto node1 (std::make_shared<btcnew::node> (system.io_ctx, path, system.alarm, node_config1, system.work));
		ASSERT_FALSE (node1->init_error ());
		bool error (false);
		btcnew::wallets wallets (error, *node1);
		wallets.create (id);
		auto transaction_source (node1->wallets.env.tx_begin_write ());
		auto tx_source = static_cast<MDB_txn *> (transaction_source.get_handle ());
		auto & mdb_store (dynamic_cast<btcnew::mdb_store &> (node1->store));
		auto transaction_destination (mdb_store.tx_begin_write ());
		auto tx_destination = static_cast<MDB_txn *> (transaction_destination.get_handle ());
		wallets.move_table (id.to_string (), tx_source, tx_destination);
		node1->store.version_put (transaction_destination, 11);

		btcnew::account_info info;
		ASSERT_FALSE (mdb_store.account_get (transaction_destination, btcnew::genesis_account, info));
		auto rep_block = node1->rep_block (btcnew::genesis_account);
		btcnew::account_info_v13 account_info_v13 (info.head, rep_block, info.open_block, info.balance, info.modified, info.block_count, info.epoch ());
		auto status (mdb_put (mdb_store.env.tx (transaction_destination), info.epoch () == btcnew::epoch::epoch_0 ? mdb_store.accounts_v0 : mdb_store.accounts_v1, btcnew::mdb_val (btcnew::test_genesis_key.pub), btcnew::mdb_val (account_info_v13), 0));
		(void)status;
		assert (status == 0);
	}
	auto node1 (std::make_shared<btcnew::node> (system.io_ctx, path, system.alarm, node_config1, system.work));
	ASSERT_EQ (1, node1->wallets.items.size ());
	ASSERT_EQ (id, node1->wallets.items.begin ()->first);
	auto transaction_new (node1->wallets.env.tx_begin_write ());
	auto tx_new = static_cast<MDB_txn *> (transaction_new.get_handle ());
	auto transaction_old (node1->store.tx_begin_write ());
	auto tx_old = static_cast<MDB_txn *> (transaction_old.get_handle ());
	MDB_dbi old_handle;
	ASSERT_EQ (MDB_NOTFOUND, mdb_dbi_open (tx_old, id.to_string ().c_str (), 0, &old_handle));
	MDB_dbi new_handle;
	ASSERT_EQ (0, mdb_dbi_open (tx_new, id.to_string ().c_str (), 0, &new_handle));
}

// Keeps breaking whenever we add new DBs
TEST (wallets, DISABLED_wallet_create_max)
{
	btcnew::system system (24000, 1);
	bool error (false);
	btcnew::wallets wallets (error, *system.nodes[0]);
	const int nonWalletDbs = 19;
	for (int i = 0; i < system.nodes[0]->config.lmdb_max_dbs - nonWalletDbs; i++)
	{
		auto wallet_id = btcnew::random_wallet_id ();
		auto wallet = wallets.create (wallet_id);
		auto existing = wallets.items.find (wallet_id);
		ASSERT_TRUE (existing != wallets.items.end ());
		btcnew::raw_key seed;
		seed.data = 0;
		auto transaction (system.nodes[0]->store.tx_begin_write ());
		existing->second->store.seed_set (transaction, seed);
	}
	auto wallet_id = btcnew::random_wallet_id ();
	wallets.create (wallet_id);
	auto existing = wallets.items.find (wallet_id);
	ASSERT_TRUE (existing == wallets.items.end ());
}

TEST (wallets, reload)
{
	btcnew::system system (24000, 1);
	btcnew::wallet_id one (1);
	bool error (false);
	ASSERT_FALSE (error);
	ASSERT_EQ (1, system.nodes[0]->wallets.items.size ());
	{
		btcnew::lock_guard<std::mutex> lock_wallet (system.nodes[0]->wallets.mutex);
		btcnew::inactive_node node (system.nodes[0]->application_path, 24001);
		auto wallet (node.node->wallets.create (one));
		ASSERT_NE (wallet, nullptr);
	}
	system.deadline_set (5s);
	while (system.nodes[0]->wallets.open (one) == nullptr)
	{
		system.poll ();
	}
	ASSERT_EQ (2, system.nodes[0]->wallets.items.size ());
}

TEST (wallets, vote_minimum)
{
	btcnew::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	btcnew::keypair key1;
	btcnew::keypair key2;
	btcnew::genesis genesis;
	btcnew::state_block send1 (btcnew::test_genesis_key.pub, genesis.hash (), btcnew::test_genesis_key.pub, std::numeric_limits<btcnew::uint128_t>::max () - node1.config.vote_minimum.number (), key1.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ()));
	ASSERT_EQ (btcnew::process_result::progress, node1.process (send1).code);
	btcnew::state_block open1 (key1.pub, 0, key1.pub, node1.config.vote_minimum.number (), send1.hash (), key1.prv, key1.pub, *system.work.generate (key1.pub));
	ASSERT_EQ (btcnew::process_result::progress, node1.process (open1).code);
	// send2 with amount vote_minimum - 1 (not voting representative)
	btcnew::state_block send2 (btcnew::test_genesis_key.pub, send1.hash (), btcnew::test_genesis_key.pub, std::numeric_limits<btcnew::uint128_t>::max () - 2 * node1.config.vote_minimum.number () + 1, key2.pub, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (send1.hash ()));
	ASSERT_EQ (btcnew::process_result::progress, node1.process (send2).code);
	btcnew::state_block open2 (key2.pub, 0, key2.pub, node1.config.vote_minimum.number () - 1, send2.hash (), key2.prv, key2.pub, *system.work.generate (key2.pub));
	ASSERT_EQ (btcnew::process_result::progress, node1.process (open2).code);
	auto wallet (node1.wallets.items.begin ()->second);
	ASSERT_EQ (0, wallet->representatives.size ());
	wallet->insert_adhoc (btcnew::test_genesis_key.prv);
	wallet->insert_adhoc (key1.prv);
	wallet->insert_adhoc (key2.prv);
	node1.wallets.compute_reps ();
	ASSERT_EQ (2, wallet->representatives.size ());
}
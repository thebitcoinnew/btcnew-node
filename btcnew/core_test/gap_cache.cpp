#include <btcnew/core_test/testutil.hpp>
#include <btcnew/node/testing.hpp>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

TEST (gap_cache, add_new)
{
	btcnew::system system (24000, 1);
	btcnew::gap_cache cache (*system.nodes[0]);
	auto block1 (std::make_shared<btcnew::send_block> (0, 1, 2, btcnew::keypair ().prv, 4, 5));
	cache.add (block1->hash ());
}

TEST (gap_cache, add_existing)
{
	btcnew::system system (24000, 1);
	btcnew::gap_cache cache (*system.nodes[0]);
	auto block1 (std::make_shared<btcnew::send_block> (0, 1, 2, btcnew::keypair ().prv, 4, 5));
	cache.add (block1->hash ());
	btcnew::unique_lock<std::mutex> lock (cache.mutex);
	auto existing1 (cache.blocks.get<1> ().find (block1->hash ()));
	ASSERT_NE (cache.blocks.get<1> ().end (), existing1);
	auto arrival (existing1->arrival);
	lock.unlock ();
	system.deadline_set (20s);
	while (arrival == std::chrono::steady_clock::now ())
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	cache.add (block1->hash ());
	ASSERT_EQ (1, cache.size ());
	lock.lock ();
	auto existing2 (cache.blocks.get<1> ().find (block1->hash ()));
	ASSERT_NE (cache.blocks.get<1> ().end (), existing2);
	ASSERT_GT (existing2->arrival, arrival);
}

TEST (gap_cache, comparison)
{
	btcnew::system system (24000, 1);
	btcnew::gap_cache cache (*system.nodes[0]);
	auto block1 (std::make_shared<btcnew::send_block> (1, 0, 2, btcnew::keypair ().prv, 4, 5));
	cache.add (block1->hash ());
	btcnew::unique_lock<std::mutex> lock (cache.mutex);
	auto existing1 (cache.blocks.get<1> ().find (block1->hash ()));
	ASSERT_NE (cache.blocks.get<1> ().end (), existing1);
	auto arrival (existing1->arrival);
	lock.unlock ();
	system.deadline_set (20s);
	while (std::chrono::steady_clock::now () == arrival)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
	auto block3 (std::make_shared<btcnew::send_block> (0, 42, 1, btcnew::keypair ().prv, 3, 4));
	cache.add (block3->hash ());
	ASSERT_EQ (2, cache.size ());
	lock.lock ();
	auto existing2 (cache.blocks.get<1> ().find (block3->hash ()));
	ASSERT_NE (cache.blocks.get<1> ().end (), existing2);
	ASSERT_GT (existing2->arrival, arrival);
	ASSERT_EQ (arrival, cache.blocks.get<1> ().begin ()->arrival);
}

TEST (gap_cache, gap_bootstrap)
{
	btcnew::system system (24000, 2);
	btcnew::block_hash latest (system.nodes[0]->latest (btcnew::test_genesis_key.pub));
	btcnew::keypair key;
	auto send (std::make_shared<btcnew::send_block> (latest, key.pub, btcnew::genesis_amount - 100, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (latest)));
	{
		auto transaction (system.nodes[0]->store.tx_begin_write ());
		ASSERT_EQ (btcnew::process_result::progress, system.nodes[0]->block_processor.process_one (transaction, send).code);
	}
	ASSERT_EQ (btcnew::genesis_amount - 100, system.nodes[0]->balance (btcnew::genesis_account));
	ASSERT_EQ (btcnew::genesis_amount, system.nodes[1]->balance (btcnew::genesis_account));
	system.wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	auto latest_block (system.wallet (0)->send_action (btcnew::test_genesis_key.pub, key.pub, 100));
	ASSERT_NE (nullptr, latest_block);
	ASSERT_EQ (btcnew::genesis_amount - 200, system.nodes[0]->balance (btcnew::genesis_account));
	ASSERT_EQ (btcnew::genesis_amount, system.nodes[1]->balance (btcnew::genesis_account));
	system.deadline_set (10s);
	{
		// The separate publish and vote system doesn't work very well here because it's instantly confirmed.
		// We help it get the block and vote out here.
		auto transaction (system.nodes[0]->store.tx_begin_read ());
		system.nodes[0]->network.flood_block (latest_block);
	}
	while (system.nodes[1]->balance (btcnew::genesis_account) != btcnew::genesis_amount - 200)
	{
		ASSERT_NO_ERROR (system.poll ());
	}
}

TEST (gap_cache, two_dependencies)
{
	btcnew::system system (24000, 1);
	btcnew::keypair key;
	btcnew::genesis genesis;
	auto send1 (std::make_shared<btcnew::send_block> (genesis.hash (), key.pub, 1, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (genesis.hash ())));
	auto send2 (std::make_shared<btcnew::send_block> (send1->hash (), key.pub, 0, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *system.work.generate (send1->hash ())));
	auto open (std::make_shared<btcnew::open_block> (send1->hash (), key.pub, key.pub, key.prv, key.pub, *system.work.generate (key.pub)));
	ASSERT_EQ (0, system.nodes[0]->gap_cache.size ());
	system.nodes[0]->block_processor.add (send2, btcnew::seconds_since_epoch ());
	system.nodes[0]->block_processor.flush ();
	ASSERT_EQ (1, system.nodes[0]->gap_cache.size ());
	system.nodes[0]->block_processor.add (open, btcnew::seconds_since_epoch ());
	system.nodes[0]->block_processor.flush ();
	ASSERT_EQ (2, system.nodes[0]->gap_cache.size ());
	system.nodes[0]->block_processor.add (send1, btcnew::seconds_since_epoch ());
	system.nodes[0]->block_processor.flush ();
	ASSERT_EQ (0, system.nodes[0]->gap_cache.size ());
	auto transaction (system.nodes[0]->store.tx_begin_read ());
	ASSERT_TRUE (system.nodes[0]->store.block_exists (transaction, send1->hash ()));
	ASSERT_TRUE (system.nodes[0]->store.block_exists (transaction, send2->hash ()));
	ASSERT_TRUE (system.nodes[0]->store.block_exists (transaction, open->hash ()));
}

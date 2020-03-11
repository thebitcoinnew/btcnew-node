#include <btcnew/core_test/testutil.hpp>
#include <btcnew/node/node.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <future>
#include <thread>

TEST (processor_service, bad_send_signature)
{
	btcnew::logger_mt logger;
	auto store = btcnew::make_store (logger, btcnew::unique_path ());
	ASSERT_FALSE (store->init_error ());
	btcnew::stat stats;
	btcnew::ledger ledger (*store, stats);
	btcnew::genesis genesis;
	auto transaction (store->tx_begin_write ());
	store->initialize (transaction, genesis, ledger.rep_weights, ledger.cemented_count, ledger.block_count_cache);
	btcnew::work_pool pool (std::numeric_limits<unsigned>::max ());
	btcnew::account_info info1;
	ASSERT_FALSE (store->account_get (transaction, btcnew::test_genesis_key.pub, info1));
	btcnew::keypair key2;
	btcnew::send_block send (info1.head, btcnew::test_genesis_key.pub, 50, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *pool.generate (info1.head));
	send.signature.bytes[32] ^= 0x1;
	ASSERT_EQ (btcnew::process_result::bad_signature, ledger.process (transaction, send).code);
}

TEST (processor_service, bad_receive_signature)
{
	btcnew::logger_mt logger;
	auto store = btcnew::make_store (logger, btcnew::unique_path ());
	ASSERT_FALSE (store->init_error ());
	btcnew::stat stats;
	btcnew::ledger ledger (*store, stats);
	btcnew::genesis genesis;
	auto transaction (store->tx_begin_write ());
	store->initialize (transaction, genesis, ledger.rep_weights, ledger.cemented_count, ledger.block_count_cache);
	btcnew::work_pool pool (std::numeric_limits<unsigned>::max ());
	btcnew::account_info info1;
	ASSERT_FALSE (store->account_get (transaction, btcnew::test_genesis_key.pub, info1));
	btcnew::send_block send (info1.head, btcnew::test_genesis_key.pub, 50, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *pool.generate (info1.head));
	btcnew::block_hash hash1 (send.hash ());
	ASSERT_EQ (btcnew::process_result::progress, ledger.process (transaction, send).code);
	btcnew::account_info info2;
	ASSERT_FALSE (store->account_get (transaction, btcnew::test_genesis_key.pub, info2));
	btcnew::receive_block receive (hash1, hash1, btcnew::test_genesis_key.prv, btcnew::test_genesis_key.pub, *pool.generate (hash1));
	receive.signature.bytes[32] ^= 0x1;
	ASSERT_EQ (btcnew::process_result::bad_signature, ledger.process (transaction, receive).code);
}

TEST (alarm, one)
{
	boost::asio::io_context io_ctx;
	btcnew::alarm alarm (io_ctx);
	std::atomic<bool> done (false);
	std::mutex mutex;
	btcnew::condition_variable condition;
	alarm.add (std::chrono::steady_clock::now (), [&] () {
		{
			btcnew::lock_guard<std::mutex> lock (mutex);
			done = true;
		}
		condition.notify_one ();
	});
	boost::asio::io_context::work work (io_ctx);
	boost::thread thread ([&io_ctx] () { io_ctx.run (); });
	btcnew::unique_lock<std::mutex> unique (mutex);
	condition.wait (unique, [&] () { return !!done; });
	io_ctx.stop ();
	thread.join ();
}

TEST (alarm, many)
{
	boost::asio::io_context io_ctx;
	btcnew::alarm alarm (io_ctx);
	std::atomic<int> count (0);
	std::mutex mutex;
	btcnew::condition_variable condition;
	for (auto i (0); i < 50; ++i)
	{
		alarm.add (std::chrono::steady_clock::now (), [&] () {
			{
				btcnew::lock_guard<std::mutex> lock (mutex);
				count += 1;
			}
			condition.notify_one ();
		});
	}
	boost::asio::io_context::work work (io_ctx);
	std::vector<boost::thread> threads;
	for (auto i (0); i < 50; ++i)
	{
		threads.push_back (boost::thread ([&io_ctx] () { io_ctx.run (); }));
	}
	btcnew::unique_lock<std::mutex> unique (mutex);
	condition.wait (unique, [&] () { return count == 50; });
	io_ctx.stop ();
	for (auto i (threads.begin ()), j (threads.end ()); i != j; ++i)
	{
		i->join ();
	}
}

TEST (alarm, top_execution)
{
	boost::asio::io_context io_ctx;
	btcnew::alarm alarm (io_ctx);
	int value1 (0);
	int value2 (0);
	std::mutex mutex;
	std::promise<bool> promise;
	alarm.add (std::chrono::steady_clock::now (), [&] () {
		btcnew::lock_guard<std::mutex> lock (mutex);
		value1 = 1;
		value2 = 1;
	});
	alarm.add (std::chrono::steady_clock::now () + std::chrono::milliseconds (1), [&] () {
		btcnew::lock_guard<std::mutex> lock (mutex);
		value2 = 2;
		promise.set_value (false);
	});
	boost::asio::io_context::work work (io_ctx);
	boost::thread thread ([&io_ctx] () {
		io_ctx.run ();
	});
	promise.get_future ().get ();
	btcnew::lock_guard<std::mutex> lock (mutex);
	ASSERT_EQ (1, value1);
	ASSERT_EQ (2, value2);
	io_ctx.stop ();
	thread.join ();
}

#include <btcnew/core_test/testutil.hpp>
#include <btcnew/node/socket.hpp>
#include <btcnew/node/testing.hpp>

#include <gtest/gtest.h>

#include <boost/thread.hpp>

using namespace std::chrono_literals;

TEST (socket, concurrent_writes)
{
	auto node_flags = btcnew::inactive_node_flag_defaults ();
	node_flags.read_only = false;
	btcnew::inactive_node inactivenode (btcnew::unique_path (), 24000, node_flags);
	auto node = inactivenode.node;

	// This gives more realistic execution than using system#poll, allowing writes to
	// queue up and drain concurrently.
	btcnew::thread_runner runner (node->io_ctx, 1);

	constexpr size_t max_connections = 4;
	constexpr size_t client_count = max_connections;
	constexpr size_t message_count = 4;
	constexpr size_t total_message_count = client_count * message_count;

	// We're expecting client_count*4 messages
	btcnew::util::counted_completion read_count_completion (total_message_count);
	std::function<void (std::shared_ptr<btcnew::socket>)> reader = [&read_count_completion, &total_message_count, &reader] (std::shared_ptr<btcnew::socket> socket_a) {
		auto buff (std::make_shared<std::vector<uint8_t>> ());
		buff->resize (1);
#ifndef _WIN32
#pragma GCC diagnostic push
#if defined(__has_warning)
#if __has_warning("-Wunused-lambda-capture")
/** total_message_count is constexpr and a capture isn't needed. However, removing it fails to compile on VS2017 due to a known compiler bug. */
#pragma GCC diagnostic ignored "-Wunused-lambda-capture"
#endif
#endif
#endif
		socket_a->async_read (buff, 1, [&read_count_completion, &reader, &total_message_count, socket_a, buff] (boost::system::error_code const & ec, size_t size_a) {
			if (!ec)
			{
				if (read_count_completion.increment () < total_message_count)
				{
					reader (socket_a);
				}
			}
			else if (ec != boost::asio::error::eof)
			{
				std::cerr << "async_read: " << ec.message () << std::endl;
			}
		});
#ifndef _WIN32
#pragma GCC diagnostic pop
#endif
	};

	boost::asio::ip::tcp::endpoint endpoint (boost::asio::ip::address_v4::any (), 25000);

	auto server_socket (std::make_shared<btcnew::server_socket> (node, endpoint, max_connections, btcnew::socket::concurrency::multi_writer));
	boost::system::error_code ec;
	server_socket->start (ec);
	ASSERT_FALSE (ec);
	std::vector<std::shared_ptr<btcnew::socket>> connections;

	// On every new connection, start reading data
	server_socket->on_connection ([&connections, &reader] (std::shared_ptr<btcnew::socket> new_connection, boost::system::error_code const & ec_a) {
		if (ec_a)
		{
			std::cerr << "on_connection: " << ec_a.message () << std::endl;
		}
		else
		{
			connections.push_back (new_connection);
			reader (new_connection);
		}
		// Keep accepting connections
		return true;
	});

	btcnew::util::counted_completion connection_count_completion (client_count);
	std::vector<std::shared_ptr<btcnew::socket>> clients;
	for (unsigned i = 0; i < client_count; i++)
	{
		auto client (std::make_shared<btcnew::socket> (node, boost::none, btcnew::socket::concurrency::multi_writer));
		clients.push_back (client);
		client->async_connect (boost::asio::ip::tcp::endpoint (boost::asio::ip::address_v4::loopback (), 25000),
		[&connection_count_completion] (boost::system::error_code const & ec_a) {
			if (ec_a)
			{
				std::cerr << "async_connect: " << ec_a.message () << std::endl;
			}
			else
			{
				connection_count_completion.increment ();
			}
		});
	}
	ASSERT_FALSE (connection_count_completion.await_count_for (10s));

	// Execute overlapping writes from multiple threads
	auto client (clients[0]);
	std::vector<std::thread> client_threads;
	for (int i = 0; i < client_count; i++)
	{
#ifndef _WIN32
#pragma GCC diagnostic push
#if defined(__has_warning)
#if __has_warning("-Wunused-lambda-capture")
/** total_message_count is constexpr and a capture isn't needed. However, removing it fails to compile on VS2017 due to a known compiler bug. */
#pragma GCC diagnostic ignored "-Wunused-lambda-capture"
#endif
#endif
#endif
		client_threads.emplace_back ([&client, &message_count] () {
			for (int i = 0; i < message_count; i++)
			{
				std::vector<uint8_t> buff;
				buff.push_back ('A' + i);
				client->async_write (btcnew::shared_const_buffer (std::move (buff)));
			}
		});
#ifndef _WIN32
#pragma GCC diagnostic pop
#endif
	}

	ASSERT_FALSE (read_count_completion.await_count_for (10s));
	node->stop ();
	runner.stop_event_processing ();
	runner.join ();

	ASSERT_EQ (node->stats.count (btcnew::stat::type::tcp, btcnew::stat::detail::tcp_accept_success, btcnew::stat::dir::in), client_count);
	// We may exhaust max connections and have some tcp accept failures, but no more than the client count
	ASSERT_LT (node->stats.count (btcnew::stat::type::tcp, btcnew::stat::detail::tcp_accept_failure, btcnew::stat::dir::in), client_count);

	for (auto & t : client_threads)
	{
		t.join ();
	}
}

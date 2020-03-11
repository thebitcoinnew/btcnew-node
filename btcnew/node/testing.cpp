#include <btcnew/core_test/testutil.hpp>
#include <btcnew/crypto_lib/random_pool.hpp>
#include <btcnew/node/common.hpp>
#include <btcnew/node/testing.hpp>
#include <btcnew/node/transport/udp.hpp>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cstdlib>

std::string btcnew::error_system_messages::message (int ev) const
{
	switch (static_cast<btcnew::error_system> (ev))
	{
		case btcnew::error_system::generic:
			return "Unknown error";
		case btcnew::error_system::deadline_expired:
			return "Deadline expired";
	}

	return "Invalid error code";
}

/** Returns the node added. */
std::shared_ptr<btcnew::node> btcnew::system::add_node (btcnew::node_config const & node_config_a, btcnew::node_flags node_flags_a, btcnew::transport::transport_type type_a)
{
	auto node (std::make_shared<btcnew::node> (io_ctx, btcnew::unique_path (), alarm, node_config_a, work, node_flags_a));
	assert (!node->init_error ());
	node->start ();
	node->wallets.create (btcnew::random_wallet_id ());
	nodes.reserve (nodes.size () + 1);
	nodes.push_back (node);
	if (nodes.size () > 1)
	{
		auto begin = nodes.end () - 2;
		for (auto i (begin), j (begin + 1), n (nodes.end ()); j != n; ++i, ++j)
		{
			auto node1 (*i);
			auto node2 (*j);
			auto starting1 (node1->network.size ());
			size_t starting_listener1 (node1->bootstrap.realtime_count);
			decltype (starting1) new1;
			auto starting2 (node2->network.size ());
			size_t starting_listener2 (node2->bootstrap.realtime_count);
			decltype (starting2) new2;
			if (type_a == btcnew::transport::transport_type::tcp)
			{
				(*j)->network.merge_peer ((*i)->network.endpoint ());
			}
			else
			{
				// UDP connection
				auto channel (std::make_shared<btcnew::transport::channel_udp> ((*j)->network.udp_channels, (*i)->network.endpoint (), node1->network_params.protocol.protocol_version));
				(*j)->network.send_keepalive (channel);
			}
			do
			{
				poll ();
				new1 = node1->network.size ();
				new2 = node2->network.size ();
			} while (new1 == starting1 || new2 == starting2);
			if (type_a == btcnew::transport::transport_type::tcp && node_config_a.tcp_incoming_connections_max != 0 && !node_flags_a.disable_tcp_realtime)
			{
				// Wait for initial connection finish
				decltype (starting_listener1) new_listener1;
				decltype (starting_listener2) new_listener2;
				do
				{
					poll ();
					new_listener1 = node1->bootstrap.realtime_count;
					new_listener2 = node2->bootstrap.realtime_count;
				} while (new_listener1 == starting_listener1 || new_listener2 == starting_listener2);
			}
		}
		auto iterations1 (0);
		while (std::any_of (begin, nodes.end (), [] (std::shared_ptr<btcnew::node> const & node_a) { return node_a->bootstrap_initiator.in_progress (); }))
		{
			poll ();
			++iterations1;
			assert (iterations1 < 10000);
		}
	}
	else
	{
		auto iterations1 (0);
		while (node->bootstrap_initiator.in_progress ())
		{
			poll ();
			++iterations1;
			assert (iterations1 < 10000);
		}
	}

	return node;
}

btcnew::system::system ()
{
	auto scale_str = std::getenv ("DEADLINE_SCALE_FACTOR");
	if (scale_str)
	{
		deadline_scaling_factor = std::stod (scale_str);
	}
	logging.init (btcnew::unique_path ());
}

btcnew::system::system (uint16_t port_a, uint16_t count_a, btcnew::transport::transport_type type_a) :
system ()
{
	nodes.reserve (count_a);
	for (uint16_t i (0); i < count_a; ++i)
	{
		btcnew::node_config config (port_a + i, logging);
		add_node (config, btcnew::node_flags (), type_a);
	}
}

btcnew::system::~system ()
{
	for (auto & i : nodes)
	{
		i->stop ();
	}

#ifndef _WIN32
	// Windows cannot remove the log and data files while they are still owned by this process.
	// They will be removed later

	// Clean up tmp directories created by the tests. Since it's sometimes useful to
	// see log files after test failures, an environment variable is supported to
	// retain the files.
	if (std::getenv ("TEST_KEEP_TMPDIRS") == nullptr)
	{
		btcnew::remove_temporary_directories ();
	}
#endif
}

std::shared_ptr<btcnew::wallet> btcnew::system::wallet (size_t index_a)
{
	assert (nodes.size () > index_a);
	auto size (nodes[index_a]->wallets.items.size ());
	(void)size;
	assert (size == 1);
	return nodes[index_a]->wallets.items.begin ()->second;
}

btcnew::account btcnew::system::account (btcnew::transaction const & transaction_a, size_t index_a)
{
	auto wallet_l (wallet (index_a));
	auto keys (wallet_l->store.begin (transaction_a));
	assert (keys != wallet_l->store.end ());
	auto result (keys->first);
	assert (++keys == wallet_l->store.end ());
	return btcnew::account (result);
}

void btcnew::system::deadline_set (std::chrono::duration<double, std::nano> const & delta_a)
{
	deadline = std::chrono::steady_clock::now () + delta_a * deadline_scaling_factor;
}

std::error_code btcnew::system::poll (std::chrono::nanoseconds const & wait_time)
{
	std::error_code ec;
	io_ctx.run_one_for (wait_time);

	if (std::chrono::steady_clock::now () > deadline)
	{
		ec = btcnew::error_system::deadline_expired;
		stop ();
	}
	return ec;
}

namespace
{
class traffic_generator : public std::enable_shared_from_this<traffic_generator>
{
public:
	traffic_generator (uint32_t count_a, uint32_t wait_a, std::shared_ptr<btcnew::node> const & node_a, btcnew::system & system_a) :
	count (count_a),
	wait (wait_a),
	node (node_a),
	system (system_a)
	{
	}
	void run ()
	{
		auto count_l (count - 1);
		count = count_l - 1;
		system.generate_activity (*node, accounts);
		if (count_l > 0)
		{
			auto this_l (shared_from_this ());
			node->alarm.add (std::chrono::steady_clock::now () + std::chrono::milliseconds (wait), [this_l] () { this_l->run (); });
		}
	}
	std::vector<btcnew::account> accounts;
	uint32_t count;
	uint32_t wait;
	std::shared_ptr<btcnew::node> node;
	btcnew::system & system;
};
}

void btcnew::system::generate_usage_traffic (uint32_t count_a, uint32_t wait_a)
{
	for (size_t i (0), n (nodes.size ()); i != n; ++i)
	{
		generate_usage_traffic (count_a, wait_a, i);
	}
}

void btcnew::system::generate_usage_traffic (uint32_t count_a, uint32_t wait_a, size_t index_a)
{
	assert (nodes.size () > index_a);
	assert (count_a > 0);
	auto generate (std::make_shared<traffic_generator> (count_a, wait_a, nodes[index_a], *this));
	generate->run ();
}

void btcnew::system::generate_rollback (btcnew::node & node_a, std::vector<btcnew::account> & accounts_a)
{
	auto transaction (node_a.store.tx_begin_write ());
	assert (std::numeric_limits<CryptoPP::word32>::max () > accounts_a.size ());
	auto index (random_pool::generate_word32 (0, static_cast<CryptoPP::word32> (accounts_a.size () - 1)));
	auto account (accounts_a[index]);
	btcnew::account_info info;
	auto error (node_a.store.account_get (transaction, account, info));
	if (!error)
	{
		auto hash (info.open_block);
		btcnew::genesis genesis;
		if (hash != genesis.hash ())
		{
			accounts_a[index] = accounts_a[accounts_a.size () - 1];
			accounts_a.pop_back ();
			std::vector<std::shared_ptr<btcnew::block>> rollback_list;
			auto error = node_a.ledger.rollback (transaction, hash, rollback_list);
			(void)error;
			assert (!error);
			for (auto & i : rollback_list)
			{
				node_a.wallets.watcher->remove (i);
				node_a.active.erase (*i);
			}
		}
	}
}

void btcnew::system::generate_receive (btcnew::node & node_a)
{
	std::shared_ptr<btcnew::block> send_block;
	{
		auto transaction (node_a.store.tx_begin_read ());
		btcnew::account random_account;
		random_pool::generate_block (random_account.bytes.data (), sizeof (random_account.bytes));
		auto i (node_a.store.pending_begin (transaction, btcnew::pending_key (random_account, 0)));
		if (i != node_a.store.pending_end ())
		{
			btcnew::pending_key const & send_hash (i->first);
			send_block = node_a.store.block_get (transaction, send_hash.hash);
		}
	}
	if (send_block != nullptr)
	{
		auto receive_error (wallet (0)->receive_sync (send_block, btcnew::genesis_account, std::numeric_limits<btcnew::uint128_t>::max ()));
		(void)receive_error;
	}
}

void btcnew::system::generate_activity (btcnew::node & node_a, std::vector<btcnew::account> & accounts_a)
{
	auto what (random_pool::generate_byte ());
	if (what < 0x1)
	{
		generate_rollback (node_a, accounts_a);
	}
	else if (what < 0x10)
	{
		generate_change_known (node_a, accounts_a);
	}
	else if (what < 0x20)
	{
		generate_change_unknown (node_a, accounts_a);
	}
	else if (what < 0x70)
	{
		generate_receive (node_a);
	}
	else if (what < 0xc0)
	{
		generate_send_existing (node_a, accounts_a);
	}
	else
	{
		generate_send_new (node_a, accounts_a);
	}
}

btcnew::account btcnew::system::get_random_account (std::vector<btcnew::account> & accounts_a)
{
	assert (std::numeric_limits<CryptoPP::word32>::max () > accounts_a.size ());
	auto index (random_pool::generate_word32 (0, static_cast<CryptoPP::word32> (accounts_a.size () - 1)));
	auto result (accounts_a[index]);
	return result;
}

btcnew::uint128_t btcnew::system::get_random_amount (btcnew::transaction const & transaction_a, btcnew::node & node_a, btcnew::account const & account_a)
{
	btcnew::uint128_t balance (node_a.ledger.account_balance (transaction_a, account_a));
	btcnew::uint128_union random_amount;
	btcnew::random_pool::generate_block (random_amount.bytes.data (), sizeof (random_amount.bytes));
	return (((btcnew::uint256_t{ random_amount.number () } * balance) / btcnew::uint256_t{ std::numeric_limits<btcnew::uint128_t>::max () }).convert_to<btcnew::uint128_t> ());
}

void btcnew::system::generate_send_existing (btcnew::node & node_a, std::vector<btcnew::account> & accounts_a)
{
	btcnew::uint128_t amount;
	btcnew::account destination;
	btcnew::account source;
	{
		btcnew::account account;
		random_pool::generate_block (account.bytes.data (), sizeof (account.bytes));
		auto transaction (node_a.store.tx_begin_read ());
		btcnew::store_iterator<btcnew::account, btcnew::account_info> entry (node_a.store.latest_begin (transaction, account));
		if (entry == node_a.store.latest_end ())
		{
			entry = node_a.store.latest_begin (transaction);
		}
		assert (entry != node_a.store.latest_end ());
		destination = btcnew::account (entry->first);
		source = get_random_account (accounts_a);
		amount = get_random_amount (transaction, node_a, source);
	}
	if (!amount.is_zero ())
	{
		auto hash (wallet (0)->send_sync (source, destination, amount));
		(void)hash;
		assert (!hash.is_zero ());
	}
}

void btcnew::system::generate_change_known (btcnew::node & node_a, std::vector<btcnew::account> & accounts_a)
{
	btcnew::account source (get_random_account (accounts_a));
	if (!node_a.latest (source).is_zero ())
	{
		btcnew::account destination (get_random_account (accounts_a));
		auto change_error (wallet (0)->change_sync (source, destination));
		(void)change_error;
		assert (!change_error);
	}
}

void btcnew::system::generate_change_unknown (btcnew::node & node_a, std::vector<btcnew::account> & accounts_a)
{
	btcnew::account source (get_random_account (accounts_a));
	if (!node_a.latest (source).is_zero ())
	{
		btcnew::keypair key;
		btcnew::account destination (key.pub);
		auto change_error (wallet (0)->change_sync (source, destination));
		(void)change_error;
		assert (!change_error);
	}
}

void btcnew::system::generate_send_new (btcnew::node & node_a, std::vector<btcnew::account> & accounts_a)
{
	assert (node_a.wallets.items.size () == 1);
	btcnew::uint128_t amount;
	btcnew::account source;
	{
		auto transaction (node_a.store.tx_begin_read ());
		source = get_random_account (accounts_a);
		amount = get_random_amount (transaction, node_a, source);
	}
	if (!amount.is_zero ())
	{
		auto pub (node_a.wallets.items.begin ()->second->deterministic_insert ());
		accounts_a.push_back (pub);
		auto hash (wallet (0)->send_sync (source, pub, amount));
		(void)hash;
		assert (!hash.is_zero ());
	}
}

void btcnew::system::generate_mass_activity (uint32_t count_a, btcnew::node & node_a)
{
	std::vector<btcnew::account> accounts;
	wallet (0)->insert_adhoc (btcnew::test_genesis_key.prv);
	accounts.push_back (btcnew::test_genesis_key.pub);
	auto previous (std::chrono::steady_clock::now ());
	for (uint32_t i (0); i < count_a; ++i)
	{
		if ((i & 0xff) == 0)
		{
			auto now (std::chrono::steady_clock::now ());
			auto us (std::chrono::duration_cast<std::chrono::microseconds> (now - previous).count ());
			uint64_t count (0);
			uint64_t state (0);
			{
				auto transaction (node_a.store.tx_begin_read ());
				auto block_counts (node_a.store.block_count (transaction));
				count = block_counts.sum ();
				state = block_counts.state;
			}
			std::cerr << boost::str (boost::format ("Mass activity iteration %1% us %2% us/t %3% state: %4% old: %5%\n") % i % us % (us / 256) % state % (count - state));
			previous = now;
		}
		generate_activity (node_a, accounts);
	}
}

void btcnew::system::stop ()
{
	for (auto i : nodes)
	{
		i->stop ();
	}
	work.stop ();
}

namespace btcnew
{
void cleanup_test_directories_on_exit ()
{
	// Makes sure everything is cleaned up
	btcnew::logging::release_file_sink ();
	// Clean up tmp directories created by the tests. Since it's sometimes useful to
	// see log files after test failures, an environment variable is supported to
	// retain the files.
	if (std::getenv ("TEST_KEEP_TMPDIRS") == nullptr)
	{
		btcnew::remove_temporary_directories ();
	}
}
}

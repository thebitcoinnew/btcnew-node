#include <btcnew/node/network.hpp>
#include <btcnew/node/node.hpp>

#include <numeric>
#include <sstream>

btcnew::network::network (btcnew::node & node_a, uint16_t port_a) :
buffer_container (node_a.stats, btcnew::network::buffer_size, 4096), // 2Mb receive buffer
resolver (node_a.io_ctx),
node (node_a),
udp_channels (node_a, port_a),
tcp_channels (node_a),
disconnect_observer ([] () {})
{
	boost::thread::attributes attrs;
	btcnew::thread_attributes::set (attrs);
	for (size_t i = 0; i < node.config.network_threads; ++i)
	{
		packet_processing_threads.push_back (boost::thread (attrs, [this] () {
			btcnew::thread_role::set (btcnew::thread_role::name::packet_processing);
			try
			{
				udp_channels.process_packets ();
			}
			catch (boost::system::error_code & ec)
			{
				this->node.logger.try_log (FATAL_LOG_PREFIX, ec.message ());
				release_assert (false);
			}
			catch (std::error_code & ec)
			{
				this->node.logger.try_log (FATAL_LOG_PREFIX, ec.message ());
				release_assert (false);
			}
			catch (std::runtime_error & err)
			{
				this->node.logger.try_log (FATAL_LOG_PREFIX, err.what ());
				release_assert (false);
			}
			catch (...)
			{
				this->node.logger.try_log (FATAL_LOG_PREFIX, "Unknown exception");
				release_assert (false);
			}
			if (this->node.config.logging.network_packet_logging ())
			{
				this->node.logger.try_log ("Exiting packet processing thread");
			}
		}));
	}
}

btcnew::network::~network ()
{
	stop ();
}

void btcnew::network::start ()
{
	ongoing_cleanup ();
	ongoing_syn_cookie_cleanup ();
	if (!node.flags.disable_udp)
	{
		udp_channels.start ();
	}
	if (!node.flags.disable_tcp_realtime)
	{
		tcp_channels.start ();
	}
	ongoing_keepalive ();
}

void btcnew::network::stop ()
{
	if (!stopped.exchange (true))
	{
		udp_channels.stop ();
		tcp_channels.stop ();
		resolver.cancel ();
		buffer_container.stop ();
		for (auto & thread : packet_processing_threads)
		{
			thread.join ();
		}
	}
}

void btcnew::network::send_keepalive (std::shared_ptr<btcnew::transport::channel> channel_a)
{
	btcnew::keepalive message;
	random_fill (message.peers);
	channel_a->send (message);
}

void btcnew::network::send_keepalive_self (std::shared_ptr<btcnew::transport::channel> channel_a)
{
	btcnew::keepalive message;
	if (node.config.external_address != boost::asio::ip::address_v6{} && node.config.external_port != 0)
	{
		message.peers[0] = btcnew::endpoint (node.config.external_address, node.config.external_port);
	}
	else
	{
		auto external_address (node.port_mapping.external_address ());
		if (external_address.address () != boost::asio::ip::address_v4::any ())
		{
			message.peers[0] = btcnew::endpoint (boost::asio::ip::address_v6{}, endpoint ().port ());
			boost::system::error_code ec;
			auto external_v6 = boost::asio::ip::address_v6::from_string (external_address.address ().to_string (), ec);
			message.peers[1] = btcnew::endpoint (external_v6, external_address.port ());
		}
		else
		{
			message.peers[0] = btcnew::endpoint (boost::asio::ip::address_v6{}, endpoint ().port ());
		}
	}
	channel_a->send (message);
}

void btcnew::network::send_node_id_handshake (std::shared_ptr<btcnew::transport::channel> channel_a, boost::optional<btcnew::uint256_union> const & query, boost::optional<btcnew::uint256_union> const & respond_to)
{
	boost::optional<std::pair<btcnew::account, btcnew::signature>> response (boost::none);
	if (respond_to)
	{
		response = std::make_pair (node.node_id.pub, btcnew::sign_message (node.node_id.prv, node.node_id.pub, *respond_to));
		assert (!btcnew::validate_message (response->first, *respond_to, response->second));
	}
	btcnew::node_id_handshake message (query, response);
	if (node.config.logging.network_node_id_handshake_logging ())
	{
		node.logger.try_log (boost::str (boost::format ("Node ID handshake sent with node ID %1% to %2%: query %3%, respond_to %4% (signature %5%)") % node.node_id.pub.to_node_id () % channel_a->get_endpoint () % (query ? query->to_string () : std::string ("[none]")) % (respond_to ? respond_to->to_string () : std::string ("[none]")) % (response ? response->second.to_string () : std::string ("[none]"))));
	}
	channel_a->send (message);
}

template <typename T>
bool confirm_block (btcnew::transaction const & transaction_a, btcnew::node & node_a, T & list_a, std::shared_ptr<btcnew::block> block_a, bool also_publish)
{
	bool result (false);
	if (node_a.config.enable_voting)
	{
		auto hash (block_a->hash ());
		// Search in cache
		auto votes (node_a.votes_cache.find (hash));
		if (votes.empty ())
		{
			// Generate new vote
			node_a.wallets.foreach_representative ([&result, &list_a, &node_a, &transaction_a, &hash] (btcnew::public_key const & pub_a, btcnew::raw_key const & prv_a) {
				result = true;
				auto vote (node_a.store.vote_generate (transaction_a, pub_a, prv_a, std::vector<btcnew::block_hash> (1, hash)));
				btcnew::confirm_ack confirm (vote);
				for (auto j (list_a.begin ()), m (list_a.end ()); j != m; ++j)
				{
					j->get ()->send (confirm);
				}
				node_a.votes_cache.add (vote);
			});
		}
		else
		{
			// Send from cache
			for (auto & vote : votes)
			{
				btcnew::confirm_ack confirm (vote);
				for (auto j (list_a.begin ()), m (list_a.end ()); j != m; ++j)
				{
					j->get ()->send (confirm);
				}
			}
		}
		// Republish if required
		if (also_publish)
		{
			btcnew::publish publish (block_a);
			for (auto j (list_a.begin ()), m (list_a.end ()); j != m; ++j)
			{
				j->get ()->send (publish);
			}
		}
	}
	return result;
}

bool confirm_block (btcnew::transaction const & transaction_a, btcnew::node & node_a, std::shared_ptr<btcnew::transport::channel> channel_a, std::shared_ptr<btcnew::block> block_a, bool also_publish)
{
	std::array<std::shared_ptr<btcnew::transport::channel>, 1> endpoints = { channel_a };
	auto result (confirm_block (transaction_a, node_a, endpoints, std::move (block_a), also_publish));
	return result;
}

void btcnew::network::confirm_hashes (btcnew::transaction const & transaction_a, std::shared_ptr<btcnew::transport::channel> channel_a, std::vector<btcnew::block_hash> blocks_bundle_a)
{
	if (node.config.enable_voting)
	{
		node.wallets.foreach_representative ([this, &blocks_bundle_a, &channel_a, &transaction_a] (btcnew::public_key const & pub_a, btcnew::raw_key const & prv_a) {
			auto vote (this->node.store.vote_generate (transaction_a, pub_a, prv_a, blocks_bundle_a));
			btcnew::confirm_ack confirm (vote);
			std::shared_ptr<std::vector<uint8_t>> bytes (new std::vector<uint8_t>);
			{
				btcnew::vectorstream stream (*bytes);
				confirm.serialize (stream);
			}
			channel_a->send (confirm);
			this->node.votes_cache.add (vote);
		});
	}
}

bool btcnew::network::send_votes_cache (std::shared_ptr<btcnew::transport::channel> channel_a, btcnew::block_hash const & hash_a)
{
	// Search in cache
	auto votes (node.votes_cache.find (hash_a));
	// Send from cache
	for (auto & vote : votes)
	{
		btcnew::confirm_ack confirm (vote);
		channel_a->send (confirm);
	}
	// Returns true if votes were sent
	bool result (!votes.empty ());
	return result;
}

void btcnew::network::flood_message (btcnew::message const & message_a, bool const is_droppable_a)
{
	auto list (list_fanout ());
	for (auto i (list.begin ()), n (list.end ()); i != n; ++i)
	{
		(*i)->send (message_a, nullptr, is_droppable_a);
	}
}

void btcnew::network::flood_block_many (std::deque<std::shared_ptr<btcnew::block>> blocks_a, std::function<void ()> callback_a, unsigned delay_a)
{
	auto block_l (blocks_a.front ());
	blocks_a.pop_front ();
	flood_block (block_l);
	if (!blocks_a.empty ())
	{
		std::weak_ptr<btcnew::node> node_w (node.shared ());
		// clang-format off
		node.alarm.add (std::chrono::steady_clock::now () + std::chrono::milliseconds (delay_a + std::rand () % delay_a), [node_w, blocks (std::move (blocks_a)), callback_a, delay_a]() {
			if (auto node_l = node_w.lock ())
			{
				node_l->network.flood_block_many (std::move (blocks), callback_a, delay_a);
			}
		});
		// clang-format on
	}
	else if (callback_a)
	{
		callback_a ();
	}
}

void btcnew::network::send_confirm_req (std::shared_ptr<btcnew::transport::channel> channel_a, std::shared_ptr<btcnew::block> block_a)
{
	// Confirmation request with hash + root
	if (channel_a->get_network_version () >= node.network_params.protocol.tcp_realtime_protocol_version_min)
	{
		btcnew::confirm_req req (block_a->hash (), block_a->root ());
		channel_a->send (req);
	}
	// Confirmation request with full block
	else
	{
		btcnew::confirm_req req (block_a);
		channel_a->send (req);
	}
}

void btcnew::network::broadcast_confirm_req (std::shared_ptr<btcnew::block> block_a)
{
	auto list (std::make_shared<std::vector<std::shared_ptr<btcnew::transport::channel>>> (node.rep_crawler.representative_endpoints (std::numeric_limits<size_t>::max ())));
	if (list->empty () || node.rep_crawler.total_weight () < node.config.online_weight_minimum.number ())
	{
		// broadcast request to all peers (with max limit 2 * sqrt (peers count))
		auto peers (node.network.list (std::min (static_cast<size_t> (100), 2 * node.network.size_sqrt ())));
		list->clear ();
		for (auto & peer : peers)
		{
			list->push_back (peer);
		}
	}

	/*
	 * In either case (broadcasting to all representatives, or broadcasting to
	 * all peers because there are not enough connected representatives),
	 * limit each instance to a single random up-to-32 selection.  The invoker
	 * of "broadcast_confirm_req" will be responsible for calling it again
	 * if the votes for a block have not arrived in time.
	 */
	const size_t max_endpoints = 32;
	random_pool::shuffle (list->begin (), list->end ());
	if (list->size () > max_endpoints)
	{
		list->erase (list->begin () + max_endpoints, list->end ());
	}

	broadcast_confirm_req_base (block_a, list, 0);
}

void btcnew::network::broadcast_confirm_req_base (std::shared_ptr<btcnew::block> block_a, std::shared_ptr<std::vector<std::shared_ptr<btcnew::transport::channel>>> endpoints_a, unsigned delay_a, bool resumption)
{
	const size_t max_reps = 10;
	if (!resumption && node.config.logging.network_logging ())
	{
		node.logger.try_log (boost::str (boost::format ("Broadcasting confirm req for block %1% to %2% representatives") % block_a->hash ().to_string () % endpoints_a->size ()));
	}
	auto count (0);
	while (!endpoints_a->empty () && count < max_reps)
	{
		auto channel (endpoints_a->back ());
		send_confirm_req (channel, block_a);
		endpoints_a->pop_back ();
		count++;
	}
	if (!endpoints_a->empty ())
	{
		delay_a += std::rand () % broadcast_interval_ms;

		std::weak_ptr<btcnew::node> node_w (node.shared ());
		node.alarm.add (std::chrono::steady_clock::now () + std::chrono::milliseconds (delay_a), [node_w, block_a, endpoints_a, delay_a] () {
			if (auto node_l = node_w.lock ())
			{
				node_l->network.broadcast_confirm_req_base (block_a, endpoints_a, delay_a, true);
			}
		});
	}
}

void btcnew::network::broadcast_confirm_req_batched_many (std::unordered_map<std::shared_ptr<btcnew::transport::channel>, std::deque<std::pair<btcnew::block_hash, btcnew::root>>> request_bundle_a, std::function<void ()> callback_a, unsigned delay_a, bool resumption_a)
{
	if (!resumption_a && node.config.logging.network_logging ())
	{
		node.logger.try_log (boost::str (boost::format ("Broadcasting batch confirm req to %1% representatives") % request_bundle_a.size ()));
	}

	for (auto i (request_bundle_a.begin ()), n (request_bundle_a.end ()); i != n;)
	{
		std::vector<std::pair<btcnew::block_hash, btcnew::root>> roots_hashes_l;
		// Limit max request size hash + root to 7 pairs
		while (roots_hashes_l.size () < confirm_req_hashes_max && !i->second.empty ())
		{
			// expects ordering by priority, descending
			roots_hashes_l.push_back (i->second.front ());
			i->second.pop_front ();
		}
		btcnew::confirm_req req (roots_hashes_l);
		i->first->send (req);
		if (i->second.empty ())
		{
			i = request_bundle_a.erase (i);
		}
		else
		{
			++i;
		}
	}
	if (!request_bundle_a.empty ())
	{
		std::weak_ptr<btcnew::node> node_w (node.shared ());
		node.alarm.add (std::chrono::steady_clock::now () + std::chrono::milliseconds (delay_a), [node_w, request_bundle_a, callback_a, delay_a] () {
			if (auto node_l = node_w.lock ())
			{
				node_l->network.broadcast_confirm_req_batched_many (request_bundle_a, callback_a, delay_a, true);
			}
		});
	}
	else if (callback_a)
	{
		callback_a ();
	}
}

void btcnew::network::broadcast_confirm_req_many (std::deque<std::pair<std::shared_ptr<btcnew::block>, std::shared_ptr<std::vector<std::shared_ptr<btcnew::transport::channel>>>>> requests_a, std::function<void ()> callback_a, unsigned delay_a)
{
	auto pair_l (requests_a.front ());
	requests_a.pop_front ();
	auto block_l (pair_l.first);
	// confirm_req to representatives
	auto endpoints (pair_l.second);
	if (!endpoints->empty ())
	{
		broadcast_confirm_req_base (block_l, endpoints, delay_a);
	}
	/* Continue while blocks remain
	Broadcast with random delay between delay_a & 2*delay_a */
	if (!requests_a.empty ())
	{
		std::weak_ptr<btcnew::node> node_w (node.shared ());
		node.alarm.add (std::chrono::steady_clock::now () + std::chrono::milliseconds (delay_a + std::rand () % delay_a), [node_w, requests_a, callback_a, delay_a] () {
			if (auto node_l = node_w.lock ())
			{
				node_l->network.broadcast_confirm_req_many (requests_a, callback_a, delay_a);
			}
		});
	}
	else if (callback_a)
	{
		callback_a ();
	}
}

namespace
{
class network_message_visitor : public btcnew::message_visitor
{
public:
	network_message_visitor (btcnew::node & node_a, std::shared_ptr<btcnew::transport::channel> const & channel_a) :
	node (node_a),
	channel (channel_a)
	{
	}
	void keepalive (btcnew::keepalive const & message_a) override
	{
		if (node.config.logging.network_keepalive_logging ())
		{
			node.logger.try_log (boost::str (boost::format ("Received keepalive message from %1%") % channel->to_string ()));
		}
		node.stats.inc (btcnew::stat::type::message, btcnew::stat::detail::keepalive, btcnew::stat::dir::in);
		node.network.merge_peers (message_a.peers);
	}
	void publish (btcnew::publish const & message_a) override
	{
		if (node.config.logging.network_message_logging ())
		{
			node.logger.try_log (boost::str (boost::format ("Publish message from %1% for %2%") % channel->to_string () % message_a.block->hash ().to_string ()));
		}
		node.stats.inc (btcnew::stat::type::message, btcnew::stat::detail::publish, btcnew::stat::dir::in);
		if (!node.block_processor.full ())
		{
			node.process_active (message_a.block);
		}
		else
		{
			node.stats.inc (btcnew::stat::type::drop, btcnew::stat::detail::publish, btcnew::stat::dir::in);
		}
		node.active.publish (message_a.block);
	}
	void confirm_req (btcnew::confirm_req const & message_a) override
	{
		if (node.config.logging.network_message_logging ())
		{
			if (!message_a.roots_hashes.empty ())
			{
				node.logger.try_log (boost::str (boost::format ("Confirm_req message from %1% for hashes:roots %2%") % channel->to_string () % message_a.roots_string ()));
			}
			else
			{
				node.logger.try_log (boost::str (boost::format ("Confirm_req message from %1% for %2%") % channel->to_string () % message_a.block->hash ().to_string ()));
			}
		}
		node.stats.inc (btcnew::stat::type::message, btcnew::stat::detail::confirm_req, btcnew::stat::dir::in);
		// Don't load nodes with disabled voting
		if (node.config.enable_voting && node.wallets.reps_count)
		{
			if (message_a.block != nullptr)
			{
				auto hash (message_a.block->hash ());
				if (!node.network.send_votes_cache (channel, hash))
				{
					auto transaction (node.store.tx_begin_read ());
					auto successor (node.ledger.successor (transaction, message_a.block->qualified_root ()));
					if (successor != nullptr)
					{
						auto same_block (successor->hash () == hash);
						confirm_block (transaction, node, channel, std::move (successor), !same_block);
					}
				}
			}
			else if (!message_a.roots_hashes.empty ())
			{
				auto transaction (node.store.tx_begin_read ());
				std::vector<btcnew::block_hash> blocks_bundle;
				std::vector<std::shared_ptr<btcnew::vote>> cached_votes;
				size_t cached_count (0);
				for (auto & root_hash : message_a.roots_hashes)
				{
					auto find_votes (node.votes_cache.find (root_hash.first));
					if (!find_votes.empty ())
					{
						++cached_count;
						cached_votes.insert (cached_votes.end (), find_votes.begin (), find_votes.end ());
					}
					if (!find_votes.empty () || (!root_hash.first.is_zero () && node.store.block_exists (transaction, root_hash.first)))
					{
						blocks_bundle.push_back (root_hash.first);
					}
					else if (!root_hash.second.is_zero ())
					{
						btcnew::block_hash successor (0);
						// Search for block root
						successor = node.store.block_successor (transaction, root_hash.second);
						// Search for account root
						if (successor.is_zero ())
						{
							btcnew::account_info info;
							auto error (node.store.account_get (transaction, root_hash.second, info));
							if (!error)
							{
								successor = info.open_block;
							}
						}
						if (!successor.is_zero ())
						{
							auto find_successor_votes (node.votes_cache.find (successor));
							if (!find_successor_votes.empty ())
							{
								++cached_count;
								cached_votes.insert (cached_votes.end (), find_successor_votes.begin (), find_successor_votes.end ());
							}
							blocks_bundle.push_back (successor);
							auto successor_block (node.store.block_get (transaction, successor));
							assert (successor_block != nullptr);
							btcnew::publish publish (successor_block);
							channel->send (publish);
						}
					}
				}
				/* Decide to send cached votes or to create new vote
				If there is at least one new hash to confirm, then create new batch vote
				Otherwise use more bandwidth & save local resources required to sign vote */
				if (!blocks_bundle.empty () && cached_count < blocks_bundle.size ())
				{
					node.network.confirm_hashes (transaction, channel, blocks_bundle);
				}
				else
				{
					// Send from cache
					for (auto & vote : cached_votes)
					{
						btcnew::confirm_ack confirm (vote);
						channel->send (confirm);
					}
				}
			}
		}
	}
	void confirm_ack (btcnew::confirm_ack const & message_a) override
	{
		if (node.config.logging.network_message_logging ())
		{
			node.logger.try_log (boost::str (boost::format ("Received confirm_ack message from %1% for %2%sequence %3%") % channel->to_string () % message_a.vote->hashes_string () % std::to_string (message_a.vote->sequence)));
		}
		node.stats.inc (btcnew::stat::type::message, btcnew::stat::detail::confirm_ack, btcnew::stat::dir::in);
		for (auto & vote_block : message_a.vote->blocks)
		{
			if (!vote_block.which ())
			{
				auto block (boost::get<std::shared_ptr<btcnew::block>> (vote_block));
				if (!node.block_processor.full ())
				{
					node.process_active (block);
				}
				else
				{
					node.stats.inc (btcnew::stat::type::drop, btcnew::stat::detail::confirm_ack, btcnew::stat::dir::in);
				}
				node.active.publish (block);
			}
		}
		node.vote_processor.vote (message_a.vote, channel);
	}
	void bulk_pull (btcnew::bulk_pull const &) override
	{
		assert (false);
	}
	void bulk_pull_account (btcnew::bulk_pull_account const &) override
	{
		assert (false);
	}
	void bulk_push (btcnew::bulk_push const &) override
	{
		assert (false);
	}
	void frontier_req (btcnew::frontier_req const &) override
	{
		assert (false);
	}
	void node_id_handshake (btcnew::node_id_handshake const & message_a) override
	{
		node.stats.inc (btcnew::stat::type::message, btcnew::stat::detail::node_id_handshake, btcnew::stat::dir::in);
	}
	btcnew::node & node;
	std::shared_ptr<btcnew::transport::channel> channel;
};
}

void btcnew::network::process_message (btcnew::message const & message_a, std::shared_ptr<btcnew::transport::channel> channel_a)
{
	network_message_visitor visitor (node, channel_a);
	message_a.visit (visitor);
}

// Send keepalives to all the peers we've been notified of
void btcnew::network::merge_peers (std::array<btcnew::endpoint, 8> const & peers_a)
{
	for (auto i (peers_a.begin ()), j (peers_a.end ()); i != j; ++i)
	{
		merge_peer (*i);
	}
}

void btcnew::network::merge_peer (btcnew::endpoint const & peer_a)
{
	if (!reachout (peer_a, node.config.allow_local_peers))
	{
		std::weak_ptr<btcnew::node> node_w (node.shared ());
		node.network.tcp_channels.start_tcp (peer_a, [node_w] (std::shared_ptr<btcnew::transport::channel> channel_a) {
			if (auto node_l = node_w.lock ())
			{
				node_l->network.send_keepalive (channel_a);
			}
		});
	}
}

bool btcnew::network::not_a_peer (btcnew::endpoint const & endpoint_a, bool allow_local_peers)
{
	bool result (false);
	if (endpoint_a.address ().to_v6 ().is_unspecified ())
	{
		result = true;
	}
	else if (btcnew::transport::reserved_address (endpoint_a, allow_local_peers))
	{
		result = true;
	}
	else if (endpoint_a == endpoint ())
	{
		result = true;
	}
	return result;
}

bool btcnew::network::reachout (btcnew::endpoint const & endpoint_a, bool allow_local_peers)
{
	// Don't contact invalid IPs
	bool error = not_a_peer (endpoint_a, allow_local_peers);
	if (!error)
	{
		error |= udp_channels.reachout (endpoint_a);
		error |= tcp_channels.reachout (endpoint_a);
	}
	return error;
}

std::deque<std::shared_ptr<btcnew::transport::channel>> btcnew::network::list (size_t count_a)
{
	std::deque<std::shared_ptr<btcnew::transport::channel>> result;
	tcp_channels.list (result);
	udp_channels.list (result);
	random_pool::shuffle (result.begin (), result.end ());
	if (result.size () > count_a)
	{
		result.resize (count_a, nullptr);
	}
	return result;
}

// Simulating with sqrt_broadcast_simulate shows we only need to broadcast to sqrt(total_peers) random peers in order to successfully publish to everyone with high probability
std::deque<std::shared_ptr<btcnew::transport::channel>> btcnew::network::list_fanout ()
{
	auto result (list (size_sqrt ()));
	return result;
}

std::unordered_set<std::shared_ptr<btcnew::transport::channel>> btcnew::network::random_set (size_t count_a) const
{
	std::unordered_set<std::shared_ptr<btcnew::transport::channel>> result (tcp_channels.random_set (count_a));
	std::unordered_set<std::shared_ptr<btcnew::transport::channel>> udp_random (udp_channels.random_set (count_a));
	for (auto i (udp_random.begin ()), n (udp_random.end ()); i != n && result.size () < count_a * 1.5; ++i)
	{
		result.insert (*i);
	}
	while (result.size () > count_a)
	{
		result.erase (result.begin ());
	}
	return result;
}

void btcnew::network::random_fill (std::array<btcnew::endpoint, 8> & target_a) const
{
	auto peers (random_set (target_a.size ()));
	assert (peers.size () <= target_a.size ());
	auto endpoint (btcnew::endpoint (boost::asio::ip::address_v6{}, 0));
	assert (endpoint.address ().is_v6 ());
	std::fill (target_a.begin (), target_a.end (), endpoint);
	auto j (target_a.begin ());
	for (auto i (peers.begin ()), n (peers.end ()); i != n; ++i, ++j)
	{
		assert ((*i)->get_endpoint ().address ().is_v6 ());
		assert (j < target_a.end ());
		*j = (*i)->get_endpoint ();
	}
}

btcnew::tcp_endpoint btcnew::network::bootstrap_peer (bool lazy_bootstrap)
{
	btcnew::tcp_endpoint result (boost::asio::ip::address_v6::any (), 0);
	bool use_udp_peer (btcnew::random_pool::generate_word32 (0, 1));
	auto protocol_min (lazy_bootstrap ? node.network_params.protocol.protocol_version_bootstrap_lazy_min : node.network_params.protocol.protocol_version_bootstrap_min);
	if (use_udp_peer || tcp_channels.size () == 0)
	{
		result = udp_channels.bootstrap_peer (protocol_min);
	}
	if (result == btcnew::tcp_endpoint (boost::asio::ip::address_v6::any (), 0))
	{
		result = tcp_channels.bootstrap_peer (protocol_min);
	}
	return result;
}

std::shared_ptr<btcnew::transport::channel> btcnew::network::find_channel (btcnew::endpoint const & endpoint_a)
{
	std::shared_ptr<btcnew::transport::channel> result (tcp_channels.find_channel (btcnew::transport::map_endpoint_to_tcp (endpoint_a)));
	if (!result)
	{
		result = udp_channels.channel (endpoint_a);
	}
	return result;
}

std::shared_ptr<btcnew::transport::channel> btcnew::network::find_node_id (btcnew::account const & node_id_a)
{
	std::shared_ptr<btcnew::transport::channel> result (tcp_channels.find_node_id (node_id_a));
	if (!result)
	{
		result = udp_channels.find_node_id (node_id_a);
	}
	return result;
}

btcnew::endpoint btcnew::network::endpoint ()
{
	return udp_channels.get_local_endpoint ();
}

void btcnew::network::cleanup (std::chrono::steady_clock::time_point const & cutoff_a)
{
	tcp_channels.purge (cutoff_a);
	udp_channels.purge (cutoff_a);
	if (node.network.empty ())
	{
		disconnect_observer ();
	}
}

void btcnew::network::ongoing_cleanup ()
{
	cleanup (std::chrono::steady_clock::now () - node.network_params.node.cutoff);
	std::weak_ptr<btcnew::node> node_w (node.shared ());
	node.alarm.add (std::chrono::steady_clock::now () + node.network_params.node.period, [node_w] () {
		if (auto node_l = node_w.lock ())
		{
			node_l->network.ongoing_cleanup ();
		}
	});
}

void btcnew::network::ongoing_syn_cookie_cleanup ()
{
	syn_cookies.purge (std::chrono::steady_clock::now () - btcnew::transport::syn_cookie_cutoff);
	std::weak_ptr<btcnew::node> node_w (node.shared ());
	node.alarm.add (std::chrono::steady_clock::now () + (btcnew::transport::syn_cookie_cutoff * 2), [node_w] () {
		if (auto node_l = node_w.lock ())
		{
			node_l->network.ongoing_syn_cookie_cleanup ();
		}
	});
}

void btcnew::network::ongoing_keepalive ()
{
	flood_keepalive ();
	std::weak_ptr<btcnew::node> node_w (node.shared ());
	node.alarm.add (std::chrono::steady_clock::now () + node.network_params.node.half_period, [node_w] () {
		if (auto node_l = node_w.lock ())
		{
			node_l->network.ongoing_keepalive ();
		}
	});
}

size_t btcnew::network::size () const
{
	return tcp_channels.size () + udp_channels.size ();
}

size_t btcnew::network::size_sqrt () const
{
	return (static_cast<size_t> (std::ceil (std::sqrt (size ()))));
}

bool btcnew::network::empty () const
{
	return size () == 0;
}

btcnew::message_buffer_manager::message_buffer_manager (btcnew::stat & stats_a, size_t size, size_t count) :
stats (stats_a),
free (count),
full (count),
slab (size * count),
entries (count),
stopped (false)
{
	assert (count > 0);
	assert (size > 0);
	auto slab_data (slab.data ());
	auto entry_data (entries.data ());
	for (auto i (0); i < count; ++i, ++entry_data)
	{
		*entry_data = { slab_data + i * size, 0, btcnew::endpoint () };
		free.push_back (entry_data);
	}
}

btcnew::message_buffer * btcnew::message_buffer_manager::allocate ()
{
	btcnew::unique_lock<std::mutex> lock (mutex);
	if (!stopped && free.empty () && full.empty ())
	{
		stats.inc (btcnew::stat::type::udp, btcnew::stat::detail::blocking, btcnew::stat::dir::in);
		// clang-format off
		condition.wait (lock, [& stopped = stopped, &free = free, &full = full] { return stopped || !free.empty () || !full.empty (); });
		// clang-format on
	}
	btcnew::message_buffer * result (nullptr);
	if (!free.empty ())
	{
		result = free.front ();
		free.pop_front ();
	}
	if (result == nullptr && !full.empty ())
	{
		result = full.front ();
		full.pop_front ();
		stats.inc (btcnew::stat::type::udp, btcnew::stat::detail::overflow, btcnew::stat::dir::in);
	}
	release_assert (result || stopped);
	return result;
}

void btcnew::message_buffer_manager::enqueue (btcnew::message_buffer * data_a)
{
	assert (data_a != nullptr);
	{
		btcnew::lock_guard<std::mutex> lock (mutex);
		full.push_back (data_a);
	}
	condition.notify_all ();
}

btcnew::message_buffer * btcnew::message_buffer_manager::dequeue ()
{
	btcnew::unique_lock<std::mutex> lock (mutex);
	while (!stopped && full.empty ())
	{
		condition.wait (lock);
	}
	btcnew::message_buffer * result (nullptr);
	if (!full.empty ())
	{
		result = full.front ();
		full.pop_front ();
	}
	return result;
}

void btcnew::message_buffer_manager::release (btcnew::message_buffer * data_a)
{
	assert (data_a != nullptr);
	{
		btcnew::lock_guard<std::mutex> lock (mutex);
		free.push_back (data_a);
	}
	condition.notify_all ();
}

void btcnew::message_buffer_manager::stop ()
{
	{
		btcnew::lock_guard<std::mutex> lock (mutex);
		stopped = true;
	}
	condition.notify_all ();
}

boost::optional<btcnew::uint256_union> btcnew::syn_cookies::assign (btcnew::endpoint const & endpoint_a)
{
	auto ip_addr (endpoint_a.address ());
	assert (ip_addr.is_v6 ());
	btcnew::lock_guard<std::mutex> lock (syn_cookie_mutex);
	unsigned & ip_cookies = cookies_per_ip[ip_addr];
	boost::optional<btcnew::uint256_union> result;
	if (ip_cookies < btcnew::transport::max_peers_per_ip)
	{
		if (cookies.find (endpoint_a) == cookies.end ())
		{
			btcnew::uint256_union query;
			random_pool::generate_block (query.bytes.data (), query.bytes.size ());
			syn_cookie_info info{ query, std::chrono::steady_clock::now () };
			cookies[endpoint_a] = info;
			++ip_cookies;
			result = query;
		}
	}
	return result;
}

bool btcnew::syn_cookies::validate (btcnew::endpoint const & endpoint_a, btcnew::account const & node_id, btcnew::signature const & sig)
{
	auto ip_addr (endpoint_a.address ());
	assert (ip_addr.is_v6 ());
	btcnew::lock_guard<std::mutex> lock (syn_cookie_mutex);
	auto result (true);
	auto cookie_it (cookies.find (endpoint_a));
	if (cookie_it != cookies.end () && !btcnew::validate_message (node_id, cookie_it->second.cookie, sig))
	{
		result = false;
		cookies.erase (cookie_it);
		unsigned & ip_cookies = cookies_per_ip[ip_addr];
		if (ip_cookies > 0)
		{
			--ip_cookies;
		}
		else
		{
			assert (false && "More SYN cookies deleted than created for IP");
		}
	}
	return result;
}

void btcnew::syn_cookies::purge (std::chrono::steady_clock::time_point const & cutoff_a)
{
	btcnew::lock_guard<std::mutex> lock (syn_cookie_mutex);
	auto it (cookies.begin ());
	while (it != cookies.end ())
	{
		auto info (it->second);
		if (info.created_at < cutoff_a)
		{
			unsigned & per_ip = cookies_per_ip[it->first.address ()];
			if (per_ip > 0)
			{
				--per_ip;
			}
			else
			{
				assert (false && "More SYN cookies deleted than created for IP");
			}
			it = cookies.erase (it);
		}
		else
		{
			++it;
		}
	}
}

std::unique_ptr<btcnew::seq_con_info_component> btcnew::syn_cookies::collect_seq_con_info (std::string const & name)
{
	size_t syn_cookies_count = 0;
	size_t syn_cookies_per_ip_count = 0;
	{
		btcnew::lock_guard<std::mutex> syn_cookie_guard (syn_cookie_mutex);
		syn_cookies_count = cookies.size ();
		syn_cookies_per_ip_count = cookies_per_ip.size ();
	}
	auto composite = std::make_unique<seq_con_info_composite> (name);
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "syn_cookies", syn_cookies_count, sizeof (decltype (cookies)::value_type) }));
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "syn_cookies_per_ip", syn_cookies_per_ip_count, sizeof (decltype (cookies_per_ip)::value_type) }));
	return composite;
}

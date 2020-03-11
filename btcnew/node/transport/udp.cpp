#include <btcnew/crypto_lib/random_pool.hpp>
#include <btcnew/lib/stats.hpp>
#include <btcnew/node/node.hpp>
#include <btcnew/node/transport/udp.hpp>

btcnew::transport::channel_udp::channel_udp (btcnew::transport::udp_channels & channels_a, btcnew::endpoint const & endpoint_a, uint8_t protocol_version_a) :
channel (channels_a.node),
endpoint (endpoint_a),
channels (channels_a)
{
	set_network_version (protocol_version_a);
	assert (endpoint_a.address ().is_v6 ());
}

size_t btcnew::transport::channel_udp::hash_code () const
{
	std::hash<::btcnew::endpoint> hash;
	return hash (endpoint);
}

bool btcnew::transport::channel_udp::operator== (btcnew::transport::channel const & other_a) const
{
	bool result (false);
	auto other_l (dynamic_cast<btcnew::transport::channel_udp const *> (&other_a));
	if (other_l != nullptr)
	{
		return *this == *other_l;
	}
	return result;
}

void btcnew::transport::channel_udp::send_buffer (btcnew::shared_const_buffer const & buffer_a, btcnew::stat::detail detail_a, std::function<void (boost::system::error_code const &, size_t)> const & callback_a)
{
	set_last_packet_sent (std::chrono::steady_clock::now ());
	channels.send (buffer_a, endpoint, callback (detail_a, callback_a));
}

std::function<void (boost::system::error_code const &, size_t)> btcnew::transport::channel_udp::callback (btcnew::stat::detail detail_a, std::function<void (boost::system::error_code const &, size_t)> const & callback_a) const
{
	// clang-format off
	return [node = std::weak_ptr<btcnew::node> (channels.node.shared ()), callback_a ](boost::system::error_code const & ec, size_t size_a)
	{
		if (auto node_l = node.lock ())
		{
			if (ec == boost::system::errc::host_unreachable)
			{
				node_l->stats.inc (btcnew::stat::type::error, btcnew::stat::detail::unreachable_host, btcnew::stat::dir::out);
			}
			if (size_a > 0)
			{
				node_l->stats.add (btcnew::stat::type::traffic_udp, btcnew::stat::dir::out, size_a);
			}

			if (callback_a)
			{
				callback_a (ec, size_a);
			}
		}
	};
	// clang-format on
}

std::string btcnew::transport::channel_udp::to_string () const
{
	return boost::str (boost::format ("%1%") % endpoint);
}

btcnew::transport::udp_channels::udp_channels (btcnew::node & node_a, uint16_t port_a) :
node (node_a),
strand (node_a.io_ctx.get_executor ()),
socket (node_a.io_ctx, btcnew::endpoint (boost::asio::ip::address_v6::any (), port_a))
{
	boost::system::error_code ec;
	auto port (socket.local_endpoint (ec).port ());
	if (ec)
	{
		node.logger.try_log ("Unable to retrieve port: ", ec.message ());
	}

	local_endpoint = btcnew::endpoint (boost::asio::ip::address_v6::loopback (), port);
}

void btcnew::transport::udp_channels::send (btcnew::shared_const_buffer const & buffer_a, btcnew::endpoint endpoint_a, std::function<void (boost::system::error_code const &, size_t)> const & callback_a)
{
	boost::asio::post (strand,
	[this, buffer_a, endpoint_a, callback_a] () {
		this->socket.async_send_to (buffer_a, endpoint_a,
		boost::asio::bind_executor (strand, callback_a));
	});
}

std::shared_ptr<btcnew::transport::channel_udp> btcnew::transport::udp_channels::insert (btcnew::endpoint const & endpoint_a, unsigned network_version_a)
{
	assert (endpoint_a.address ().is_v6 ());
	std::shared_ptr<btcnew::transport::channel_udp> result;
	if (!node.network.not_a_peer (endpoint_a, node.config.allow_local_peers) && (node.network_params.network.is_test_network () || !max_ip_connections (endpoint_a)))
	{
		btcnew::unique_lock<std::mutex> lock (mutex);
		auto existing (channels.get<endpoint_tag> ().find (endpoint_a));
		if (existing != channels.get<endpoint_tag> ().end ())
		{
			result = existing->channel;
		}
		else
		{
			result = std::make_shared<btcnew::transport::channel_udp> (*this, endpoint_a, network_version_a);
			channels.get<endpoint_tag> ().insert ({ result });
			lock.unlock ();
			node.network.channel_observer (result);
		}
	}
	return result;
}

void btcnew::transport::udp_channels::erase (btcnew::endpoint const & endpoint_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	channels.get<endpoint_tag> ().erase (endpoint_a);
}

size_t btcnew::transport::udp_channels::size () const
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	return channels.size ();
}

std::shared_ptr<btcnew::transport::channel_udp> btcnew::transport::udp_channels::channel (btcnew::endpoint const & endpoint_a) const
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	std::shared_ptr<btcnew::transport::channel_udp> result;
	auto existing (channels.get<endpoint_tag> ().find (endpoint_a));
	if (existing != channels.get<endpoint_tag> ().end ())
	{
		result = existing->channel;
	}
	return result;
}

std::unordered_set<std::shared_ptr<btcnew::transport::channel>> btcnew::transport::udp_channels::random_set (size_t count_a) const
{
	std::unordered_set<std::shared_ptr<btcnew::transport::channel>> result;
	result.reserve (count_a);
	btcnew::lock_guard<std::mutex> lock (mutex);
	// Stop trying to fill result with random samples after this many attempts
	auto random_cutoff (count_a * 2);
	auto peers_size (channels.size ());
	// Usually count_a will be much smaller than peers.size()
	// Otherwise make sure we have a cutoff on attempting to randomly fill
	if (!channels.empty ())
	{
		for (auto i (0); i < random_cutoff && result.size () < count_a; ++i)
		{
			auto index (btcnew::random_pool::generate_word32 (0, static_cast<CryptoPP::word32> (peers_size - 1)));
			result.insert (channels.get<random_access_tag> ()[index].channel);
		}
	}
	return result;
}

void btcnew::transport::udp_channels::random_fill (std::array<btcnew::endpoint, 8> & target_a) const
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

bool btcnew::transport::udp_channels::store_all (bool clear_peers)
{
	// We can't hold the mutex while starting a write transaction, so
	// we collect endpoints to be saved and then relase the lock.
	std::vector<btcnew::endpoint> endpoints;
	{
		btcnew::lock_guard<std::mutex> lock (mutex);
		endpoints.reserve (channels.size ());
		std::transform (channels.begin (), channels.end (),
		std::back_inserter (endpoints), [] (const auto & channel) { return channel.endpoint (); });
	}
	bool result (false);
	if (!endpoints.empty ())
	{
		// Clear all peers then refresh with the current list of peers
		auto transaction (node.store.tx_begin_write ({ tables::peers }));
		if (clear_peers)
		{
			node.store.peer_clear (transaction);
		}
		for (auto endpoint : endpoints)
		{
			btcnew::endpoint_key endpoint_key (endpoint.address ().to_v6 ().to_bytes (), endpoint.port ());
			node.store.peer_put (transaction, std::move (endpoint_key));
		}
		result = true;
	}
	return result;
}

std::shared_ptr<btcnew::transport::channel_udp> btcnew::transport::udp_channels::find_node_id (btcnew::account const & node_id_a)
{
	std::shared_ptr<btcnew::transport::channel_udp> result;
	btcnew::lock_guard<std::mutex> lock (mutex);
	auto existing (channels.get<node_id_tag> ().find (node_id_a));
	if (existing != channels.get<node_id_tag> ().end ())
	{
		result = existing->channel;
	}
	return result;
}

void btcnew::transport::udp_channels::clean_node_id (btcnew::account const & node_id_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	channels.get<node_id_tag> ().erase (node_id_a);
}

void btcnew::transport::udp_channels::clean_node_id (btcnew::endpoint const & endpoint_a, btcnew::account const & node_id_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	auto existing (channels.get<node_id_tag> ().equal_range (node_id_a));
	for (auto & record : boost::make_iterator_range (existing))
	{
		// Remove duplicate node ID for same IP address
		if (record.endpoint ().address () == endpoint_a.address () && record.endpoint ().port () != endpoint_a.port ())
		{
			channels.get<endpoint_tag> ().erase (record.endpoint ());
			break;
		}
	}
}

btcnew::tcp_endpoint btcnew::transport::udp_channels::bootstrap_peer (uint8_t connection_protocol_version_min)
{
	btcnew::tcp_endpoint result (boost::asio::ip::address_v6::any (), 0);
	btcnew::lock_guard<std::mutex> lock (mutex);
	for (auto i (channels.get<last_bootstrap_attempt_tag> ().begin ()), n (channels.get<last_bootstrap_attempt_tag> ().end ()); i != n;)
	{
		if (i->channel->get_network_version () >= connection_protocol_version_min)
		{
			result = btcnew::transport::map_endpoint_to_tcp (i->endpoint ());
			channels.get<last_bootstrap_attempt_tag> ().modify (i, [] (channel_udp_wrapper & wrapper_a) {
				wrapper_a.channel->set_last_bootstrap_attempt (std::chrono::steady_clock::now ());
			});
			i = n;
		}
		else
		{
			++i;
		}
	}
	return result;
}

void btcnew::transport::udp_channels::receive ()
{
	if (node.config.logging.network_packet_logging ())
	{
		node.logger.try_log ("Receiving packet");
	}

	auto data (node.network.buffer_container.allocate ());

	socket.async_receive_from (boost::asio::buffer (data->buffer, btcnew::network::buffer_size), data->endpoint,
	boost::asio::bind_executor (strand,
	[this, data] (boost::system::error_code const & error, std::size_t size_a) {
		if (!error && !stopped)
		{
			data->size = size_a;
			this->node.network.buffer_container.enqueue (data);
			this->receive ();
		}
		else
		{
			this->node.network.buffer_container.release (data);
			if (error)
			{
				if (this->node.config.logging.network_logging ())
				{
					this->node.logger.try_log (boost::str (boost::format ("UDP Receive error: %1%") % error.message ()));
				}
			}
			if (!stopped)
			{
				this->node.alarm.add (std::chrono::steady_clock::now () + std::chrono::seconds (5), [this] () { this->receive (); });
			}
		}
	}));
}

void btcnew::transport::udp_channels::start ()
{
	for (size_t i = 0; i < node.config.io_threads; ++i)
	{
		boost::asio::post (strand, [this] () {
			receive ();
		});
	}
	ongoing_keepalive ();
}

void btcnew::transport::udp_channels::stop ()
{
	// Stop and invalidate local endpoint
	stopped = true;
	btcnew::lock_guard<std::mutex> lock (mutex);
	local_endpoint = btcnew::endpoint (boost::asio::ip::address_v6::loopback (), 0);

	// On test-net, close directly to avoid address-reuse issues. On livenet, close
	// through the strand as multiple IO threads may access the socket.
	// clang-format off
	if (node.network_params.network.is_test_network ())
	{
		this->close_socket ();
	}
	else
	{
		boost::asio::dispatch (strand, [this] {
			this->close_socket ();
		});
	}
	// clang-format on
}

void btcnew::transport::udp_channels::close_socket ()
{
	boost::system::error_code ignored;
	this->socket.close (ignored);
	this->local_endpoint = btcnew::endpoint (boost::asio::ip::address_v6::loopback (), 0);
}

btcnew::endpoint btcnew::transport::udp_channels::get_local_endpoint () const
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	return local_endpoint;
}

namespace
{
class udp_message_visitor : public btcnew::message_visitor
{
public:
	udp_message_visitor (btcnew::node & node_a, btcnew::endpoint const & endpoint_a) :
	node (node_a),
	endpoint (endpoint_a)
	{
	}
	void keepalive (btcnew::keepalive const & message_a) override
	{
		if (!node.network.udp_channels.max_ip_connections (endpoint))
		{
			auto cookie (node.network.syn_cookies.assign (endpoint));
			if (cookie)
			{
				// New connection
				auto find_channel (node.network.udp_channels.channel (endpoint));
				if (find_channel)
				{
					node.network.send_node_id_handshake (find_channel, *cookie, boost::none);
					node.network.send_keepalive_self (find_channel);
				}
				else if (!node.network.tcp_channels.find_channel (btcnew::transport::map_endpoint_to_tcp (endpoint)))
				{
					// Don't start connection if TCP channel to same IP:port exists
					find_channel = std::make_shared<btcnew::transport::channel_udp> (node.network.udp_channels, endpoint, node.network_params.protocol.protocol_version);
					node.network.send_node_id_handshake (find_channel, *cookie, boost::none);
				}
			}
			// Check for special node port data
			auto peer0 (message_a.peers[0]);
			if (peer0.address () == boost::asio::ip::address_v6{} && peer0.port () != 0)
			{
				btcnew::endpoint new_endpoint (endpoint.address (), peer0.port ());
				node.network.merge_peer (new_endpoint);
			}
		}
		message (message_a);
	}
	void publish (btcnew::publish const & message_a) override
	{
		message (message_a);
	}
	void confirm_req (btcnew::confirm_req const & message_a) override
	{
		message (message_a);
	}
	void confirm_ack (btcnew::confirm_ack const & message_a) override
	{
		message (message_a);
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
		if (node.config.logging.network_node_id_handshake_logging ())
		{
			node.logger.try_log (boost::str (boost::format ("Received node_id_handshake message from %1% with query %2% and response ID %3%") % endpoint % (message_a.query ? message_a.query->to_string () : std::string ("[none]")) % (message_a.response ? message_a.response->first.to_node_id () : std::string ("[none]"))));
		}
		boost::optional<btcnew::uint256_union> out_query;
		boost::optional<btcnew::uint256_union> out_respond_to;
		if (message_a.query)
		{
			out_respond_to = message_a.query;
		}
		auto validated_response (false);
		if (message_a.response)
		{
			if (!node.network.syn_cookies.validate (endpoint, message_a.response->first, message_a.response->second))
			{
				validated_response = true;
				if (message_a.response->first != node.node_id.pub && !node.network.tcp_channels.find_node_id (message_a.response->first))
				{
					node.network.udp_channels.clean_node_id (endpoint, message_a.response->first);
					auto new_channel (node.network.udp_channels.insert (endpoint, message_a.header.version_using));
					if (new_channel)
					{
						node.network.udp_channels.modify (new_channel, [&message_a] (std::shared_ptr<btcnew::transport::channel_udp> channel_a) {
							channel_a->set_node_id (message_a.response->first);
							channel_a->set_last_packet_received (std::chrono::steady_clock::now ());
						});
					}
				}
			}
			else if (node.config.logging.network_node_id_handshake_logging ())
			{
				node.logger.try_log (boost::str (boost::format ("Failed to validate syn cookie signature %1% by %2%") % message_a.response->second.to_string () % message_a.response->first.to_account ()));
			}
		}
		if (!validated_response && node.network.udp_channels.channel (endpoint) == nullptr)
		{
			out_query = node.network.syn_cookies.assign (endpoint);
		}
		if (out_query || out_respond_to)
		{
			auto find_channel (node.network.udp_channels.channel (endpoint));
			if (!find_channel)
			{
				find_channel = std::make_shared<btcnew::transport::channel_udp> (node.network.udp_channels, endpoint, node.network_params.protocol.protocol_version);
			}
			node.network.send_node_id_handshake (find_channel, out_query, out_respond_to);
		}
		message (message_a);
	}
	void message (btcnew::message const & message_a)
	{
		auto find_channel (node.network.udp_channels.channel (endpoint));
		if (find_channel)
		{
			node.network.udp_channels.modify (find_channel, [] (std::shared_ptr<btcnew::transport::channel_udp> channel_a) {
				channel_a->set_last_packet_received (std::chrono::steady_clock::now ());
			});
			node.network.process_message (message_a, find_channel);
		}
	}
	btcnew::node & node;
	btcnew::endpoint endpoint;
};
}

void btcnew::transport::udp_channels::receive_action (btcnew::message_buffer * data_a)
{
	auto allowed_sender (true);
	if (data_a->endpoint == local_endpoint)
	{
		allowed_sender = false;
	}
	else if (data_a->endpoint.address ().to_v6 ().is_unspecified ())
	{
		allowed_sender = false;
	}
	else if (btcnew::transport::reserved_address (data_a->endpoint, node.config.allow_local_peers))
	{
		allowed_sender = false;
	}
	if (allowed_sender)
	{
		udp_message_visitor visitor (node, data_a->endpoint);
		btcnew::message_parser parser (node.block_uniquer, node.vote_uniquer, visitor, node.work);
		parser.deserialize_buffer (data_a->buffer, data_a->size);
		if (parser.status != btcnew::message_parser::parse_status::success)
		{
			node.stats.inc (btcnew::stat::type::error);

			switch (parser.status)
			{
				case btcnew::message_parser::parse_status::insufficient_work:
					// We've already increment error count, update detail only
					node.stats.inc_detail_only (btcnew::stat::type::error, btcnew::stat::detail::insufficient_work);
					break;
				case btcnew::message_parser::parse_status::invalid_magic:
					node.stats.inc (btcnew::stat::type::udp, btcnew::stat::detail::invalid_magic);
					break;
				case btcnew::message_parser::parse_status::invalid_network:
					node.stats.inc (btcnew::stat::type::udp, btcnew::stat::detail::invalid_network);
					break;
				case btcnew::message_parser::parse_status::invalid_header:
					node.stats.inc (btcnew::stat::type::udp, btcnew::stat::detail::invalid_header);
					break;
				case btcnew::message_parser::parse_status::invalid_message_type:
					node.stats.inc (btcnew::stat::type::udp, btcnew::stat::detail::invalid_message_type);
					break;
				case btcnew::message_parser::parse_status::invalid_keepalive_message:
					node.stats.inc (btcnew::stat::type::udp, btcnew::stat::detail::invalid_keepalive_message);
					break;
				case btcnew::message_parser::parse_status::invalid_publish_message:
					node.stats.inc (btcnew::stat::type::udp, btcnew::stat::detail::invalid_publish_message);
					break;
				case btcnew::message_parser::parse_status::invalid_confirm_req_message:
					node.stats.inc (btcnew::stat::type::udp, btcnew::stat::detail::invalid_confirm_req_message);
					break;
				case btcnew::message_parser::parse_status::invalid_confirm_ack_message:
					node.stats.inc (btcnew::stat::type::udp, btcnew::stat::detail::invalid_confirm_ack_message);
					break;
				case btcnew::message_parser::parse_status::invalid_node_id_handshake_message:
					node.stats.inc (btcnew::stat::type::udp, btcnew::stat::detail::invalid_node_id_handshake_message);
					break;
				case btcnew::message_parser::parse_status::outdated_version:
					node.stats.inc (btcnew::stat::type::udp, btcnew::stat::detail::outdated_version);
					break;
				case btcnew::message_parser::parse_status::success:
					/* Already checked, unreachable */
					break;
			}
		}
		else
		{
			node.stats.add (btcnew::stat::type::traffic_udp, btcnew::stat::dir::in, data_a->size);
		}
	}
	else
	{
		if (node.config.logging.network_packet_logging ())
		{
			node.logger.try_log (boost::str (boost::format ("Reserved sender %1%") % data_a->endpoint));
		}

		node.stats.inc_detail_only (btcnew::stat::type::error, btcnew::stat::detail::bad_sender);
	}
}

void btcnew::transport::udp_channels::process_packets ()
{
	while (!stopped)
	{
		auto data (node.network.buffer_container.dequeue ());
		if (data == nullptr)
		{
			break;
		}
		receive_action (data);
		node.network.buffer_container.release (data);
	}
}

std::shared_ptr<btcnew::transport::channel> btcnew::transport::udp_channels::create (btcnew::endpoint const & endpoint_a)
{
	return std::make_shared<btcnew::transport::channel_udp> (*this, endpoint_a, node.network_params.protocol.protocol_version);
}

bool btcnew::transport::udp_channels::max_ip_connections (btcnew::endpoint const & endpoint_a)
{
	btcnew::unique_lock<std::mutex> lock (mutex);
	bool result (channels.get<ip_address_tag> ().count (endpoint_a.address ()) >= btcnew::transport::max_peers_per_ip);
	return result;
}

bool btcnew::transport::udp_channels::reachout (btcnew::endpoint const & endpoint_a)
{
	// Don't overload single IP
	bool error = max_ip_connections (endpoint_a);
	if (!error)
	{
		auto endpoint_l (btcnew::transport::map_endpoint_to_v6 (endpoint_a));
		// Don't keepalive to nodes that already sent us something
		error |= channel (endpoint_l) != nullptr;
		btcnew::lock_guard<std::mutex> lock (mutex);
		auto existing (attempts.find (endpoint_l));
		error |= existing != attempts.end ();
		attempts.insert ({ endpoint_l, std::chrono::steady_clock::now () });
	}
	return error;
}

std::unique_ptr<btcnew::seq_con_info_component> btcnew::transport::udp_channels::collect_seq_con_info (std::string const & name)
{
	size_t channels_count = 0;
	size_t attemps_count = 0;
	{
		btcnew::lock_guard<std::mutex> guard (mutex);
		channels_count = channels.size ();
		attemps_count = attempts.size ();
	}

	auto composite = std::make_unique<seq_con_info_composite> (name);
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "channels", channels_count, sizeof (decltype (channels)::value_type) }));
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "attempts", attemps_count, sizeof (decltype (attempts)::value_type) }));

	return composite;
}

void btcnew::transport::udp_channels::purge (std::chrono::steady_clock::time_point const & cutoff_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	auto disconnect_cutoff (channels.get<last_packet_received_tag> ().lower_bound (cutoff_a));
	channels.get<last_packet_received_tag> ().erase (channels.get<last_packet_received_tag> ().begin (), disconnect_cutoff);
	// Remove keepalive attempt tracking for attempts older than cutoff
	auto attempts_cutoff (attempts.get<1> ().lower_bound (cutoff_a));
	attempts.get<1> ().erase (attempts.get<1> ().begin (), attempts_cutoff);
}

void btcnew::transport::udp_channels::ongoing_keepalive ()
{
	btcnew::keepalive message;
	node.network.random_fill (message.peers);
	std::vector<std::shared_ptr<btcnew::transport::channel_udp>> send_list;
	btcnew::unique_lock<std::mutex> lock (mutex);
	auto keepalive_cutoff (channels.get<last_packet_received_tag> ().lower_bound (std::chrono::steady_clock::now () - node.network_params.node.period));
	for (auto i (channels.get<last_packet_received_tag> ().begin ()); i != keepalive_cutoff; ++i)
	{
		send_list.push_back (i->channel);
	}
	lock.unlock ();
	for (auto & channel : send_list)
	{
		channel->send (message);
	}
	std::weak_ptr<btcnew::node> node_w (node.shared ());
	node.alarm.add (std::chrono::steady_clock::now () + node.network_params.node.period, [node_w] () {
		if (auto node_l = node_w.lock ())
		{
			node_l->network.udp_channels.ongoing_keepalive ();
		}
	});
}

void btcnew::transport::udp_channels::list (std::deque<std::shared_ptr<btcnew::transport::channel>> & deque_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	for (auto i (channels.begin ()), j (channels.end ()); i != j; ++i)
	{
		deque_a.push_back (i->channel);
	}
}

void btcnew::transport::udp_channels::modify (std::shared_ptr<btcnew::transport::channel_udp> channel_a, std::function<void (std::shared_ptr<btcnew::transport::channel_udp>)> modify_callback_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	auto existing (channels.get<endpoint_tag> ().find (channel_a->endpoint));
	if (existing != channels.get<endpoint_tag> ().end ())
	{
		channels.get<endpoint_tag> ().modify (existing, [modify_callback_a] (channel_udp_wrapper & wrapper_a) {
			modify_callback_a (wrapper_a.channel);
		});
	}
}

#include <btcnew/lib/stats.hpp>
#include <btcnew/node/node.hpp>
#include <btcnew/node/transport/tcp.hpp>

btcnew::transport::channel_tcp::channel_tcp (btcnew::node & node_a, std::weak_ptr<btcnew::socket> socket_a) :
channel (node_a),
socket (socket_a)
{
}

btcnew::transport::channel_tcp::~channel_tcp ()
{
	btcnew::lock_guard<std::mutex> lk (channel_mutex);
	// Close socket. Exception: socket is used by bootstrap_server
	if (auto socket_l = socket.lock ())
	{
		if (!server)
		{
			socket_l->close ();
		}
		// Remove response server
		if (auto response_server_l = response_server.lock ())
		{
			response_server_l->stop ();
		}
	}
}

size_t btcnew::transport::channel_tcp::hash_code () const
{
	std::hash<::btcnew::tcp_endpoint> hash;
	if (auto socket_l = socket.lock ())
	{
		return hash (socket_l->remote_endpoint ());
	}

	return 0;
}

bool btcnew::transport::channel_tcp::operator== (btcnew::transport::channel const & other_a) const
{
	bool result (false);
	auto other_l (dynamic_cast<btcnew::transport::channel_tcp const *> (&other_a));
	if (other_l != nullptr)
	{
		return *this == *other_l;
	}
	return result;
}

void btcnew::transport::channel_tcp::send_buffer (btcnew::shared_const_buffer const & buffer_a, btcnew::stat::detail detail_a, std::function<void (boost::system::error_code const &, size_t)> const & callback_a)
{
	if (auto socket_l = socket.lock ())
	{
		socket_l->async_write (buffer_a, tcp_callback (detail_a, socket_l->remote_endpoint (), callback_a));
	}
}

std::function<void (boost::system::error_code const &, size_t)> btcnew::transport::channel_tcp::callback (btcnew::stat::detail detail_a, std::function<void (boost::system::error_code const &, size_t)> const & callback_a) const
{
	return callback_a;
}

std::function<void (boost::system::error_code const &, size_t)> btcnew::transport::channel_tcp::tcp_callback (btcnew::stat::detail detail_a, btcnew::tcp_endpoint const & endpoint_a, std::function<void (boost::system::error_code const &, size_t)> const & callback_a) const
{
	// clang-format off
	return [endpoint_a, node = std::weak_ptr<btcnew::node> (node.shared ()), callback_a ](boost::system::error_code const & ec, size_t size_a)
	{
		if (auto node_l = node.lock ())
		{
			if (!ec)
			{
				node_l->network.tcp_channels.update (endpoint_a);
			}
			if (ec == boost::system::errc::host_unreachable)
			{
				node_l->stats.inc (btcnew::stat::type::error, btcnew::stat::detail::unreachable_host, btcnew::stat::dir::out);
			}
			if (callback_a)
			{
				callback_a (ec, size_a);
			}
		}
	};
	// clang-format on
}

std::string btcnew::transport::channel_tcp::to_string () const
{
	if (auto socket_l = socket.lock ())
	{
		return boost::str (boost::format ("%1%") % socket_l->remote_endpoint ());
	}
	return "";
}

btcnew::transport::tcp_channels::tcp_channels (btcnew::node & node_a) :
node (node_a)
{
}

bool btcnew::transport::tcp_channels::insert (std::shared_ptr<btcnew::transport::channel_tcp> channel_a, std::shared_ptr<btcnew::socket> socket_a, std::shared_ptr<btcnew::bootstrap_server> bootstrap_server_a)
{
	auto endpoint (channel_a->get_tcp_endpoint ());
	assert (endpoint.address ().is_v6 ());
	auto udp_endpoint (btcnew::transport::map_tcp_to_endpoint (endpoint));
	bool error (true);
	if (!node.network.not_a_peer (udp_endpoint, node.config.allow_local_peers) && !stopped)
	{
		btcnew::unique_lock<std::mutex> lock (mutex);
		auto existing (channels.get<endpoint_tag> ().find (endpoint));
		if (existing == channels.get<endpoint_tag> ().end ())
		{
			auto node_id (channel_a->get_node_id ());
			if (!channel_a->server)
			{
				channels.get<node_id_tag> ().erase (node_id);
			}
			channels.get<endpoint_tag> ().insert ({ channel_a, socket_a, bootstrap_server_a });
			error = false;
			lock.unlock ();
			node.network.channel_observer (channel_a);
			// Remove UDP channel to same IP:port if exists
			node.network.udp_channels.erase (udp_endpoint);
			// Remove UDP channels with same node ID
			node.network.udp_channels.clean_node_id (node_id);
		}
	}
	return error;
}

void btcnew::transport::tcp_channels::erase (btcnew::tcp_endpoint const & endpoint_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	channels.get<endpoint_tag> ().erase (endpoint_a);
}

size_t btcnew::transport::tcp_channels::size () const
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	return channels.size ();
}

std::shared_ptr<btcnew::transport::channel_tcp> btcnew::transport::tcp_channels::find_channel (btcnew::tcp_endpoint const & endpoint_a) const
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	std::shared_ptr<btcnew::transport::channel_tcp> result;
	auto existing (channels.get<endpoint_tag> ().find (endpoint_a));
	if (existing != channels.get<endpoint_tag> ().end ())
	{
		result = existing->channel;
	}
	return result;
}

std::unordered_set<std::shared_ptr<btcnew::transport::channel>> btcnew::transport::tcp_channels::random_set (size_t count_a) const
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

void btcnew::transport::tcp_channels::random_fill (std::array<btcnew::endpoint, 8> & target_a) const
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

bool btcnew::transport::tcp_channels::store_all (bool clear_peers)
{
	// We can't hold the mutex while starting a write transaction, so
	// we collect endpoints to be saved and then relase the lock.
	std::vector<btcnew::endpoint> endpoints;
	{
		btcnew::lock_guard<std::mutex> lock (mutex);
		endpoints.reserve (channels.size ());
		std::transform (channels.begin (), channels.end (),
		std::back_inserter (endpoints), [] (const auto & channel) { return btcnew::transport::map_tcp_to_endpoint (channel.endpoint ()); });
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

std::shared_ptr<btcnew::transport::channel_tcp> btcnew::transport::tcp_channels::find_node_id (btcnew::account const & node_id_a)
{
	std::shared_ptr<btcnew::transport::channel_tcp> result;
	btcnew::lock_guard<std::mutex> lock (mutex);
	auto existing (channels.get<node_id_tag> ().find (node_id_a));
	if (existing != channels.get<node_id_tag> ().end ())
	{
		result = existing->channel;
	}
	return result;
}

btcnew::tcp_endpoint btcnew::transport::tcp_channels::bootstrap_peer (uint8_t connection_protocol_version_min)
{
	btcnew::tcp_endpoint result (boost::asio::ip::address_v6::any (), 0);
	btcnew::lock_guard<std::mutex> lock (mutex);
	for (auto i (channels.get<last_bootstrap_attempt_tag> ().begin ()), n (channels.get<last_bootstrap_attempt_tag> ().end ()); i != n;)
	{
		if (i->channel->get_network_version () >= connection_protocol_version_min)
		{
			result = i->endpoint ();
			channels.get<last_bootstrap_attempt_tag> ().modify (i, [] (channel_tcp_wrapper & wrapper_a) {
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

void btcnew::transport::tcp_channels::process_message (btcnew::message const & message_a, btcnew::tcp_endpoint const & endpoint_a, btcnew::account const & node_id_a, std::shared_ptr<btcnew::socket> socket_a, btcnew::bootstrap_server_type type_a)
{
	if (!stopped)
	{
		auto channel (node.network.find_channel (btcnew::transport::map_tcp_to_endpoint (endpoint_a)));
		if (channel)
		{
			node.network.process_message (message_a, channel);
		}
		else
		{
			channel = node.network.find_node_id (node_id_a);
			if (channel)
			{
				node.network.process_message (message_a, channel);
			}
			else if (!node_id_a.is_zero ())
			{
				// Add temporary channel
				socket_a->set_writer_concurrency (btcnew::socket::concurrency::multi_writer);
				auto temporary_channel (std::make_shared<btcnew::transport::channel_tcp> (node, socket_a));
				assert (endpoint_a == temporary_channel->get_tcp_endpoint ());
				temporary_channel->set_node_id (node_id_a);
				temporary_channel->set_network_version (message_a.header.version_using);
				temporary_channel->set_last_packet_received (std::chrono::steady_clock::now ());
				temporary_channel->set_last_packet_sent (std::chrono::steady_clock::now ());
				temporary_channel->server = true;
				assert (type_a == btcnew::bootstrap_server_type::realtime || type_a == btcnew::bootstrap_server_type::realtime_response_server);
				// Don't insert temporary channels for response_server
				if (type_a == btcnew::bootstrap_server_type::realtime)
				{
					insert (temporary_channel, socket_a, nullptr);
				}
				node.network.process_message (message_a, temporary_channel);
			}
			else
			{
				// Initial node_id_handshake request without node ID
				assert (message_a.header.type == btcnew::message_type::node_id_handshake);
				assert (type_a == btcnew::bootstrap_server_type::undefined);
				node.stats.inc (btcnew::stat::type::message, btcnew::stat::detail::node_id_handshake, btcnew::stat::dir::in);
			}
		}
	}
}

void btcnew::transport::tcp_channels::process_keepalive (btcnew::keepalive const & message_a, btcnew::tcp_endpoint const & endpoint_a)
{
	if (!max_ip_connections (endpoint_a))
	{
		// Check for special node port data
		auto peer0 (message_a.peers[0]);
		if (peer0.address () == boost::asio::ip::address_v6{} && peer0.port () != 0)
		{
			btcnew::endpoint new_endpoint (endpoint_a.address (), peer0.port ());
			node.network.merge_peer (new_endpoint);
		}
		auto udp_channel (std::make_shared<btcnew::transport::channel_udp> (node.network.udp_channels, btcnew::transport::map_tcp_to_endpoint (endpoint_a), node.network_params.protocol.protocol_version));
		node.network.process_message (message_a, udp_channel);
	}
}

void btcnew::transport::tcp_channels::start ()
{
	ongoing_keepalive ();
}

void btcnew::transport::tcp_channels::stop ()
{
	stopped = true;
	btcnew::unique_lock<std::mutex> lock (mutex);
	// Close all TCP sockets
	for (auto i (channels.begin ()), j (channels.end ()); i != j; ++i)
	{
		if (i->socket)
		{
			i->socket->close ();
		}
		// Remove response server
		if (i->response_server)
		{
			i->response_server->stop ();
		}
	}
	channels.clear ();
	node_id_handshake_sockets.clear ();
}

bool btcnew::transport::tcp_channels::max_ip_connections (btcnew::tcp_endpoint const & endpoint_a)
{
	btcnew::unique_lock<std::mutex> lock (mutex);
	bool result (channels.get<ip_address_tag> ().count (endpoint_a.address ()) >= btcnew::transport::max_peers_per_ip);
	return result;
}

bool btcnew::transport::tcp_channels::reachout (btcnew::endpoint const & endpoint_a)
{
	auto tcp_endpoint (btcnew::transport::map_endpoint_to_tcp (endpoint_a));
	// Don't overload single IP
	bool error = max_ip_connections (tcp_endpoint);
	if (!error)
	{
		// Don't keepalive to nodes that already sent us something
		error |= find_channel (tcp_endpoint) != nullptr;
		btcnew::lock_guard<std::mutex> lock (mutex);
		auto existing (attempts.find (tcp_endpoint));
		error |= existing != attempts.end ();
		attempts.insert ({ tcp_endpoint, std::chrono::steady_clock::now () });
	}
	return error;
}

std::unique_ptr<btcnew::seq_con_info_component> btcnew::transport::tcp_channels::collect_seq_con_info (std::string const & name)
{
	size_t channels_count = 0;
	size_t attemps_count = 0;
	size_t node_id_handshake_sockets_count = 0;
	{
		btcnew::lock_guard<std::mutex> guard (mutex);
		channels_count = channels.size ();
		attemps_count = attempts.size ();
		node_id_handshake_sockets_count = node_id_handshake_sockets.size ();
	}

	auto composite = std::make_unique<seq_con_info_composite> (name);
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "channels", channels_count, sizeof (decltype (channels)::value_type) }));
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "attempts", attemps_count, sizeof (decltype (attempts)::value_type) }));
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "node_id_handshake_sockets", node_id_handshake_sockets_count, sizeof (decltype (node_id_handshake_sockets)::value_type) }));

	return composite;
}

void btcnew::transport::tcp_channels::purge (std::chrono::steady_clock::time_point const & cutoff_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	auto disconnect_cutoff (channels.get<last_packet_sent_tag> ().lower_bound (cutoff_a));
	channels.get<last_packet_sent_tag> ().erase (channels.get<last_packet_sent_tag> ().begin (), disconnect_cutoff);
	// Remove keepalive attempt tracking for attempts older than cutoff
	auto attempts_cutoff (attempts.get<1> ().lower_bound (cutoff_a));
	attempts.get<1> ().erase (attempts.get<1> ().begin (), attempts_cutoff);

	// Cleanup any sockets which may still be existing from failed node id handshakes
	node_id_handshake_sockets.erase (std::remove_if (node_id_handshake_sockets.begin (), node_id_handshake_sockets.end (), [this] (auto socket) {
		return channels.get<endpoint_tag> ().find (socket->remote_endpoint ()) == channels.get<endpoint_tag> ().end ();
	}),
	node_id_handshake_sockets.end ());
}

void btcnew::transport::tcp_channels::ongoing_keepalive ()
{
	btcnew::keepalive message;
	node.network.random_fill (message.peers);
	btcnew::unique_lock<std::mutex> lock (mutex);
	// Wake up channels
	std::vector<std::shared_ptr<btcnew::transport::channel_tcp>> send_list;
	auto keepalive_sent_cutoff (channels.get<last_packet_sent_tag> ().lower_bound (std::chrono::steady_clock::now () - node.network_params.node.period));
	for (auto i (channels.get<last_packet_sent_tag> ().begin ()); i != keepalive_sent_cutoff; ++i)
	{
		send_list.push_back (i->channel);
	}
	lock.unlock ();
	for (auto & channel : send_list)
	{
		channel->send (message);
	}
	// Attempt to start TCP connections to known UDP peers
	btcnew::tcp_endpoint invalid_endpoint (boost::asio::ip::address_v6::any (), 0);
	if (!node.network_params.network.is_test_network () && !node.flags.disable_udp)
	{
		size_t random_count (std::min (static_cast<size_t> (6), static_cast<size_t> (std::ceil (std::sqrt (node.network.udp_channels.size ())))));
		for (auto i (0); i <= random_count; ++i)
		{
			auto tcp_endpoint (node.network.udp_channels.bootstrap_peer (node.network_params.protocol.tcp_realtime_protocol_version_min));
			if (tcp_endpoint != invalid_endpoint && find_channel (tcp_endpoint) == nullptr)
			{
				start_tcp (btcnew::transport::map_tcp_to_endpoint (tcp_endpoint));
			}
		}
	}
	std::weak_ptr<btcnew::node> node_w (node.shared ());
	node.alarm.add (std::chrono::steady_clock::now () + node.network_params.node.half_period, [node_w] () {
		if (auto node_l = node_w.lock ())
		{
			if (!node_l->network.tcp_channels.stopped)
			{
				node_l->network.tcp_channels.ongoing_keepalive ();
			}
		}
	});
}

void btcnew::transport::tcp_channels::list (std::deque<std::shared_ptr<btcnew::transport::channel>> & deque_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	for (auto i (channels.begin ()), j (channels.end ()); i != j; ++i)
	{
		deque_a.push_back (i->channel);
	}
}

void btcnew::transport::tcp_channels::modify (std::shared_ptr<btcnew::transport::channel_tcp> channel_a, std::function<void (std::shared_ptr<btcnew::transport::channel_tcp>)> modify_callback_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	auto existing (channels.get<endpoint_tag> ().find (channel_a->get_tcp_endpoint ()));
	if (existing != channels.get<endpoint_tag> ().end ())
	{
		channels.get<endpoint_tag> ().modify (existing, [modify_callback_a] (channel_tcp_wrapper & wrapper_a) {
			modify_callback_a (wrapper_a.channel);
		});
	}
}

void btcnew::transport::tcp_channels::update (btcnew::tcp_endpoint const & endpoint_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	auto existing (channels.get<endpoint_tag> ().find (endpoint_a));
	if (existing != channels.get<endpoint_tag> ().end ())
	{
		channels.get<endpoint_tag> ().modify (existing, [] (channel_tcp_wrapper & wrapper_a) {
			wrapper_a.channel->set_last_packet_sent (std::chrono::steady_clock::now ());
		});
	}
}

bool btcnew::transport::tcp_channels::node_id_handhake_sockets_empty () const
{
	btcnew::lock_guard<std::mutex> guard (mutex);
	return node_id_handshake_sockets.empty ();
}

void btcnew::transport::tcp_channels::push_node_id_handshake_socket (std::shared_ptr<btcnew::socket> const & socket_a)
{
	btcnew::lock_guard<std::mutex> guard (mutex);
	node_id_handshake_sockets.push_back (socket_a);
}

void btcnew::transport::tcp_channels::remove_node_id_handshake_socket (std::shared_ptr<btcnew::socket> const & socket_a)
{
	std::weak_ptr<btcnew::node> node_w (node.shared ());
	if (auto node_l = node_w.lock ())
	{
		btcnew::lock_guard<std::mutex> guard (mutex);
		node_id_handshake_sockets.erase (std::remove (node_id_handshake_sockets.begin (), node_id_handshake_sockets.end (), socket_a), node_id_handshake_sockets.end ());
	}
}

void btcnew::transport::tcp_channels::start_tcp (btcnew::endpoint const & endpoint_a, std::function<void (std::shared_ptr<btcnew::transport::channel>)> const & callback_a)
{
	if (node.flags.disable_tcp_realtime)
	{
		node.network.tcp_channels.udp_fallback (endpoint_a, callback_a);
		return;
	}
	auto socket (std::make_shared<btcnew::socket> (node.shared_from_this (), boost::none, btcnew::socket::concurrency::multi_writer));
	std::weak_ptr<btcnew::socket> socket_w (socket);
	auto channel (std::make_shared<btcnew::transport::channel_tcp> (node, socket_w));
	std::weak_ptr<btcnew::node> node_w (node.shared ());
	socket->async_connect (btcnew::transport::map_endpoint_to_tcp (endpoint_a),
	[node_w, channel, socket, endpoint_a, callback_a] (boost::system::error_code const & ec) {
		if (auto node_l = node_w.lock ())
		{
			if (!ec && channel)
			{
				// TCP node ID handshake
				auto cookie (node_l->network.syn_cookies.assign (endpoint_a));
				btcnew::node_id_handshake message (cookie, boost::none);
				auto bytes = message.to_shared_const_buffer ();
				if (node_l->config.logging.network_node_id_handshake_logging ())
				{
					node_l->logger.try_log (boost::str (boost::format ("Node ID handshake request sent with node ID %1% to %2%: query %3%") % node_l->node_id.pub.to_node_id () % endpoint_a % (*cookie).to_string ()));
				}
				std::shared_ptr<std::vector<uint8_t>> receive_buffer (std::make_shared<std::vector<uint8_t>> ());
				receive_buffer->resize (256);
				node_l->network.tcp_channels.push_node_id_handshake_socket (socket);
				channel->send_buffer (bytes, btcnew::stat::detail::node_id_handshake, [node_w, channel, endpoint_a, receive_buffer, callback_a] (boost::system::error_code const & ec, size_t size_a) {
					if (auto node_l = node_w.lock ())
					{
						if (!ec && channel)
						{
							node_l->network.tcp_channels.start_tcp_receive_node_id (channel, endpoint_a, receive_buffer, callback_a);
						}
						else
						{
							if (auto socket_l = channel->socket.lock ())
							{
								node_l->network.tcp_channels.remove_node_id_handshake_socket (socket_l);
							}
							if (node_l->config.logging.network_node_id_handshake_logging ())
							{
								node_l->logger.try_log (boost::str (boost::format ("Error sending node_id_handshake to %1%: %2%") % endpoint_a % ec.message ()));
							}
							node_l->network.tcp_channels.udp_fallback (endpoint_a, callback_a);
						}
					}
				});
			}
			else
			{
				node_l->network.tcp_channels.udp_fallback (endpoint_a, callback_a);
			}
		}
	});
}

void btcnew::transport::tcp_channels::start_tcp_receive_node_id (std::shared_ptr<btcnew::transport::channel_tcp> channel_a, btcnew::endpoint const & endpoint_a, std::shared_ptr<std::vector<uint8_t>> receive_buffer_a, std::function<void (std::shared_ptr<btcnew::transport::channel>)> const & callback_a)
{
	std::weak_ptr<btcnew::node> node_w (node.shared ());
	if (auto socket_l = channel_a->socket.lock ())
	{
		// clang-format off
		auto cleanup_and_udp_fallback = [socket_w = channel_a->socket, node_w](btcnew::endpoint const & endpoint_a, std::function<void(std::shared_ptr<btcnew::transport::channel>)> const & callback_a) {
			if (auto node_l = node_w.lock ())
			{
				node_l->network.tcp_channels.udp_fallback (endpoint_a, callback_a);

				if (auto socket_l = socket_w.lock ())
				{
					node_l->network.tcp_channels.remove_node_id_handshake_socket (socket_l);
				}
			}
		};
		// clang-format on

		socket_l->async_read (receive_buffer_a, 8 + sizeof (btcnew::account) + sizeof (btcnew::account) + sizeof (btcnew::signature), [node_w, channel_a, endpoint_a, receive_buffer_a, callback_a, cleanup_and_udp_fallback] (boost::system::error_code const & ec, size_t size_a) {
			if (auto node_l = node_w.lock ())
			{
				if (!ec && channel_a)
				{
					node_l->stats.inc (btcnew::stat::type::message, btcnew::stat::detail::node_id_handshake, btcnew::stat::dir::in);
					auto error (false);
					btcnew::bufferstream stream (receive_buffer_a->data (), size_a);
					btcnew::message_header header (error, stream);
					if (!error && header.type == btcnew::message_type::node_id_handshake && header.version_using >= node_l->network_params.protocol.protocol_version_min)
					{
						btcnew::node_id_handshake message (error, stream, header);
						if (!error && message.response && message.query)
						{
							channel_a->set_network_version (header.version_using);
							auto node_id (message.response->first);
							bool process (!node_l->network.syn_cookies.validate (endpoint_a, node_id, message.response->second) && node_id != node_l->node_id.pub);
							if (process)
							{
								/* If node ID is known, don't establish new connection
								   Exception: temporary channels from bootstrap_server */
								auto existing_channel (node_l->network.tcp_channels.find_node_id (node_id));
								if (existing_channel)
								{
									process = existing_channel->server;
								}
							}
							if (process)
							{
								channel_a->set_node_id (node_id);
								channel_a->set_last_packet_received (std::chrono::steady_clock::now ());
								boost::optional<std::pair<btcnew::account, btcnew::signature>> response (std::make_pair (node_l->node_id.pub, btcnew::sign_message (node_l->node_id.prv, node_l->node_id.pub, *message.query)));
								btcnew::node_id_handshake response_message (boost::none, response);
								auto bytes = response_message.to_shared_const_buffer ();
								if (node_l->config.logging.network_node_id_handshake_logging ())
								{
									node_l->logger.try_log (boost::str (boost::format ("Node ID handshake response sent with node ID %1% to %2%: query %3%") % node_l->node_id.pub.to_node_id () % endpoint_a % (*message.query).to_string ()));
								}
								channel_a->send_buffer (bytes, btcnew::stat::detail::node_id_handshake, [node_w, channel_a, endpoint_a, callback_a, cleanup_and_udp_fallback] (boost::system::error_code const & ec, size_t size_a) {
									if (auto node_l = node_w.lock ())
									{
										if (!ec && channel_a)
										{
											// Insert new node ID connection
											if (auto socket_l = channel_a->socket.lock ())
											{
												channel_a->set_last_packet_sent (std::chrono::steady_clock::now ());
												auto response_server = std::make_shared<btcnew::bootstrap_server> (socket_l, node_l);
												node_l->network.tcp_channels.insert (channel_a, socket_l, response_server);
												if (callback_a)
												{
													callback_a (channel_a);
												}
												// Listen for possible responses
												response_server->type = btcnew::bootstrap_server_type::realtime_response_server;
												response_server->remote_node_id = channel_a->get_node_id ();
												response_server->receive ();
												node_l->network.tcp_channels.remove_node_id_handshake_socket (socket_l);
											}
										}
										else
										{
											if (node_l->config.logging.network_node_id_handshake_logging ())
											{
												node_l->logger.try_log (boost::str (boost::format ("Error sending node_id_handshake to %1%: %2%") % endpoint_a % ec.message ()));
											}
											cleanup_and_udp_fallback (endpoint_a, callback_a);
										}
									}
								});
							}
						}
						else
						{
							cleanup_and_udp_fallback (endpoint_a, callback_a);
						}
					}
					else
					{
						cleanup_and_udp_fallback (endpoint_a, callback_a);
					}
				}
				else
				{
					if (node_l->config.logging.network_node_id_handshake_logging ())
					{
						node_l->logger.try_log (boost::str (boost::format ("Error reading node_id_handshake from %1%: %2%") % endpoint_a % ec.message ()));
					}
					cleanup_and_udp_fallback (endpoint_a, callback_a);
				}
			}
		});
	}
}

void btcnew::transport::tcp_channels::udp_fallback (btcnew::endpoint const & endpoint_a, std::function<void (std::shared_ptr<btcnew::transport::channel>)> const & callback_a)
{
	if (callback_a && !node.flags.disable_udp)
	{
		auto channel_udp (node.network.udp_channels.create (endpoint_a));
		callback_a (channel_udp);
	}
}

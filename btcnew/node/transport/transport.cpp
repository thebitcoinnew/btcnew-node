#include <btcnew/node/common.hpp>
#include <btcnew/node/node.hpp>
#include <btcnew/node/transport/transport.hpp>

#include <numeric>

namespace
{
class callback_visitor : public btcnew::message_visitor
{
public:
	void keepalive (btcnew::keepalive const & message_a) override
	{
		result = btcnew::stat::detail::keepalive;
	}
	void publish (btcnew::publish const & message_a) override
	{
		result = btcnew::stat::detail::publish;
	}
	void confirm_req (btcnew::confirm_req const & message_a) override
	{
		result = btcnew::stat::detail::confirm_req;
	}
	void confirm_ack (btcnew::confirm_ack const & message_a) override
	{
		result = btcnew::stat::detail::confirm_ack;
	}
	void bulk_pull (btcnew::bulk_pull const & message_a) override
	{
		result = btcnew::stat::detail::bulk_pull;
	}
	void bulk_pull_account (btcnew::bulk_pull_account const & message_a) override
	{
		result = btcnew::stat::detail::bulk_pull_account;
	}
	void bulk_push (btcnew::bulk_push const & message_a) override
	{
		result = btcnew::stat::detail::bulk_push;
	}
	void frontier_req (btcnew::frontier_req const & message_a) override
	{
		result = btcnew::stat::detail::frontier_req;
	}
	void node_id_handshake (btcnew::node_id_handshake const & message_a) override
	{
		result = btcnew::stat::detail::node_id_handshake;
	}
	btcnew::stat::detail result;
};
}

btcnew::endpoint btcnew::transport::map_endpoint_to_v6 (btcnew::endpoint const & endpoint_a)
{
	auto endpoint_l (endpoint_a);
	if (endpoint_l.address ().is_v4 ())
	{
		endpoint_l = btcnew::endpoint (boost::asio::ip::address_v6::v4_mapped (endpoint_l.address ().to_v4 ()), endpoint_l.port ());
	}
	return endpoint_l;
}

btcnew::endpoint btcnew::transport::map_tcp_to_endpoint (btcnew::tcp_endpoint const & endpoint_a)
{
	return btcnew::endpoint (endpoint_a.address (), endpoint_a.port ());
}

btcnew::tcp_endpoint btcnew::transport::map_endpoint_to_tcp (btcnew::endpoint const & endpoint_a)
{
	return btcnew::tcp_endpoint (endpoint_a.address (), endpoint_a.port ());
}

btcnew::transport::channel::channel (btcnew::node & node_a) :
limiter (node_a.config.bandwidth_limit),
node (node_a)
{
	set_network_version (node_a.network_params.protocol.protocol_version);
}

void btcnew::transport::channel::send (btcnew::message const & message_a, std::function<void (boost::system::error_code const &, size_t)> const & callback_a, bool const is_droppable_a)
{
	callback_visitor visitor;
	message_a.visit (visitor);
	auto buffer (message_a.to_shared_const_buffer ());
	auto detail (visitor.result);
	if (!is_droppable_a || !limiter.should_drop (buffer.size ()))
	{
		send_buffer (buffer, detail, callback_a);
		node.stats.inc (btcnew::stat::type::message, detail, btcnew::stat::dir::out);
	}
	else
	{
		node.stats.inc (btcnew::stat::type::drop, detail, btcnew::stat::dir::out);
		if (node.config.logging.network_packet_logging ())
		{
			auto key = static_cast<uint8_t> (detail) << 8;
			node.logger.always_log (boost::str (boost::format ("%1% of size %2% dropped") % node.stats.detail_to_string (key) % buffer.size ()));
		}
	}
}

namespace
{
boost::asio::ip::address_v6 mapped_from_v4_bytes (unsigned long address_a)
{
	return boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4 (address_a));
}
}

bool btcnew::transport::reserved_address (btcnew::endpoint const & endpoint_a, bool allow_local_peers)
{
	assert (endpoint_a.address ().is_v6 ());
	auto bytes (endpoint_a.address ().to_v6 ());
	auto result (false);
	static auto const rfc1700_min (mapped_from_v4_bytes (0x00000000ul));
	static auto const rfc1700_max (mapped_from_v4_bytes (0x00fffffful));
	static auto const rfc1918_1_min (mapped_from_v4_bytes (0x0a000000ul));
	static auto const rfc1918_1_max (mapped_from_v4_bytes (0x0afffffful));
	static auto const rfc1918_2_min (mapped_from_v4_bytes (0xac100000ul));
	static auto const rfc1918_2_max (mapped_from_v4_bytes (0xac1ffffful));
	static auto const rfc1918_3_min (mapped_from_v4_bytes (0xc0a80000ul));
	static auto const rfc1918_3_max (mapped_from_v4_bytes (0xc0a8fffful));
	static auto const rfc6598_min (mapped_from_v4_bytes (0x64400000ul));
	static auto const rfc6598_max (mapped_from_v4_bytes (0x647ffffful));
	static auto const rfc5737_1_min (mapped_from_v4_bytes (0xc0000200ul));
	static auto const rfc5737_1_max (mapped_from_v4_bytes (0xc00002fful));
	static auto const rfc5737_2_min (mapped_from_v4_bytes (0xc6336400ul));
	static auto const rfc5737_2_max (mapped_from_v4_bytes (0xc63364fful));
	static auto const rfc5737_3_min (mapped_from_v4_bytes (0xcb007100ul));
	static auto const rfc5737_3_max (mapped_from_v4_bytes (0xcb0071fful));
	static auto const ipv4_multicast_min (mapped_from_v4_bytes (0xe0000000ul));
	static auto const ipv4_multicast_max (mapped_from_v4_bytes (0xeffffffful));
	static auto const rfc6890_min (mapped_from_v4_bytes (0xf0000000ul));
	static auto const rfc6890_max (mapped_from_v4_bytes (0xfffffffful));
	static auto const rfc6666_min (boost::asio::ip::address_v6::from_string ("100::"));
	static auto const rfc6666_max (boost::asio::ip::address_v6::from_string ("100::ffff:ffff:ffff:ffff"));
	static auto const rfc3849_min (boost::asio::ip::address_v6::from_string ("2001:db8::"));
	static auto const rfc3849_max (boost::asio::ip::address_v6::from_string ("2001:db8:ffff:ffff:ffff:ffff:ffff:ffff"));
	static auto const rfc4193_min (boost::asio::ip::address_v6::from_string ("fc00::"));
	static auto const rfc4193_max (boost::asio::ip::address_v6::from_string ("fd00:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
	static auto const ipv6_multicast_min (boost::asio::ip::address_v6::from_string ("ff00::"));
	static auto const ipv6_multicast_max (boost::asio::ip::address_v6::from_string ("ff00:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
	if (endpoint_a.port () == 0)
	{
		result = true;
	}
	else if (bytes >= rfc1700_min && bytes <= rfc1700_max)
	{
		result = true;
	}
	else if (bytes >= rfc5737_1_min && bytes <= rfc5737_1_max)
	{
		result = true;
	}
	else if (bytes >= rfc5737_2_min && bytes <= rfc5737_2_max)
	{
		result = true;
	}
	else if (bytes >= rfc5737_3_min && bytes <= rfc5737_3_max)
	{
		result = true;
	}
	else if (bytes >= ipv4_multicast_min && bytes <= ipv4_multicast_max)
	{
		result = true;
	}
	else if (bytes >= rfc6890_min && bytes <= rfc6890_max)
	{
		result = true;
	}
	else if (bytes >= rfc6666_min && bytes <= rfc6666_max)
	{
		result = true;
	}
	else if (bytes >= rfc3849_min && bytes <= rfc3849_max)
	{
		result = true;
	}
	else if (bytes >= ipv6_multicast_min && bytes <= ipv6_multicast_max)
	{
		result = true;
	}
	else if (!allow_local_peers)
	{
		if (bytes >= rfc1918_1_min && bytes <= rfc1918_1_max)
		{
			result = true;
		}
		else if (bytes >= rfc1918_2_min && bytes <= rfc1918_2_max)
		{
			result = true;
		}
		else if (bytes >= rfc1918_3_min && bytes <= rfc1918_3_max)
		{
			result = true;
		}
		else if (bytes >= rfc6598_min && bytes <= rfc6598_max)
		{
			result = true;
		}
		else if (bytes >= rfc4193_min && bytes <= rfc4193_max)
		{
			result = true;
		}
	}
	return result;
}

using namespace std::chrono_literals;

btcnew::bandwidth_limiter::bandwidth_limiter (const size_t limit_a) :
next_trend (std::chrono::steady_clock::now () + 50ms),
limit (limit_a),
rate (0),
trended_rate (0)
{
}

bool btcnew::bandwidth_limiter::should_drop (const size_t & message_size)
{
	bool result (false);
	if (limit == 0) //never drop if limit is 0
	{
		return result;
	}
	btcnew::lock_guard<std::mutex> lock (mutex);

	if (message_size > limit / rate_buffer.size () || rate + message_size > limit)
	{
		result = true;
	}
	else
	{
		rate = rate + message_size;
	}
	if (next_trend < std::chrono::steady_clock::now ())
	{
		next_trend = std::chrono::steady_clock::now () + 50ms;
		rate_buffer.push_back (rate);
		trended_rate = std::accumulate (rate_buffer.begin (), rate_buffer.end (), size_t{ 0 }) / rate_buffer.size ();
		rate = 0;
	}
	return result;
}

size_t btcnew::bandwidth_limiter::get_rate ()
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	return trended_rate;
}

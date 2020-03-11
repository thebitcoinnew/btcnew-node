#pragma once

#include <btcnew/lib/stats.hpp>
#include <btcnew/node/common.hpp>
#include <btcnew/node/socket.hpp>

#include <unordered_set>

namespace btcnew
{
class bandwidth_limiter final
{
public:
	// initialize with rate 0 = unbounded
	bandwidth_limiter (const size_t);
	bool should_drop (const size_t &);
	size_t get_rate ();

private:
	//last time rate was adjusted
	std::chrono::steady_clock::time_point next_trend;
	//trend rate over 20 poll periods
	boost::circular_buffer<size_t> rate_buffer{ 20, 0 };
	//limit bandwidth to
	const size_t limit;
	//rate, increment if message_size + rate < rate
	size_t rate;
	//trended rate to even out spikes in traffic
	size_t trended_rate;
	std::mutex mutex;
};
namespace transport
{
	class message;
	btcnew::endpoint map_endpoint_to_v6 (btcnew::endpoint const &);
	btcnew::endpoint map_tcp_to_endpoint (btcnew::tcp_endpoint const &);
	btcnew::tcp_endpoint map_endpoint_to_tcp (btcnew::endpoint const &);
	// Unassigned, reserved, self
	bool reserved_address (btcnew::endpoint const &, bool = false);
	// Maximum number of peers per IP
	static size_t constexpr max_peers_per_ip = 10;
	static std::chrono::seconds constexpr syn_cookie_cutoff = std::chrono::seconds (5);
	enum class transport_type : uint8_t
	{
		undefined = 0,
		udp = 1,
		tcp = 2
	};
	class channel
	{
	public:
		channel (btcnew::node &);
		virtual ~channel () = default;
		virtual size_t hash_code () const = 0;
		virtual bool operator== (btcnew::transport::channel const &) const = 0;
		void send (btcnew::message const &, std::function<void (boost::system::error_code const &, size_t)> const & = nullptr, bool const = true);
		virtual void send_buffer (btcnew::shared_const_buffer const &, btcnew::stat::detail, std::function<void (boost::system::error_code const &, size_t)> const & = nullptr) = 0;
		virtual std::function<void (boost::system::error_code const &, size_t)> callback (btcnew::stat::detail, std::function<void (boost::system::error_code const &, size_t)> const & = nullptr) const = 0;
		virtual std::string to_string () const = 0;
		virtual btcnew::endpoint get_endpoint () const = 0;
		virtual btcnew::tcp_endpoint get_tcp_endpoint () const = 0;
		virtual btcnew::transport::transport_type get_type () const = 0;

		std::chrono::steady_clock::time_point get_last_bootstrap_attempt () const
		{
			btcnew::lock_guard<std::mutex> lk (channel_mutex);
			return last_bootstrap_attempt;
		}

		void set_last_bootstrap_attempt (std::chrono::steady_clock::time_point const time_a)
		{
			btcnew::lock_guard<std::mutex> lk (channel_mutex);
			last_bootstrap_attempt = time_a;
		}

		std::chrono::steady_clock::time_point get_last_packet_received () const
		{
			btcnew::lock_guard<std::mutex> lk (channel_mutex);
			return last_packet_received;
		}

		void set_last_packet_received (std::chrono::steady_clock::time_point const time_a)
		{
			btcnew::lock_guard<std::mutex> lk (channel_mutex);
			last_packet_received = time_a;
		}

		std::chrono::steady_clock::time_point get_last_packet_sent () const
		{
			btcnew::lock_guard<std::mutex> lk (channel_mutex);
			return last_packet_sent;
		}

		void set_last_packet_sent (std::chrono::steady_clock::time_point const time_a)
		{
			btcnew::lock_guard<std::mutex> lk (channel_mutex);
			last_packet_sent = time_a;
		}

		boost::optional<btcnew::account> get_node_id_optional () const
		{
			btcnew::lock_guard<std::mutex> lk (channel_mutex);
			return node_id;
		}

		btcnew::account get_node_id () const
		{
			btcnew::lock_guard<std::mutex> lk (channel_mutex);
			if (node_id.is_initialized ())
			{
				return node_id.get ();
			}
			else
			{
				return 0;
			}
		}

		void set_node_id (btcnew::account node_id_a)
		{
			btcnew::lock_guard<std::mutex> lk (channel_mutex);
			node_id = node_id_a;
		}

		uint8_t get_network_version () const
		{
			return network_version;
		}

		void set_network_version (uint8_t network_version_a)
		{
			network_version = network_version_a;
		}

		mutable std::mutex channel_mutex;
		btcnew::bandwidth_limiter limiter;

	private:
		std::chrono::steady_clock::time_point last_bootstrap_attempt{ std::chrono::steady_clock::time_point () };
		std::chrono::steady_clock::time_point last_packet_received{ std::chrono::steady_clock::time_point () };
		std::chrono::steady_clock::time_point last_packet_sent{ std::chrono::steady_clock::time_point () };
		boost::optional<btcnew::account> node_id{ boost::none };
		std::atomic<uint8_t> network_version{ 0 };

	protected:
		btcnew::node & node;
	};
} // namespace transport
} // namespace btcnew

namespace std
{
template <>
struct hash<::btcnew::transport::channel>
{
	size_t operator() (::btcnew::transport::channel const & channel_a) const
	{
		return channel_a.hash_code ();
	}
};
template <>
struct equal_to<std::reference_wrapper<::btcnew::transport::channel const>>
{
	bool operator() (std::reference_wrapper<::btcnew::transport::channel const> const & lhs, std::reference_wrapper<::btcnew::transport::channel const> const & rhs) const
	{
		return lhs.get () == rhs.get ();
	}
};
}

namespace boost
{
template <>
struct hash<::btcnew::transport::channel>
{
	size_t operator() (::btcnew::transport::channel const & channel_a) const
	{
		std::hash<::btcnew::transport::channel> hash;
		return hash (channel_a);
	}
};
template <>
struct hash<std::reference_wrapper<::btcnew::transport::channel const>>
{
	size_t operator() (std::reference_wrapper<::btcnew::transport::channel const> const & channel_a) const
	{
		std::hash<::btcnew::transport::channel> hash;
		return hash (channel_a.get ());
	}
};
}

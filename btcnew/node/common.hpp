#pragma once

#include <btcnew/boost/asio.hpp>
#include <btcnew/crypto_lib/random_pool.hpp>
#include <btcnew/lib/asio.hpp>
#include <btcnew/lib/config.hpp>
#include <btcnew/lib/memory.hpp>
#include <btcnew/secure/common.hpp>

#include <bitset>

namespace btcnew
{
using endpoint = boost::asio::ip::udp::endpoint;
bool parse_port (std::string const &, uint16_t &);
bool parse_address_port (std::string const &, boost::asio::ip::address &, uint16_t &);
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
bool parse_endpoint (std::string const &, btcnew::endpoint &);
bool parse_tcp_endpoint (std::string const &, btcnew::tcp_endpoint &);
}

namespace
{
uint64_t ip_address_hash_raw (boost::asio::ip::address const & ip_a, uint16_t port = 0)
{
	static btcnew::random_constants constants;
	assert (ip_a.is_v6 ());
	uint64_t result;
	btcnew::uint128_union address;
	address.bytes = ip_a.to_v6 ().to_bytes ();
	blake2b_state state;
	blake2b_init (&state, sizeof (result));
	blake2b_update (&state, constants.random_128.bytes.data (), constants.random_128.bytes.size ());
	if (port != 0)
	{
		blake2b_update (&state, &port, sizeof (port));
	}
	blake2b_update (&state, address.bytes.data (), address.bytes.size ());
	blake2b_final (&state, &result, sizeof (result));
	return result;
}
uint64_t endpoint_hash_raw (btcnew::endpoint const & endpoint_a)
{
	uint64_t result (ip_address_hash_raw (endpoint_a.address (), endpoint_a.port ()));
	return result;
}
uint64_t endpoint_hash_raw (btcnew::tcp_endpoint const & endpoint_a)
{
	uint64_t result (ip_address_hash_raw (endpoint_a.address (), endpoint_a.port ()));
	return result;
}

template <size_t size>
struct endpoint_hash
{
};
template <>
struct endpoint_hash<8>
{
	size_t operator() (btcnew::endpoint const & endpoint_a) const
	{
		return endpoint_hash_raw (endpoint_a);
	}
	size_t operator() (btcnew::tcp_endpoint const & endpoint_a) const
	{
		return endpoint_hash_raw (endpoint_a);
	}
};
template <>
struct endpoint_hash<4>
{
	size_t operator() (btcnew::endpoint const & endpoint_a) const
	{
		uint64_t big (endpoint_hash_raw (endpoint_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
	size_t operator() (btcnew::tcp_endpoint const & endpoint_a) const
	{
		uint64_t big (endpoint_hash_raw (endpoint_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
};
template <size_t size>
struct ip_address_hash
{
};
template <>
struct ip_address_hash<8>
{
	size_t operator() (boost::asio::ip::address const & ip_address_a) const
	{
		return ip_address_hash_raw (ip_address_a);
	}
};
template <>
struct ip_address_hash<4>
{
	size_t operator() (boost::asio::ip::address const & ip_address_a) const
	{
		uint64_t big (ip_address_hash_raw (ip_address_a));
		uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
		return result;
	}
};
}

namespace std
{
template <>
struct hash<::btcnew::endpoint>
{
	size_t operator() (::btcnew::endpoint const & endpoint_a) const
	{
		endpoint_hash<sizeof (size_t)> ehash;
		return ehash (endpoint_a);
	}
};
template <>
struct hash<::btcnew::tcp_endpoint>
{
	size_t operator() (::btcnew::tcp_endpoint const & endpoint_a) const
	{
		endpoint_hash<sizeof (size_t)> ehash;
		return ehash (endpoint_a);
	}
};
template <>
struct hash<boost::asio::ip::address>
{
	size_t operator() (boost::asio::ip::address const & ip_a) const
	{
		ip_address_hash<sizeof (size_t)> ihash;
		return ihash (ip_a);
	}
};
}
namespace boost
{
template <>
struct hash<::btcnew::endpoint>
{
	size_t operator() (::btcnew::endpoint const & endpoint_a) const
	{
		std::hash<::btcnew::endpoint> hash;
		return hash (endpoint_a);
	}
};
template <>
struct hash<::btcnew::tcp_endpoint>
{
	size_t operator() (::btcnew::tcp_endpoint const & endpoint_a) const
	{
		std::hash<::btcnew::tcp_endpoint> hash;
		return hash (endpoint_a);
	}
};
template <>
struct hash<boost::asio::ip::address>
{
	size_t operator() (boost::asio::ip::address const & ip_a) const
	{
		std::hash<boost::asio::ip::address> hash;
		return hash (ip_a);
	}
};
}

namespace btcnew
{
/**
 * Message types are serialized to the network and existing values must thus never change as
 * types are added, removed and reordered in the enum.
 */
enum class message_type : uint8_t
{
	invalid = 0x0,
	not_a_type = 0x1,
	keepalive = 0x2,
	publish = 0x3,
	confirm_req = 0x4,
	confirm_ack = 0x5,
	bulk_pull = 0x6,
	bulk_push = 0x7,
	frontier_req = 0x8,
	/* deleted 0x9 */
	node_id_handshake = 0x0a,
	bulk_pull_account = 0x0b
};
enum class bulk_pull_account_flags : uint8_t
{
	pending_hash_and_amount = 0x0,
	pending_address_only = 0x1,
	pending_hash_amount_and_address = 0x2
};
class message_visitor;
class message_header final
{
public:
	explicit message_header (btcnew::message_type);
	message_header (bool &, btcnew::stream &);
	void serialize (btcnew::stream &) const;
	bool deserialize (btcnew::stream &);
	btcnew::block_type block_type () const;
	void block_type_set (btcnew::block_type);
	uint8_t count_get () const;
	void count_set (uint8_t);
	uint8_t version_max;
	uint8_t version_using;
	uint8_t version_min;
	btcnew::message_type type;
	std::bitset<16> extensions;

	void flag_set (uint8_t);
	static uint8_t constexpr bulk_pull_count_present_flag = 0;
	bool bulk_pull_is_count_present () const;
	static uint8_t constexpr node_id_handshake_query_flag = 0;
	static uint8_t constexpr node_id_handshake_response_flag = 1;
	bool node_id_handshake_is_query () const;
	bool node_id_handshake_is_response () const;

	/** Size of the payload in bytes. For some messages, the payload size is based on header flags. */
	size_t payload_length_bytes () const;

	static std::bitset<16> constexpr block_type_mask = std::bitset<16> (0x0f00);
	static std::bitset<16> constexpr count_mask = std::bitset<16> (0xf000);
};
class message
{
public:
	explicit message (btcnew::message_type);
	explicit message (btcnew::message_header const &);
	virtual ~message () = default;
	virtual void serialize (btcnew::stream &) const = 0;
	virtual void visit (btcnew::message_visitor &) const = 0;
	std::shared_ptr<std::vector<uint8_t>> to_bytes () const
	{
		auto bytes = std::make_shared<std::vector<uint8_t>> ();
		btcnew::vectorstream stream (*bytes);
		serialize (stream);
		return bytes;
	}
	btcnew::shared_const_buffer to_shared_const_buffer () const
	{
		return shared_const_buffer (to_bytes ());
	}
	btcnew::message_header header;
};
class work_pool;
class message_parser final
{
public:
	enum class parse_status
	{
		success,
		insufficient_work,
		invalid_header,
		invalid_message_type,
		invalid_keepalive_message,
		invalid_publish_message,
		invalid_confirm_req_message,
		invalid_confirm_ack_message,
		invalid_node_id_handshake_message,
		outdated_version,
		invalid_magic,
		invalid_network
	};
	message_parser (btcnew::block_uniquer &, btcnew::vote_uniquer &, btcnew::message_visitor &, btcnew::work_pool &);
	void deserialize_buffer (uint8_t const *, size_t);
	void deserialize_keepalive (btcnew::stream &, btcnew::message_header const &);
	void deserialize_publish (btcnew::stream &, btcnew::message_header const &);
	void deserialize_confirm_req (btcnew::stream &, btcnew::message_header const &);
	void deserialize_confirm_ack (btcnew::stream &, btcnew::message_header const &);
	void deserialize_node_id_handshake (btcnew::stream &, btcnew::message_header const &);
	bool at_end (btcnew::stream &);
	btcnew::block_uniquer & block_uniquer;
	btcnew::vote_uniquer & vote_uniquer;
	btcnew::message_visitor & visitor;
	btcnew::work_pool & pool;
	parse_status status;
	std::string status_string ();
	static const size_t max_safe_udp_message_size;
};
class keepalive final : public message
{
public:
	keepalive ();
	keepalive (bool &, btcnew::stream &, btcnew::message_header const &);
	void visit (btcnew::message_visitor &) const override;
	void serialize (btcnew::stream &) const override;
	bool deserialize (btcnew::stream &);
	bool operator== (btcnew::keepalive const &) const;
	std::array<btcnew::endpoint, 8> peers;
	static size_t constexpr size = 8 * (16 + 2);
};
class publish final : public message
{
public:
	publish (bool &, btcnew::stream &, btcnew::message_header const &, btcnew::block_uniquer * = nullptr);
	explicit publish (std::shared_ptr<btcnew::block>);
	void visit (btcnew::message_visitor &) const override;
	void serialize (btcnew::stream &) const override;
	bool deserialize (btcnew::stream &, btcnew::block_uniquer * = nullptr);
	bool operator== (btcnew::publish const &) const;
	std::shared_ptr<btcnew::block> block;
};
class confirm_req final : public message
{
public:
	confirm_req (bool &, btcnew::stream &, btcnew::message_header const &, btcnew::block_uniquer * = nullptr);
	explicit confirm_req (std::shared_ptr<btcnew::block>);
	confirm_req (std::vector<std::pair<btcnew::block_hash, btcnew::root>> const &);
	confirm_req (btcnew::block_hash const &, btcnew::root const &);
	void serialize (btcnew::stream &) const override;
	bool deserialize (btcnew::stream &, btcnew::block_uniquer * = nullptr);
	void visit (btcnew::message_visitor &) const override;
	bool operator== (btcnew::confirm_req const &) const;
	std::shared_ptr<btcnew::block> block;
	std::vector<std::pair<btcnew::block_hash, btcnew::root>> roots_hashes;
	std::string roots_string () const;
	static size_t size (btcnew::block_type, size_t = 0);
};
class confirm_ack final : public message
{
public:
	confirm_ack (bool &, btcnew::stream &, btcnew::message_header const &, btcnew::vote_uniquer * = nullptr);
	explicit confirm_ack (std::shared_ptr<btcnew::vote>);
	void serialize (btcnew::stream &) const override;
	void visit (btcnew::message_visitor &) const override;
	bool operator== (btcnew::confirm_ack const &) const;
	std::shared_ptr<btcnew::vote> vote;
	static size_t size (btcnew::block_type, size_t = 0);
};
class frontier_req final : public message
{
public:
	frontier_req ();
	frontier_req (bool &, btcnew::stream &, btcnew::message_header const &);
	void serialize (btcnew::stream &) const override;
	bool deserialize (btcnew::stream &);
	void visit (btcnew::message_visitor &) const override;
	bool operator== (btcnew::frontier_req const &) const;
	btcnew::account start;
	uint32_t age;
	uint32_t count;
	static size_t constexpr size = sizeof (start) + sizeof (age) + sizeof (count);
};
class bulk_pull final : public message
{
public:
	using count_t = uint32_t;
	bulk_pull ();
	bulk_pull (bool &, btcnew::stream &, btcnew::message_header const &);
	void serialize (btcnew::stream &) const override;
	bool deserialize (btcnew::stream &);
	void visit (btcnew::message_visitor &) const override;
	btcnew::hash_or_account start{ 0 };
	btcnew::block_hash end{ 0 };
	count_t count{ 0 };
	bool is_count_present () const;
	void set_count_present (bool);
	static size_t constexpr count_present_flag = btcnew::message_header::bulk_pull_count_present_flag;
	static size_t constexpr extended_parameters_size = 8;
	static size_t constexpr size = sizeof (start) + sizeof (end);
};
class bulk_pull_account final : public message
{
public:
	bulk_pull_account ();
	bulk_pull_account (bool &, btcnew::stream &, btcnew::message_header const &);
	void serialize (btcnew::stream &) const override;
	bool deserialize (btcnew::stream &);
	void visit (btcnew::message_visitor &) const override;
	btcnew::account account;
	btcnew::amount minimum_amount;
	bulk_pull_account_flags flags;
	static size_t constexpr size = sizeof (account) + sizeof (minimum_amount) + sizeof (bulk_pull_account_flags);
};
class bulk_push final : public message
{
public:
	bulk_push ();
	explicit bulk_push (btcnew::message_header const &);
	void serialize (btcnew::stream &) const override;
	bool deserialize (btcnew::stream &);
	void visit (btcnew::message_visitor &) const override;
};
class node_id_handshake final : public message
{
public:
	node_id_handshake (bool &, btcnew::stream &, btcnew::message_header const &);
	node_id_handshake (boost::optional<btcnew::uint256_union>, boost::optional<std::pair<btcnew::account, btcnew::signature>>);
	void serialize (btcnew::stream &) const override;
	bool deserialize (btcnew::stream &);
	void visit (btcnew::message_visitor &) const override;
	bool operator== (btcnew::node_id_handshake const &) const;
	boost::optional<btcnew::uint256_union> query;
	boost::optional<std::pair<btcnew::account, btcnew::signature>> response;
	size_t size () const;
	static size_t size (btcnew::message_header const &);
};
class message_visitor
{
public:
	virtual void keepalive (btcnew::keepalive const &) = 0;
	virtual void publish (btcnew::publish const &) = 0;
	virtual void confirm_req (btcnew::confirm_req const &) = 0;
	virtual void confirm_ack (btcnew::confirm_ack const &) = 0;
	virtual void bulk_pull (btcnew::bulk_pull const &) = 0;
	virtual void bulk_pull_account (btcnew::bulk_pull_account const &) = 0;
	virtual void bulk_push (btcnew::bulk_push const &) = 0;
	virtual void frontier_req (btcnew::frontier_req const &) = 0;
	virtual void node_id_handshake (btcnew::node_id_handshake const &) = 0;
	virtual ~message_visitor ();
};

/** Helper guard which contains all the necessary purge (remove all memory even if used) functions */
class node_singleton_memory_pool_purge_guard
{
public:
	node_singleton_memory_pool_purge_guard ();

private:
	btcnew::cleanup_guard cleanup_guard;
};
}

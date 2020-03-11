#pragma once

#include <btcnew/crypto/blake2/blake2.h>
#include <btcnew/lib/errors.hpp>
#include <btcnew/lib/numbers.hpp>
#include <btcnew/lib/utility.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <cassert>
#include <streambuf>
#include <unordered_map>

namespace btcnew
{
// We operate on streams of uint8_t by convention
using stream = std::basic_streambuf<uint8_t>;
// Read a raw byte stream the size of `T' and fill value. Returns true if there was an error, false otherwise
template <typename T>
bool try_read (btcnew::stream & stream_a, T & value)
{
	static_assert (std::is_standard_layout<T>::value, "Can't stream read non-standard layout types");
	auto amount_read (stream_a.sgetn (reinterpret_cast<uint8_t *> (&value), sizeof (value)));
	return amount_read != sizeof (value);
}
// A wrapper of try_read which throws if there is an error
template <typename T>
void read (btcnew::stream & stream_a, T & value)
{
	auto error = try_read (stream_a, value);
	if (error)
	{
		throw std::runtime_error ("Failed to read type");
	}
}

template <typename T>
void write (btcnew::stream & stream_a, T const & value)
{
	static_assert (std::is_standard_layout<T>::value, "Can't stream write non-standard layout types");
	auto amount_written (stream_a.sputn (reinterpret_cast<uint8_t const *> (&value), sizeof (value)));
	(void)amount_written;
	assert (amount_written == sizeof (value));
}
class block_visitor;
enum class block_type : uint8_t
{
	invalid = 0,
	not_a_block = 1,
	send = 2,
	receive = 3,
	open = 4,
	change = 5,
	state = 6
};
class block
{
public:
	// Return a digest of the hashables in this block.
	btcnew::block_hash hash () const;
	// Return a digest of hashables and non-hashables in this block.
	btcnew::block_hash full_hash () const;
	std::string to_json () const;
	virtual void hash (blake2b_state &) const = 0;
	virtual uint64_t block_work () const = 0;
	virtual void block_work_set (uint64_t) = 0;
	virtual btcnew::account const & account () const;
	// Previous block in account's chain, zero for open block
	virtual btcnew::block_hash const & previous () const = 0;
	// Source block for open/receive blocks, zero otherwise.
	virtual btcnew::block_hash const & source () const;
	// Previous block or account number for open blocks
	virtual btcnew::root const & root () const = 0;
	// Qualified root value based on previous() and root()
	virtual btcnew::qualified_root qualified_root () const;
	// Link field for state blocks, zero otherwise.
	virtual btcnew::link const & link () const;
	virtual btcnew::account const & representative () const;
	virtual btcnew::amount const & balance () const;
	virtual void serialize (btcnew::stream &) const = 0;
	virtual void serialize_json (std::string &, bool = false) const = 0;
	virtual void serialize_json (boost::property_tree::ptree &) const = 0;
	virtual void visit (btcnew::block_visitor &) const = 0;
	virtual bool operator== (btcnew::block const &) const = 0;
	virtual btcnew::block_type type () const = 0;
	virtual btcnew::signature const & block_signature () const = 0;
	virtual void signature_set (btcnew::signature const &) = 0;
	virtual ~block () = default;
	virtual bool valid_predecessor (btcnew::block const &) const = 0;
	static size_t size (btcnew::block_type);
};
class send_hashables
{
public:
	send_hashables () = default;
	send_hashables (btcnew::block_hash const &, btcnew::account const &, btcnew::amount const &);
	send_hashables (bool &, btcnew::stream &);
	send_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	btcnew::block_hash previous;
	btcnew::account destination;
	btcnew::amount balance;
	static size_t constexpr size = sizeof (previous) + sizeof (destination) + sizeof (balance);
};
class send_block : public btcnew::block
{
public:
	send_block () = default;
	send_block (btcnew::block_hash const &, btcnew::account const &, btcnew::amount const &, btcnew::raw_key const &, btcnew::public_key const &, uint64_t);
	send_block (bool &, btcnew::stream &);
	send_block (bool &, boost::property_tree::ptree const &);
	virtual ~send_block () = default;
	using btcnew::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	btcnew::block_hash const & previous () const override;
	btcnew::root const & root () const override;
	btcnew::amount const & balance () const override;
	void serialize (btcnew::stream &) const override;
	bool deserialize (btcnew::stream &);
	void serialize_json (std::string &, bool = false) const override;
	void serialize_json (boost::property_tree::ptree &) const override;
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (btcnew::block_visitor &) const override;
	btcnew::block_type type () const override;
	btcnew::signature const & block_signature () const override;
	void signature_set (btcnew::signature const &) override;
	bool operator== (btcnew::block const &) const override;
	bool operator== (btcnew::send_block const &) const;
	bool valid_predecessor (btcnew::block const &) const override;
	send_hashables hashables;
	btcnew::signature signature;
	uint64_t work;
	static size_t constexpr size = btcnew::send_hashables::size + sizeof (signature) + sizeof (work);
};
class receive_hashables
{
public:
	receive_hashables () = default;
	receive_hashables (btcnew::block_hash const &, btcnew::block_hash const &);
	receive_hashables (bool &, btcnew::stream &);
	receive_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	btcnew::block_hash previous;
	btcnew::block_hash source;
	static size_t constexpr size = sizeof (previous) + sizeof (source);
};
class receive_block : public btcnew::block
{
public:
	receive_block () = default;
	receive_block (btcnew::block_hash const &, btcnew::block_hash const &, btcnew::raw_key const &, btcnew::public_key const &, uint64_t);
	receive_block (bool &, btcnew::stream &);
	receive_block (bool &, boost::property_tree::ptree const &);
	virtual ~receive_block () = default;
	using btcnew::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	btcnew::block_hash const & previous () const override;
	btcnew::block_hash const & source () const override;
	btcnew::root const & root () const override;
	void serialize (btcnew::stream &) const override;
	bool deserialize (btcnew::stream &);
	void serialize_json (std::string &, bool = false) const override;
	void serialize_json (boost::property_tree::ptree &) const override;
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (btcnew::block_visitor &) const override;
	btcnew::block_type type () const override;
	btcnew::signature const & block_signature () const override;
	void signature_set (btcnew::signature const &) override;
	bool operator== (btcnew::block const &) const override;
	bool operator== (btcnew::receive_block const &) const;
	bool valid_predecessor (btcnew::block const &) const override;
	receive_hashables hashables;
	btcnew::signature signature;
	uint64_t work;
	static size_t constexpr size = btcnew::receive_hashables::size + sizeof (signature) + sizeof (work);
};
class open_hashables
{
public:
	open_hashables () = default;
	open_hashables (btcnew::block_hash const &, btcnew::account const &, btcnew::account const &);
	open_hashables (bool &, btcnew::stream &);
	open_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	btcnew::block_hash source;
	btcnew::account representative;
	btcnew::account account;
	static size_t constexpr size = sizeof (source) + sizeof (representative) + sizeof (account);
};
class open_block : public btcnew::block
{
public:
	open_block () = default;
	open_block (btcnew::block_hash const &, btcnew::account const &, btcnew::account const &, btcnew::raw_key const &, btcnew::public_key const &, uint64_t);
	open_block (btcnew::block_hash const &, btcnew::account const &, btcnew::account const &, std::nullptr_t);
	open_block (bool &, btcnew::stream &);
	open_block (bool &, boost::property_tree::ptree const &);
	virtual ~open_block () = default;
	using btcnew::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	btcnew::block_hash const & previous () const override;
	btcnew::account const & account () const override;
	btcnew::block_hash const & source () const override;
	btcnew::root const & root () const override;
	btcnew::account const & representative () const override;
	void serialize (btcnew::stream &) const override;
	bool deserialize (btcnew::stream &);
	void serialize_json (std::string &, bool = false) const override;
	void serialize_json (boost::property_tree::ptree &) const override;
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (btcnew::block_visitor &) const override;
	btcnew::block_type type () const override;
	btcnew::signature const & block_signature () const override;
	void signature_set (btcnew::signature const &) override;
	bool operator== (btcnew::block const &) const override;
	bool operator== (btcnew::open_block const &) const;
	bool valid_predecessor (btcnew::block const &) const override;
	btcnew::open_hashables hashables;
	btcnew::signature signature;
	uint64_t work;
	static size_t constexpr size = btcnew::open_hashables::size + sizeof (signature) + sizeof (work);
};
class change_hashables
{
public:
	change_hashables () = default;
	change_hashables (btcnew::block_hash const &, btcnew::account const &);
	change_hashables (bool &, btcnew::stream &);
	change_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	btcnew::block_hash previous;
	btcnew::account representative;
	static size_t constexpr size = sizeof (previous) + sizeof (representative);
};
class change_block : public btcnew::block
{
public:
	change_block () = default;
	change_block (btcnew::block_hash const &, btcnew::account const &, btcnew::raw_key const &, btcnew::public_key const &, uint64_t);
	change_block (bool &, btcnew::stream &);
	change_block (bool &, boost::property_tree::ptree const &);
	virtual ~change_block () = default;
	using btcnew::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	btcnew::block_hash const & previous () const override;
	btcnew::root const & root () const override;
	btcnew::account const & representative () const override;
	void serialize (btcnew::stream &) const override;
	bool deserialize (btcnew::stream &);
	void serialize_json (std::string &, bool = false) const override;
	void serialize_json (boost::property_tree::ptree &) const override;
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (btcnew::block_visitor &) const override;
	btcnew::block_type type () const override;
	btcnew::signature const & block_signature () const override;
	void signature_set (btcnew::signature const &) override;
	bool operator== (btcnew::block const &) const override;
	bool operator== (btcnew::change_block const &) const;
	bool valid_predecessor (btcnew::block const &) const override;
	btcnew::change_hashables hashables;
	btcnew::signature signature;
	uint64_t work;
	static size_t constexpr size = btcnew::change_hashables::size + sizeof (signature) + sizeof (work);
};
class state_hashables
{
public:
	state_hashables () = default;
	state_hashables (btcnew::account const &, btcnew::block_hash const &, btcnew::account const &, btcnew::amount const &, btcnew::link const &);
	state_hashables (bool &, btcnew::stream &);
	state_hashables (bool &, boost::property_tree::ptree const &);
	void hash (blake2b_state &) const;
	// Account# / public key that operates this account
	// Uses:
	// Bulk signature validation in advance of further ledger processing
	// Arranging uncomitted transactions by account
	btcnew::account account;
	// Previous transaction in this chain
	btcnew::block_hash previous;
	// Representative of this account
	btcnew::account representative;
	// Current balance of this account
	// Allows lookup of account balance simply by looking at the head block
	btcnew::amount balance;
	// Link field contains source block_hash if receiving, destination account if sending
	btcnew::link link;
	// Serialized size
	static size_t constexpr size = sizeof (account) + sizeof (previous) + sizeof (representative) + sizeof (balance) + sizeof (link);
};
class state_block : public btcnew::block
{
public:
	state_block () = default;
	state_block (btcnew::account const &, btcnew::block_hash const &, btcnew::account const &, btcnew::amount const &, btcnew::link const &, btcnew::raw_key const &, btcnew::public_key const &, uint64_t);
	state_block (bool &, btcnew::stream &);
	state_block (bool &, boost::property_tree::ptree const &);
	virtual ~state_block () = default;
	using btcnew::block::hash;
	void hash (blake2b_state &) const override;
	uint64_t block_work () const override;
	void block_work_set (uint64_t) override;
	btcnew::block_hash const & previous () const override;
	btcnew::account const & account () const override;
	btcnew::root const & root () const override;
	btcnew::link const & link () const override;
	btcnew::account const & representative () const override;
	btcnew::amount const & balance () const override;
	void serialize (btcnew::stream &) const override;
	bool deserialize (btcnew::stream &);
	void serialize_json (std::string &, bool = false) const override;
	void serialize_json (boost::property_tree::ptree &) const override;
	bool deserialize_json (boost::property_tree::ptree const &);
	void visit (btcnew::block_visitor &) const override;
	btcnew::block_type type () const override;
	btcnew::signature const & block_signature () const override;
	void signature_set (btcnew::signature const &) override;
	bool operator== (btcnew::block const &) const override;
	bool operator== (btcnew::state_block const &) const;
	bool valid_predecessor (btcnew::block const &) const override;
	btcnew::state_hashables hashables;
	btcnew::signature signature;
	uint64_t work;
	static size_t constexpr size = btcnew::state_hashables::size + sizeof (signature) + sizeof (work);
};
class block_visitor
{
public:
	virtual void send_block (btcnew::send_block const &) = 0;
	virtual void receive_block (btcnew::receive_block const &) = 0;
	virtual void open_block (btcnew::open_block const &) = 0;
	virtual void change_block (btcnew::change_block const &) = 0;
	virtual void state_block (btcnew::state_block const &) = 0;
	virtual ~block_visitor () = default;
};
/**
 * This class serves to find and return unique variants of a block in order to minimize memory usage
 */
class block_uniquer
{
public:
	using value_type = std::pair<const btcnew::uint256_union, std::weak_ptr<btcnew::block>>;

	std::shared_ptr<btcnew::block> unique (std::shared_ptr<btcnew::block>);
	size_t size ();

private:
	std::mutex mutex;
	std::unordered_map<std::remove_const_t<value_type::first_type>, value_type::second_type> blocks;
	static unsigned constexpr cleanup_count = 2;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (block_uniquer & block_uniquer, const std::string & name);

std::shared_ptr<btcnew::block> deserialize_block (btcnew::stream &);
std::shared_ptr<btcnew::block> deserialize_block (btcnew::stream &, btcnew::block_type, btcnew::block_uniquer * = nullptr);
std::shared_ptr<btcnew::block> deserialize_block_json (boost::property_tree::ptree const &, btcnew::block_uniquer * = nullptr);
void serialize_block (btcnew::stream &, btcnew::block const &);
void block_memory_pool_purge ();
}

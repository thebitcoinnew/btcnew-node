#pragma once

#include <btcnew/crypto_lib/random_pool.hpp>
#include <btcnew/lib/config.hpp>
#include <btcnew/lib/diagnosticsconfig.hpp>
#include <btcnew/lib/logger_mt.hpp>
#include <btcnew/lib/memory.hpp>
#include <btcnew/lib/rocksdbconfig.hpp>
#include <btcnew/secure/common.hpp>
#include <btcnew/secure/versioning.hpp>

#include <boost/endian/conversion.hpp>
#include <boost/polymorphic_cast.hpp>

#include <stack>

namespace btcnew
{
/**
 * Encapsulates database specific container
 */
template <typename Val>
class db_val
{
public:
	db_val (Val const & value_a) :
	value (value_a)
	{
	}

	db_val () :
	db_val (0, nullptr)
	{
	}

	db_val (btcnew::uint128_union const & val_a) :
	db_val (sizeof (val_a), const_cast<btcnew::uint128_union *> (&val_a))
	{
	}

	db_val (btcnew::uint256_union const & val_a) :
	db_val (sizeof (val_a), const_cast<btcnew::uint256_union *> (&val_a))
	{
	}

	db_val (btcnew::account_info const & val_a) :
	db_val (val_a.db_size (), const_cast<btcnew::account_info *> (&val_a))
	{
	}

	db_val (btcnew::account_info_v13 const & val_a) :
	db_val (val_a.db_size (), const_cast<btcnew::account_info_v13 *> (&val_a))
	{
	}

	db_val (btcnew::account_info_v14 const & val_a) :
	db_val (val_a.db_size (), const_cast<btcnew::account_info_v14 *> (&val_a))
	{
	}

	db_val (btcnew::pending_info const & val_a) :
	db_val (val_a.db_size (), const_cast<btcnew::pending_info *> (&val_a))
	{
		static_assert (std::is_standard_layout<btcnew::pending_info>::value, "Standard layout is required");
	}

	db_val (btcnew::pending_info_v14 const & val_a) :
	db_val (val_a.db_size (), const_cast<btcnew::pending_info_v14 *> (&val_a))
	{
		static_assert (std::is_standard_layout<btcnew::pending_info_v14>::value, "Standard layout is required");
	}

	db_val (btcnew::pending_key const & val_a) :
	db_val (sizeof (val_a), const_cast<btcnew::pending_key *> (&val_a))
	{
		static_assert (std::is_standard_layout<btcnew::pending_key>::value, "Standard layout is required");
	}

	db_val (btcnew::unchecked_info const & val_a) :
	buffer (std::make_shared<std::vector<uint8_t>> ())
	{
		{
			btcnew::vectorstream stream (*buffer);
			val_a.serialize (stream);
		}
		convert_buffer_to_value ();
	}

	db_val (btcnew::unchecked_key const & val_a) :
	db_val (sizeof (val_a), const_cast<btcnew::unchecked_key *> (&val_a))
	{
		static_assert (std::is_standard_layout<btcnew::unchecked_key>::value, "Standard layout is required");
	}

	db_val (btcnew::block_info const & val_a) :
	db_val (sizeof (val_a), const_cast<btcnew::block_info *> (&val_a))
	{
		static_assert (std::is_standard_layout<btcnew::block_info>::value, "Standard layout is required");
	}

	db_val (btcnew::endpoint_key const & val_a) :
	db_val (sizeof (val_a), const_cast<btcnew::endpoint_key *> (&val_a))
	{
		static_assert (std::is_standard_layout<btcnew::endpoint_key>::value, "Standard layout is required");
	}

	db_val (std::shared_ptr<btcnew::block> const & val_a) :
	buffer (std::make_shared<std::vector<uint8_t>> ())
	{
		{
			btcnew::vectorstream stream (*buffer);
			btcnew::serialize_block (stream, *val_a);
		}
		convert_buffer_to_value ();
	}

	db_val (uint64_t val_a) :
	buffer (std::make_shared<std::vector<uint8_t>> ())
	{
		{
			boost::endian::native_to_big_inplace (val_a);
			btcnew::vectorstream stream (*buffer);
			btcnew::write (stream, val_a);
		}
		convert_buffer_to_value ();
	}

	explicit operator btcnew::account_info () const
	{
		btcnew::account_info result;
		assert (size () == result.db_size ());
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + result.db_size (), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator btcnew::account_info_v13 () const
	{
		btcnew::account_info_v13 result;
		assert (size () == result.db_size ());
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + result.db_size (), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator btcnew::account_info_v14 () const
	{
		btcnew::account_info_v14 result;
		assert (size () == result.db_size ());
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + result.db_size (), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator btcnew::block_info () const
	{
		btcnew::block_info result;
		assert (size () == sizeof (result));
		static_assert (sizeof (btcnew::block_info::account) + sizeof (btcnew::block_info::balance) == sizeof (result), "Packed class");
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator btcnew::pending_info_v14 () const
	{
		btcnew::pending_info_v14 result;
		assert (size () == result.db_size ());
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + result.db_size (), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator btcnew::pending_info () const
	{
		btcnew::pending_info result;
		assert (size () == result.db_size ());
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + result.db_size (), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator btcnew::pending_key () const
	{
		btcnew::pending_key result;
		assert (size () == sizeof (result));
		static_assert (sizeof (btcnew::pending_key::account) + sizeof (btcnew::pending_key::hash) == sizeof (result), "Packed class");
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator btcnew::unchecked_info () const
	{
		btcnew::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		btcnew::unchecked_info result;
		bool error (result.deserialize (stream));
		(void)error;
		assert (!error);
		return result;
	}

	explicit operator btcnew::unchecked_key () const
	{
		btcnew::unchecked_key result;
		assert (size () == sizeof (result));
		static_assert (sizeof (btcnew::unchecked_key::previous) + sizeof (btcnew::pending_key::hash) == sizeof (result), "Packed class");
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator btcnew::uint128_union () const
	{
		return convert<btcnew::uint128_union> ();
	}

	explicit operator btcnew::amount () const
	{
		return convert<btcnew::amount> ();
	}

	explicit operator btcnew::block_hash () const
	{
		return convert<btcnew::block_hash> ();
	}

	explicit operator btcnew::public_key () const
	{
		return convert<btcnew::public_key> ();
	}

	explicit operator btcnew::uint256_union () const
	{
		return convert<btcnew::uint256_union> ();
	}

	explicit operator std::array<char, 64> () const
	{
		btcnew::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		std::array<char, 64> result;
		auto error = btcnew::try_read (stream, result);
		(void)error;
		assert (!error);
		return result;
	}

	explicit operator btcnew::endpoint_key () const
	{
		btcnew::endpoint_key result;
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), reinterpret_cast<uint8_t *> (&result));
		return result;
	}

	explicit operator btcnew::state_block_w_sideband_v14 () const
	{
		btcnew::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		auto error (false);
		btcnew::state_block_w_sideband_v14 state_block_w_sideband_v14;
		state_block_w_sideband_v14.state_block = std::make_shared<btcnew::state_block> (error, stream);
		assert (!error);

		state_block_w_sideband_v14.sideband.type = btcnew::block_type::state;
		error = state_block_w_sideband_v14.sideband.deserialize (stream);
		assert (!error);

		return state_block_w_sideband_v14;
	}

	explicit operator btcnew::no_value () const
	{
		return no_value::dummy;
	}

	explicit operator std::shared_ptr<btcnew::block> () const
	{
		btcnew::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		std::shared_ptr<btcnew::block> result (btcnew::deserialize_block (stream));
		return result;
	}

	template <typename Block>
	std::shared_ptr<Block> convert_to_block () const
	{
		btcnew::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		auto error (false);
		auto result (std::make_shared<Block> (error, stream));
		assert (!error);
		return result;
	}

	explicit operator std::shared_ptr<btcnew::send_block> () const
	{
		return convert_to_block<btcnew::send_block> ();
	}

	explicit operator std::shared_ptr<btcnew::receive_block> () const
	{
		return convert_to_block<btcnew::receive_block> ();
	}

	explicit operator std::shared_ptr<btcnew::open_block> () const
	{
		return convert_to_block<btcnew::open_block> ();
	}

	explicit operator std::shared_ptr<btcnew::change_block> () const
	{
		return convert_to_block<btcnew::change_block> ();
	}

	explicit operator std::shared_ptr<btcnew::state_block> () const
	{
		return convert_to_block<btcnew::state_block> ();
	}

	explicit operator std::shared_ptr<btcnew::vote> () const
	{
		btcnew::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		auto error (false);
		auto result (btcnew::make_shared<btcnew::vote> (error, stream));
		assert (!error);
		return result;
	}

	explicit operator uint64_t () const
	{
		uint64_t result;
		btcnew::bufferstream stream (reinterpret_cast<uint8_t const *> (data ()), size ());
		auto error (btcnew::try_read (stream, result));
		(void)error;
		assert (!error);
		boost::endian::big_to_native_inplace (result);
		return result;
	}

	operator Val * () const
	{
		// Allow passing a temporary to a non-c++ function which doesn't have constness
		return const_cast<Val *> (&value);
	}

	operator Val const & () const
	{
		return value;
	}

	// Must be specialized
	void * data () const;
	size_t size () const;
	db_val (size_t size_a, void * data_a);
	void convert_buffer_to_value ();

	Val value;
	std::shared_ptr<std::vector<uint8_t>> buffer;

private:
	template <typename T>
	T convert () const
	{
		T result;
		assert (size () == sizeof (result));
		std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), result.bytes.data ());
		return result;
	}
};

class block_sideband final
{
public:
	block_sideband () = default;
	block_sideband (btcnew::block_type, btcnew::account const &, btcnew::block_hash const &, btcnew::amount const &, uint64_t, uint64_t, btcnew::epoch);
	void serialize (btcnew::stream &) const;
	bool deserialize (btcnew::stream &);
	static size_t size (btcnew::block_type);
	btcnew::block_type type{ btcnew::block_type::invalid };
	btcnew::block_hash successor{ 0 };
	btcnew::account account{ 0 };
	btcnew::amount balance{ 0 };
	uint64_t height{ 0 };
	uint64_t timestamp{ 0 };
	btcnew::epoch epoch{ btcnew::epoch::epoch_0 };
};
class transaction;
class block_store;

/**
 * Summation visitor for blocks, supporting amount and balance computations. These
 * computations are mutually dependant. The natural solution is to use mutual recursion
 * between balance and amount visitors, but this leads to very deep stacks. Hence, the
 * summation visitor uses an iterative approach.
 */
class summation_visitor final : public btcnew::block_visitor
{
	enum summation_type
	{
		invalid = 0,
		balance = 1,
		amount = 2
	};

	/** Represents an invocation frame */
	class frame final
	{
	public:
		frame (summation_type type_a, btcnew::block_hash balance_hash_a, btcnew::block_hash amount_hash_a) :
		type (type_a), balance_hash (balance_hash_a), amount_hash (amount_hash_a)
		{
		}

		/** The summation type guides the block visitor handlers */
		summation_type type{ invalid };
		/** Accumulated balance or amount */
		btcnew::uint128_t sum{ 0 };
		/** The current balance hash */
		btcnew::block_hash balance_hash{ 0 };
		/** The current amount hash */
		btcnew::block_hash amount_hash{ 0 };
		/** If true, this frame is awaiting an invocation result */
		bool awaiting_result{ false };
		/** Set by the invoked frame, representing the return value */
		btcnew::uint128_t incoming_result{ 0 };
	};

public:
	summation_visitor (btcnew::transaction const &, btcnew::block_store const &, bool is_v14_upgrade = false);
	virtual ~summation_visitor () = default;
	/** Computes the balance as of \p block_hash */
	btcnew::uint128_t compute_balance (btcnew::block_hash const & block_hash);
	/** Computes the amount delta between \p block_hash and its predecessor */
	btcnew::uint128_t compute_amount (btcnew::block_hash const & block_hash);

protected:
	btcnew::transaction const & transaction;
	btcnew::block_store const & store;
	btcnew::network_params network_params;

	/** The final result */
	btcnew::uint128_t result{ 0 };
	/** The current invocation frame */
	frame * current{ nullptr };
	/** Invocation frames */
	std::stack<frame> frames;
	/** Push a copy of \p hash of the given summation \p type */
	btcnew::summation_visitor::frame push (btcnew::summation_visitor::summation_type type, btcnew::block_hash const & hash);
	void sum_add (btcnew::uint128_t addend_a);
	void sum_set (btcnew::uint128_t value_a);
	/** The epilogue yields the result to previous frame, if any */
	void epilogue ();

	btcnew::uint128_t compute_internal (btcnew::summation_visitor::summation_type type, btcnew::block_hash const &);
	void send_block (btcnew::send_block const &) override;
	void receive_block (btcnew::receive_block const &) override;
	void open_block (btcnew::open_block const &) override;
	void change_block (btcnew::change_block const &) override;
	void state_block (btcnew::state_block const &) override;

private:
	bool is_v14_upgrade;
	std::shared_ptr<btcnew::block> block_get (btcnew::transaction const &, btcnew::block_hash const &) const;
};

/**
 * Determine the representative for this block
 */
class representative_visitor final : public btcnew::block_visitor
{
public:
	representative_visitor (btcnew::transaction const & transaction_a, btcnew::block_store & store_a);
	~representative_visitor () = default;
	void compute (btcnew::block_hash const & hash_a);
	void send_block (btcnew::send_block const & block_a) override;
	void receive_block (btcnew::receive_block const & block_a) override;
	void open_block (btcnew::open_block const & block_a) override;
	void change_block (btcnew::change_block const & block_a) override;
	void state_block (btcnew::state_block const & block_a) override;
	btcnew::transaction const & transaction;
	btcnew::block_store & store;
	btcnew::block_hash current;
	btcnew::block_hash result;
};
template <typename T, typename U>
class store_iterator_impl
{
public:
	virtual ~store_iterator_impl () = default;
	virtual btcnew::store_iterator_impl<T, U> & operator++ () = 0;
	virtual bool operator== (btcnew::store_iterator_impl<T, U> const & other_a) const = 0;
	virtual bool is_end_sentinal () const = 0;
	virtual void fill (std::pair<T, U> &) const = 0;
	btcnew::store_iterator_impl<T, U> & operator= (btcnew::store_iterator_impl<T, U> const &) = delete;
	bool operator== (btcnew::store_iterator_impl<T, U> const * other_a) const
	{
		return (other_a != nullptr && *this == *other_a) || (other_a == nullptr && is_end_sentinal ());
	}
	bool operator!= (btcnew::store_iterator_impl<T, U> const & other_a) const
	{
		return !(*this == other_a);
	}
};
/**
 * Iterates the key/value pairs of a transaction
 */
template <typename T, typename U>
class store_iterator final
{
public:
	store_iterator (std::nullptr_t)
	{
	}
	store_iterator (std::unique_ptr<btcnew::store_iterator_impl<T, U>> impl_a) :
	impl (std::move (impl_a))
	{
		impl->fill (current);
	}
	store_iterator (btcnew::store_iterator<T, U> && other_a) :
	current (std::move (other_a.current)),
	impl (std::move (other_a.impl))
	{
	}
	btcnew::store_iterator<T, U> & operator++ ()
	{
		++*impl;
		impl->fill (current);
		return *this;
	}
	btcnew::store_iterator<T, U> & operator= (btcnew::store_iterator<T, U> && other_a)
	{
		impl = std::move (other_a.impl);
		current = std::move (other_a.current);
		return *this;
	}
	btcnew::store_iterator<T, U> & operator= (btcnew::store_iterator<T, U> const &) = delete;
	std::pair<T, U> * operator-> ()
	{
		return &current;
	}
	bool operator== (btcnew::store_iterator<T, U> const & other_a) const
	{
		return (impl == nullptr && other_a.impl == nullptr) || (impl != nullptr && *impl == other_a.impl.get ()) || (other_a.impl != nullptr && *other_a.impl == impl.get ());
	}
	bool operator!= (btcnew::store_iterator<T, U> const & other_a) const
	{
		return !(*this == other_a);
	}

private:
	std::pair<T, U> current;
	std::unique_ptr<btcnew::store_iterator_impl<T, U>> impl;
};

// Keep this in alphabetical order
enum class tables
{
	accounts,
	blocks_info, // LMDB only
	cached_counts, // RocksDB only
	change_blocks,
	confirmation_height,
	frontiers,
	meta,
	online_weight,
	open_blocks,
	peers,
	pending,
	receive_blocks,
	representation,
	send_blocks,
	state_blocks,
	unchecked,
	vote
};

class transaction_impl
{
public:
	virtual ~transaction_impl () = default;
	virtual void * get_handle () const = 0;
};

class read_transaction_impl : public transaction_impl
{
public:
	virtual void reset () = 0;
	virtual void renew () = 0;
};

class write_transaction_impl : public transaction_impl
{
public:
	virtual void commit () const = 0;
	virtual void renew () = 0;
	virtual bool contains (btcnew::tables table_a) const = 0;
};

class transaction
{
public:
	virtual ~transaction () = default;
	virtual void * get_handle () const = 0;
};

/**
 * RAII wrapper of a read MDB_txn where the constructor starts the transaction
 * and the destructor aborts it.
 */
class read_transaction final : public transaction
{
public:
	explicit read_transaction (std::unique_ptr<btcnew::read_transaction_impl> read_transaction_impl);
	void * get_handle () const override;
	void reset () const;
	void renew () const;
	void refresh () const;

private:
	std::unique_ptr<btcnew::read_transaction_impl> impl;
};

/**
 * RAII wrapper of a read-write MDB_txn where the constructor starts the transaction
 * and the destructor commits it.
 */
class write_transaction final : public transaction
{
public:
	explicit write_transaction (std::unique_ptr<btcnew::write_transaction_impl> write_transaction_impl);
	void * get_handle () const override;
	void commit () const;
	void renew ();
	bool contains (btcnew::tables table_a) const;

private:
	std::unique_ptr<btcnew::write_transaction_impl> impl;
};

class rep_weights;

/**
 * Manages block storage and iteration
 */
class block_store
{
public:
	virtual ~block_store () = default;
	virtual void initialize (btcnew::write_transaction const &, btcnew::genesis const &, btcnew::rep_weights &, std::atomic<uint64_t> &, std::atomic<uint64_t> &) = 0;
	virtual void block_put (btcnew::write_transaction const &, btcnew::block_hash const &, btcnew::block const &, btcnew::block_sideband const &) = 0;
	virtual btcnew::block_hash block_successor (btcnew::transaction const &, btcnew::block_hash const &) const = 0;
	virtual void block_successor_clear (btcnew::write_transaction const &, btcnew::block_hash const &) = 0;
	virtual std::shared_ptr<btcnew::block> block_get (btcnew::transaction const &, btcnew::block_hash const &, btcnew::block_sideband * = nullptr) const = 0;
	virtual std::shared_ptr<btcnew::block> block_get_v14 (btcnew::transaction const &, btcnew::block_hash const &, btcnew::block_sideband_v14 * = nullptr, bool * = nullptr) const = 0;
	virtual std::shared_ptr<btcnew::block> block_random (btcnew::transaction const &) = 0;
	virtual void block_del (btcnew::write_transaction const &, btcnew::block_hash const &) = 0;
	virtual bool block_exists (btcnew::transaction const &, btcnew::block_hash const &) = 0;
	virtual bool block_exists (btcnew::transaction const &, btcnew::block_type, btcnew::block_hash const &) = 0;
	virtual btcnew::block_counts block_count (btcnew::transaction const &) = 0;
	virtual bool root_exists (btcnew::transaction const &, btcnew::root const &) = 0;
	virtual bool source_exists (btcnew::transaction const &, btcnew::block_hash const &) = 0;
	virtual btcnew::account block_account (btcnew::transaction const &, btcnew::block_hash const &) const = 0;

	virtual void frontier_put (btcnew::write_transaction const &, btcnew::block_hash const &, btcnew::account const &) = 0;
	virtual btcnew::account frontier_get (btcnew::transaction const &, btcnew::block_hash const &) const = 0;
	virtual void frontier_del (btcnew::write_transaction const &, btcnew::block_hash const &) = 0;

	virtual void account_put (btcnew::write_transaction const &, btcnew::account const &, btcnew::account_info const &) = 0;
	virtual bool account_get (btcnew::transaction const &, btcnew::account const &, btcnew::account_info &) = 0;
	virtual void account_del (btcnew::write_transaction const &, btcnew::account const &) = 0;
	virtual bool account_exists (btcnew::transaction const &, btcnew::account const &) = 0;
	virtual size_t account_count (btcnew::transaction const &) = 0;
	virtual void confirmation_height_clear (btcnew::write_transaction const &, btcnew::account const & account, uint64_t existing_confirmation_height) = 0;
	virtual void confirmation_height_clear (btcnew::write_transaction const &) = 0;
	virtual btcnew::store_iterator<btcnew::account, btcnew::account_info> latest_begin (btcnew::transaction const &, btcnew::account const &) = 0;
	virtual btcnew::store_iterator<btcnew::account, btcnew::account_info> latest_begin (btcnew::transaction const &) = 0;
	virtual btcnew::store_iterator<btcnew::account, btcnew::account_info> latest_end () = 0;

	virtual void pending_put (btcnew::write_transaction const &, btcnew::pending_key const &, btcnew::pending_info const &) = 0;
	virtual void pending_del (btcnew::write_transaction const &, btcnew::pending_key const &) = 0;
	virtual bool pending_get (btcnew::transaction const &, btcnew::pending_key const &, btcnew::pending_info &) = 0;
	virtual bool pending_exists (btcnew::transaction const &, btcnew::pending_key const &) = 0;
	virtual btcnew::store_iterator<btcnew::pending_key, btcnew::pending_info> pending_begin (btcnew::transaction const &, btcnew::pending_key const &) = 0;
	virtual btcnew::store_iterator<btcnew::pending_key, btcnew::pending_info> pending_begin (btcnew::transaction const &) = 0;
	virtual btcnew::store_iterator<btcnew::pending_key, btcnew::pending_info> pending_end () = 0;

	virtual bool block_info_get (btcnew::transaction const &, btcnew::block_hash const &, btcnew::block_info &) const = 0;
	virtual btcnew::uint128_t block_balance (btcnew::transaction const &, btcnew::block_hash const &) = 0;
	virtual btcnew::uint128_t block_balance_calculated (std::shared_ptr<btcnew::block>, btcnew::block_sideband const &) const = 0;
	virtual btcnew::epoch block_version (btcnew::transaction const &, btcnew::block_hash const &) = 0;

	virtual void unchecked_clear (btcnew::write_transaction const &) = 0;
	virtual void unchecked_put (btcnew::write_transaction const &, btcnew::unchecked_key const &, btcnew::unchecked_info const &) = 0;
	virtual void unchecked_put (btcnew::write_transaction const &, btcnew::block_hash const &, std::shared_ptr<btcnew::block> const &) = 0;
	virtual std::vector<btcnew::unchecked_info> unchecked_get (btcnew::transaction const &, btcnew::block_hash const &) = 0;
	virtual void unchecked_del (btcnew::write_transaction const &, btcnew::unchecked_key const &) = 0;
	virtual btcnew::store_iterator<btcnew::unchecked_key, btcnew::unchecked_info> unchecked_begin (btcnew::transaction const &) = 0;
	virtual btcnew::store_iterator<btcnew::unchecked_key, btcnew::unchecked_info> unchecked_begin (btcnew::transaction const &, btcnew::unchecked_key const &) = 0;
	virtual btcnew::store_iterator<btcnew::unchecked_key, btcnew::unchecked_info> unchecked_end () = 0;
	virtual size_t unchecked_count (btcnew::transaction const &) = 0;

	// Return latest vote for an account from store
	virtual std::shared_ptr<btcnew::vote> vote_get (btcnew::transaction const &, btcnew::account const &) = 0;
	// Populate vote with the next sequence number
	virtual std::shared_ptr<btcnew::vote> vote_generate (btcnew::transaction const &, btcnew::account const &, btcnew::raw_key const &, std::shared_ptr<btcnew::block>) = 0;
	virtual std::shared_ptr<btcnew::vote> vote_generate (btcnew::transaction const &, btcnew::account const &, btcnew::raw_key const &, std::vector<btcnew::block_hash>) = 0;
	// Return either vote or the stored vote with a higher sequence number
	virtual std::shared_ptr<btcnew::vote> vote_max (btcnew::transaction const &, std::shared_ptr<btcnew::vote>) = 0;
	// Return latest vote for an account considering the vote cache
	virtual std::shared_ptr<btcnew::vote> vote_current (btcnew::transaction const &, btcnew::account const &) = 0;
	virtual void flush (btcnew::write_transaction const &) = 0;
	virtual btcnew::store_iterator<btcnew::account, std::shared_ptr<btcnew::vote>> vote_begin (btcnew::transaction const &) = 0;
	virtual btcnew::store_iterator<btcnew::account, std::shared_ptr<btcnew::vote>> vote_end () = 0;

	virtual void online_weight_put (btcnew::write_transaction const &, uint64_t, btcnew::amount const &) = 0;
	virtual void online_weight_del (btcnew::write_transaction const &, uint64_t) = 0;
	virtual btcnew::store_iterator<uint64_t, btcnew::amount> online_weight_begin (btcnew::transaction const &) const = 0;
	virtual btcnew::store_iterator<uint64_t, btcnew::amount> online_weight_end () const = 0;
	virtual size_t online_weight_count (btcnew::transaction const &) const = 0;
	virtual void online_weight_clear (btcnew::write_transaction const &) = 0;

	virtual void version_put (btcnew::write_transaction const &, int) = 0;
	virtual int version_get (btcnew::transaction const &) const = 0;

	virtual void peer_put (btcnew::write_transaction const & transaction_a, btcnew::endpoint_key const & endpoint_a) = 0;
	virtual void peer_del (btcnew::write_transaction const & transaction_a, btcnew::endpoint_key const & endpoint_a) = 0;
	virtual bool peer_exists (btcnew::transaction const & transaction_a, btcnew::endpoint_key const & endpoint_a) const = 0;
	virtual size_t peer_count (btcnew::transaction const & transaction_a) const = 0;
	virtual void peer_clear (btcnew::write_transaction const & transaction_a) = 0;
	virtual btcnew::store_iterator<btcnew::endpoint_key, btcnew::no_value> peers_begin (btcnew::transaction const & transaction_a) const = 0;
	virtual btcnew::store_iterator<btcnew::endpoint_key, btcnew::no_value> peers_end () const = 0;

	virtual void confirmation_height_put (btcnew::write_transaction const & transaction_a, btcnew::account const & account_a, uint64_t confirmation_height_a) = 0;
	virtual bool confirmation_height_get (btcnew::transaction const & transaction_a, btcnew::account const & account_a, uint64_t & confirmation_height_a) = 0;
	virtual bool confirmation_height_exists (btcnew::transaction const & transaction_a, btcnew::account const & account_a) const = 0;
	virtual void confirmation_height_del (btcnew::write_transaction const & transaction_a, btcnew::account const & account_a) = 0;
	virtual uint64_t confirmation_height_count (btcnew::transaction const & transaction_a) = 0;
	virtual btcnew::store_iterator<btcnew::account, uint64_t> confirmation_height_begin (btcnew::transaction const & transaction_a, btcnew::account const & account_a) = 0;
	virtual btcnew::store_iterator<btcnew::account, uint64_t> confirmation_height_begin (btcnew::transaction const & transaction_a) = 0;
	virtual btcnew::store_iterator<btcnew::account, uint64_t> confirmation_height_end () = 0;

	virtual uint64_t block_account_height (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a) const = 0;
	virtual std::mutex & get_cache_mutex () = 0;

	virtual bool copy_db (boost::filesystem::path const & destination) = 0;

	/** Not applicable to all sub-classes */
	virtual void serialize_mdb_tracker (boost::property_tree::ptree &, std::chrono::milliseconds, std::chrono::milliseconds) = 0;

	virtual bool init_error () const = 0;

	/** Start read-write transaction */
	virtual btcnew::write_transaction tx_begin_write (std::vector<btcnew::tables> const & tables_to_lock = {}, std::vector<btcnew::tables> const & tables_no_lock = {}) = 0;

	/** Start read-only transaction */
	virtual btcnew::read_transaction tx_begin_read () = 0;
};

std::unique_ptr<btcnew::block_store> make_store (btcnew::logger_mt & logger, boost::filesystem::path const & path, bool open_read_only = false, bool add_db_postfix = false, btcnew::rocksdb_config const & rocksdb_config = btcnew::rocksdb_config{}, btcnew::txn_tracking_config const & txn_tracking_config_a = btcnew::txn_tracking_config{}, std::chrono::milliseconds block_processor_batch_max_time_a = std::chrono::milliseconds (5000), int lmdb_max_dbs = 128, size_t batch_size = 512, bool backup_before_upgrade = false, bool rocksdb_backend = false);
}

namespace std
{
template <>
struct hash<::btcnew::tables>
{
	size_t operator() (::btcnew::tables const & table_a) const
	{
		return static_cast<size_t> (table_a);
	}
};
}

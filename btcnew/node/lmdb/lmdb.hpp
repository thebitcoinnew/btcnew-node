#pragma once

#include <btcnew/lib/config.hpp>
#include <btcnew/lib/diagnosticsconfig.hpp>
#include <btcnew/lib/logger_mt.hpp>
#include <btcnew/lib/numbers.hpp>
#include <btcnew/node/lmdb/lmdb_env.hpp>
#include <btcnew/node/lmdb/lmdb_iterator.hpp>
#include <btcnew/node/lmdb/lmdb_txn.hpp>
#include <btcnew/secure/blockstore_partial.hpp>
#include <btcnew/secure/common.hpp>
#include <btcnew/secure/versioning.hpp>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

#include <thread>

#include <lmdb/libraries/liblmdb/lmdb.h>

namespace btcnew
{
using mdb_val = db_val<MDB_val>;

class logging_mt;
/**
 * mdb implementation of the block store
 */
class mdb_store : public block_store_partial<MDB_val, mdb_store>
{
public:
	using block_store_partial::block_exists;
	using block_store_partial::unchecked_put;

	mdb_store (btcnew::logger_mt &, boost::filesystem::path const &, btcnew::txn_tracking_config const & txn_tracking_config_a = btcnew::txn_tracking_config{}, std::chrono::milliseconds block_processor_batch_max_time_a = std::chrono::milliseconds (5000), int lmdb_max_dbs = 128, size_t batch_size = 512, bool backup_before_upgrade = false);
	btcnew::write_transaction tx_begin_write (std::vector<btcnew::tables> const & tables_requiring_lock = {}, std::vector<btcnew::tables> const & tables_no_lock = {}) override;
	btcnew::read_transaction tx_begin_read () override;

	bool block_info_get (btcnew::transaction const &, btcnew::block_hash const &, btcnew::block_info &) const override;

	void version_put (btcnew::write_transaction const &, int) override;

	void serialize_mdb_tracker (boost::property_tree::ptree &, std::chrono::milliseconds, std::chrono::milliseconds) override;

	static void create_backup_file (btcnew::mdb_env &, boost::filesystem::path const &, btcnew::logger_mt &);

private:
	btcnew::logger_mt & logger;
	bool error{ false };

public:
	btcnew::mdb_env env;

	/**
	 * Maps head block to owning account
	 * btcnew::block_hash -> btcnew::account
	 */
	MDB_dbi frontiers{ 0 };

	/**
	 * Maps account v1 to account information, head, rep, open, balance, timestamp and block count. (Removed)
	 * btcnew::account -> btcnew::block_hash, btcnew::block_hash, btcnew::block_hash, btcnew::amount, uint64_t, uint64_t
	 */
	MDB_dbi accounts_v0{ 0 };

	/**
	 * Maps account v0 to account information, head, rep, open, balance, timestamp and block count. (Removed)
	 * btcnew::account -> btcnew::block_hash, btcnew::block_hash, btcnew::block_hash, btcnew::amount, uint64_t, uint64_t
	 */
	MDB_dbi accounts_v1{ 0 };

	/**
	 * Maps account v0 to account information, head, rep, open, balance, timestamp, block count and epoch. (Removed)
	 * btcnew::account -> btcnew::block_hash, btcnew::block_hash, btcnew::block_hash, btcnew::amount, uint64_t, uint64_t, btcnew::epoch
	 */
	MDB_dbi accounts{ 0 };

	/**
	 * Maps block hash to send block.
	 * btcnew::block_hash -> btcnew::send_block
	 */
	MDB_dbi send_blocks{ 0 };

	/**
	 * Maps block hash to receive block.
	 * btcnew::block_hash -> btcnew::receive_block
	 */
	MDB_dbi receive_blocks{ 0 };

	/**
	 * Maps block hash to open block.
	 * btcnew::block_hash -> btcnew::open_block
	 */
	MDB_dbi open_blocks{ 0 };

	/**
	 * Maps block hash to change block.
	 * btcnew::block_hash -> btcnew::change_block
	 */
	MDB_dbi change_blocks{ 0 };

	/**
	 * Maps block hash to v0 state block. (Removed)
	 * btcnew::block_hash -> btcnew::state_block
	 */
	MDB_dbi state_blocks_v0{ 0 };

	/**
	 * Maps block hash to v1 state block. (Removed)
	 * btcnew::block_hash -> btcnew::state_block
	 */
	MDB_dbi state_blocks_v1{ 0 };

	/**
	 * Maps block hash to state block.
	 * btcnew::block_hash -> btcnew::state_block
	 */
	MDB_dbi state_blocks{ 0 };

	/**
	 * Maps min_version 0 (destination account, pending block) to (source account, amount). (Removed)
	 * btcnew::account, btcnew::block_hash -> btcnew::account, btcnew::amount
	 */
	MDB_dbi pending_v0{ 0 };

	/**
	 * Maps min_version 1 (destination account, pending block) to (source account, amount). (Removed)
	 * btcnew::account, btcnew::block_hash -> btcnew::account, btcnew::amount
	 */
	MDB_dbi pending_v1{ 0 };

	/**
	 * Maps (destination account, pending block) to (source account, amount, version). (Removed)
	 * btcnew::account, btcnew::block_hash -> btcnew::account, btcnew::amount, btcnew::epoch
	 */
	MDB_dbi pending{ 0 };

	/**
	 * Maps block hash to account and balance. (Removed)
	 * block_hash -> btcnew::account, btcnew::amount
	 */
	MDB_dbi blocks_info{ 0 };

	/**
	 * Representative weights. (Removed)
	 * btcnew::account -> btcnew::uint128_t
	 */
	MDB_dbi representation{ 0 };

	/**
	 * Unchecked bootstrap blocks info.
	 * btcnew::block_hash -> btcnew::unchecked_info
	 */
	MDB_dbi unchecked{ 0 };

	/**
	 * Highest vote observed for account.
	 * btcnew::account -> uint64_t
	 */
	MDB_dbi vote{ 0 };

	/**
	 * Samples of online vote weight
	 * uint64_t -> btcnew::amount
	 */
	MDB_dbi online_weight{ 0 };

	/**
	 * Meta information about block store, such as versions.
	 * btcnew::uint256_union (arbitrary key) -> blob
	 */
	MDB_dbi meta{ 0 };

	/*
	 * Endpoints for peers
	 * btcnew::endpoint_key -> no_value
	*/
	MDB_dbi peers{ 0 };

	/*
	 * Confirmation height of an account
	 * btcnew::account -> uint64_t
	 */
	MDB_dbi confirmation_height{ 0 };

	bool exists (btcnew::transaction const & transaction_a, tables table_a, btcnew::mdb_val const & key_a) const;

	int get (btcnew::transaction const & transaction_a, tables table_a, btcnew::mdb_val const & key_a, btcnew::mdb_val & value_a) const;
	int put (btcnew::write_transaction const & transaction_a, tables table_a, btcnew::mdb_val const & key_a, const btcnew::mdb_val & value_a) const;
	int del (btcnew::write_transaction const & transaction_a, tables table_a, btcnew::mdb_val const & key_a) const;

	bool copy_db (boost::filesystem::path const & destination_file) override;

	template <typename Key, typename Value>
	btcnew::store_iterator<Key, Value> make_iterator (btcnew::transaction const & transaction_a, tables table_a) const
	{
		return btcnew::store_iterator<Key, Value> (std::make_unique<btcnew::mdb_iterator<Key, Value>> (transaction_a, table_to_dbi (table_a)));
	}

	template <typename Key, typename Value>
	btcnew::store_iterator<Key, Value> make_iterator (btcnew::transaction const & transaction_a, tables table_a, btcnew::mdb_val const & key) const
	{
		return btcnew::store_iterator<Key, Value> (std::make_unique<btcnew::mdb_iterator<Key, Value>> (transaction_a, table_to_dbi (table_a), key));
	}

	bool init_error () const override;

	size_t count (btcnew::transaction const &, MDB_dbi) const;

	// These are only use in the upgrade process.
	std::shared_ptr<btcnew::block> block_get_v14 (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a, btcnew::block_sideband_v14 * sideband_a = nullptr, bool * is_state_v1 = nullptr) const override;
	bool entry_has_sideband_v14 (size_t entry_size_a, btcnew::block_type type_a) const;
	size_t block_successor_offset_v14 (btcnew::transaction const & transaction_a, size_t entry_size_a, btcnew::block_type type_a) const;
	btcnew::block_hash block_successor_v14 (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a) const;
	btcnew::mdb_val block_raw_get_v14 (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a, btcnew::block_type & type_a, bool * is_state_v1 = nullptr) const;
	boost::optional<btcnew::mdb_val> block_raw_get_by_type_v14 (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a, btcnew::block_type & type_a, bool * is_state_v1) const;
	btcnew::account block_account_computed_v14 (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a) const;
	btcnew::account block_account_v14 (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a) const;
	btcnew::uint128_t block_balance_computed_v14 (btcnew::transaction const & transaction_a, btcnew::block_hash const & hash_a) const;

private:
	bool do_upgrades (btcnew::write_transaction &, bool &, size_t);
	void upgrade_v1_to_v2 (btcnew::write_transaction const &);
	void upgrade_v2_to_v3 (btcnew::write_transaction const &);
	void upgrade_v3_to_v4 (btcnew::write_transaction const &);
	void upgrade_v4_to_v5 (btcnew::write_transaction const &);
	void upgrade_v5_to_v6 (btcnew::write_transaction const &);
	void upgrade_v6_to_v7 (btcnew::write_transaction const &);
	void upgrade_v7_to_v8 (btcnew::write_transaction const &);
	void upgrade_v8_to_v9 (btcnew::write_transaction const &);
	void upgrade_v10_to_v11 (btcnew::write_transaction const &);
	void upgrade_v11_to_v12 (btcnew::write_transaction const &);
	void upgrade_v12_to_v13 (btcnew::write_transaction &, size_t);
	void upgrade_v13_to_v14 (btcnew::write_transaction const &);
	void upgrade_v14_to_v15 (btcnew::write_transaction &);
	void open_databases (bool &, btcnew::transaction const &, unsigned);

	int drop (btcnew::write_transaction const & transaction_a, tables table_a) override;
	int clear (btcnew::write_transaction const & transaction_a, MDB_dbi handle_a);

	bool not_found (int status) const override;
	bool success (int status) const override;
	int status_code_not_found () const override;

	MDB_dbi table_to_dbi (tables table_a) const;

	btcnew::mdb_txn_tracker mdb_txn_tracker;
	btcnew::mdb_txn_callbacks create_txn_callbacks ();
	bool txn_tracking_enabled;

	size_t count (btcnew::transaction const & transaction_a, tables table_a) const override;

	bool vacuum_after_upgrade (boost::filesystem::path const & path_a, int lmdb_max_dbs);

	class upgrade_counters
	{
	public:
		upgrade_counters (uint64_t count_before_v0, uint64_t count_before_v1);
		bool are_equal () const;

		uint64_t before_v0;
		uint64_t before_v1;
		uint64_t after_v0{ 0 };
		uint64_t after_v1{ 0 };
	};
};

template <>
void * mdb_val::data () const;
template <>
size_t mdb_val::size () const;
template <>
mdb_val::db_val (size_t size_a, void * data_a);
template <>
void mdb_val::convert_buffer_to_value ();
}

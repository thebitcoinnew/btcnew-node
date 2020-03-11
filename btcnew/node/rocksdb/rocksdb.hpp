#pragma once

#include <btcnew/lib/config.hpp>
#include <btcnew/lib/logger_mt.hpp>
#include <btcnew/lib/numbers.hpp>
#include <btcnew/node/rocksdb/rocksdb_iterator.hpp>
#include <btcnew/secure/blockstore_partial.hpp>
#include <btcnew/secure/common.hpp>

#include <rocksdb/db.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/table.h>
#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>

namespace btcnew
{
class logging_mt;
class rocksdb_config;
/**
 * rocksdb implementation of the block store
 */
class rocksdb_store : public block_store_partial<rocksdb::Slice, rocksdb_store>
{
public:
	rocksdb_store (btcnew::logger_mt &, boost::filesystem::path const &, btcnew::rocksdb_config const & = btcnew::rocksdb_config{}, bool open_read_only = false);
	~rocksdb_store ();
	btcnew::write_transaction tx_begin_write (std::vector<btcnew::tables> const & tables_requiring_lock = {}, std::vector<btcnew::tables> const & tables_no_lock = {}) override;
	btcnew::read_transaction tx_begin_read () override;

	bool block_info_get (btcnew::transaction const &, btcnew::block_hash const &, btcnew::block_info &) const override;
	size_t count (btcnew::transaction const & transaction_a, tables table_a) const override;
	void version_put (btcnew::write_transaction const &, int) override;

	bool exists (btcnew::transaction const & transaction_a, tables table_a, btcnew::rocksdb_val const & key_a) const;
	int get (btcnew::transaction const & transaction_a, tables table_a, btcnew::rocksdb_val const & key_a, btcnew::rocksdb_val & value_a) const;
	int put (btcnew::write_transaction const & transaction_a, tables table_a, btcnew::rocksdb_val const & key_a, btcnew::rocksdb_val const & value_a);
	int del (btcnew::write_transaction const & transaction_a, tables table_a, btcnew::rocksdb_val const & key_a);

	void serialize_mdb_tracker (boost::property_tree::ptree &, std::chrono::milliseconds, std::chrono::milliseconds) override
	{
		// Do nothing
	}

	std::shared_ptr<btcnew::block> block_get_v14 (btcnew::transaction const &, btcnew::block_hash const &, btcnew::block_sideband_v14 * = nullptr, bool * = nullptr) const override
	{
		// Should not be called as RocksDB has no such upgrade path
		release_assert (false);
		return nullptr;
	}

	bool copy_db (boost::filesystem::path const & destination) override;

	template <typename Key, typename Value>
	btcnew::store_iterator<Key, Value> make_iterator (btcnew::transaction const & transaction_a, tables table_a) const
	{
		return btcnew::store_iterator<Key, Value> (std::make_unique<btcnew::rocksdb_iterator<Key, Value>> (db, transaction_a, table_to_column_family (table_a)));
	}

	template <typename Key, typename Value>
	btcnew::store_iterator<Key, Value> make_iterator (btcnew::transaction const & transaction_a, tables table_a, btcnew::rocksdb_val const & key) const
	{
		return btcnew::store_iterator<Key, Value> (std::make_unique<btcnew::rocksdb_iterator<Key, Value>> (db, transaction_a, table_to_column_family (table_a), key));
	}

	bool init_error () const override;

private:
	bool error{ false };
	btcnew::logger_mt & logger;
	std::vector<rocksdb::ColumnFamilyHandle *> handles;
	// Optimistic transactions are used in write mode
	rocksdb::OptimisticTransactionDB * optimistic_db = nullptr;
	rocksdb::DB * db = nullptr;
	std::shared_ptr<rocksdb::TableFactory> table_factory;
	std::unordered_map<btcnew::tables, std::mutex> write_lock_mutexes;

	rocksdb::Transaction * tx (btcnew::transaction const & transaction_a) const;
	std::vector<btcnew::tables> all_tables () const;

	bool not_found (int status) const override;
	bool success (int status) const override;
	int status_code_not_found () const override;
	int drop (btcnew::write_transaction const &, tables) override;

	rocksdb::ColumnFamilyHandle * table_to_column_family (tables table_a) const;
	int clear (rocksdb::ColumnFamilyHandle * column_family);

	void open (bool & error_a, boost::filesystem::path const & path_a, bool open_read_only_a);
	uint64_t count (btcnew::transaction const & transaction_a, rocksdb::ColumnFamilyHandle * handle) const;
	bool is_caching_counts (btcnew::tables table_a) const;

	int increment (btcnew::write_transaction const & transaction_a, tables table_a, btcnew::rocksdb_val const & key_a, uint64_t amount_a);
	int decrement (btcnew::write_transaction const & transaction_a, tables table_a, btcnew::rocksdb_val const & key_a, uint64_t amount_a);
	rocksdb::ColumnFamilyOptions get_cf_options () const;
	void construct_column_family_mutexes ();
	rocksdb::Options get_db_options () const;
	rocksdb::BlockBasedTableOptions get_table_options () const;
	btcnew::rocksdb_config rocksdb_config;
};
}

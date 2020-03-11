#pragma once

#include <btcnew/lib/config.hpp>
#include <btcnew/node/lmdb/lmdb.hpp>
#include <btcnew/node/lmdb/wallet_value.hpp>
#include <btcnew/node/openclwork.hpp>
#include <btcnew/secure/blockstore.hpp>
#include <btcnew/secure/common.hpp>

#include <boost/thread/thread.hpp>

#include <mutex>
#include <unordered_set>

namespace btcnew
{
class node;
class node_config;
class wallets;
// The fan spreads a key out over the heap to decrease the likelihood of it being recovered by memory inspection
class fan final
{
public:
	fan (btcnew::uint256_union const &, size_t);
	void value (btcnew::raw_key &);
	void value_set (btcnew::raw_key const &);
	std::vector<std::unique_ptr<btcnew::uint256_union>> values;

private:
	std::mutex mutex;
	void value_get (btcnew::raw_key &);
};
class kdf final
{
public:
	void phs (btcnew::raw_key &, std::string const &, btcnew::uint256_union const &);
	std::mutex mutex;
};
enum class key_type
{
	not_a_type,
	unknown,
	adhoc,
	deterministic
};
class wallet_store final
{
public:
	wallet_store (bool &, btcnew::kdf &, btcnew::transaction &, btcnew::account, unsigned, std::string const &);
	wallet_store (bool &, btcnew::kdf &, btcnew::transaction &, btcnew::account, unsigned, std::string const &, std::string const &);
	std::vector<btcnew::account> accounts (btcnew::transaction const &);
	void initialize (btcnew::transaction const &, bool &, std::string const &);
	btcnew::uint256_union check (btcnew::transaction const &);
	bool rekey (btcnew::transaction const &, std::string const &);
	bool valid_password (btcnew::transaction const &);
	bool valid_public_key (btcnew::public_key const &);
	bool attempt_password (btcnew::transaction const &, std::string const &);
	void wallet_key (btcnew::raw_key &, btcnew::transaction const &);
	void seed (btcnew::raw_key &, btcnew::transaction const &);
	void seed_set (btcnew::transaction const &, btcnew::raw_key const &);
	btcnew::key_type key_type (btcnew::wallet_value const &);
	btcnew::public_key deterministic_insert (btcnew::transaction const &);
	btcnew::public_key deterministic_insert (btcnew::transaction const &, uint32_t const);
	btcnew::private_key deterministic_key (btcnew::transaction const &, uint32_t);
	uint32_t deterministic_index_get (btcnew::transaction const &);
	void deterministic_index_set (btcnew::transaction const &, uint32_t);
	void deterministic_clear (btcnew::transaction const &);
	btcnew::uint256_union salt (btcnew::transaction const &);
	bool is_representative (btcnew::transaction const &);
	btcnew::account representative (btcnew::transaction const &);
	void representative_set (btcnew::transaction const &, btcnew::account const &);
	btcnew::public_key insert_adhoc (btcnew::transaction const &, btcnew::raw_key const &);
	bool insert_watch (btcnew::transaction const &, btcnew::account const &);
	void erase (btcnew::transaction const &, btcnew::account const &);
	btcnew::wallet_value entry_get_raw (btcnew::transaction const &, btcnew::account const &);
	void entry_put_raw (btcnew::transaction const &, btcnew::account const &, btcnew::wallet_value const &);
	bool fetch (btcnew::transaction const &, btcnew::account const &, btcnew::raw_key &);
	bool exists (btcnew::transaction const &, btcnew::account const &);
	void destroy (btcnew::transaction const &);
	btcnew::store_iterator<btcnew::account, btcnew::wallet_value> find (btcnew::transaction const &, btcnew::account const &);
	btcnew::store_iterator<btcnew::account, btcnew::wallet_value> begin (btcnew::transaction const &, btcnew::account const &);
	btcnew::store_iterator<btcnew::account, btcnew::wallet_value> begin (btcnew::transaction const &);
	btcnew::store_iterator<btcnew::account, btcnew::wallet_value> end ();
	void derive_key (btcnew::raw_key &, btcnew::transaction const &, std::string const &);
	void serialize_json (btcnew::transaction const &, std::string &);
	void write_backup (btcnew::transaction const &, boost::filesystem::path const &);
	bool move (btcnew::transaction const &, btcnew::wallet_store &, std::vector<btcnew::public_key> const &);
	bool import (btcnew::transaction const &, btcnew::wallet_store &);
	bool work_get (btcnew::transaction const &, btcnew::public_key const &, uint64_t &);
	void work_put (btcnew::transaction const &, btcnew::public_key const &, uint64_t);
	unsigned version (btcnew::transaction const &);
	void version_put (btcnew::transaction const &, unsigned);
	void upgrade_v1_v2 (btcnew::transaction const &);
	void upgrade_v2_v3 (btcnew::transaction const &);
	void upgrade_v3_v4 (btcnew::transaction const &);
	btcnew::fan password;
	btcnew::fan wallet_key_mem;
	static unsigned const version_1 = 1;
	static unsigned const version_2 = 2;
	static unsigned const version_3 = 3;
	static unsigned const version_4 = 4;
	static unsigned constexpr version_current = version_4;
	static btcnew::account const version_special;
	static btcnew::account const wallet_key_special;
	static btcnew::account const salt_special;
	static btcnew::account const check_special;
	static btcnew::account const representative_special;
	static btcnew::account const seed_special;
	static btcnew::account const deterministic_index_special;
	static size_t const check_iv_index;
	static size_t const seed_iv_index;
	static int const special_count;
	btcnew::kdf & kdf;
	MDB_dbi handle{ 0 };
	std::recursive_mutex mutex;

private:
	MDB_txn * tx (btcnew::transaction const &) const;
};
// A wallet is a set of account keys encrypted by a common encryption key
class wallet final : public std::enable_shared_from_this<btcnew::wallet>
{
public:
	std::shared_ptr<btcnew::block> change_action (btcnew::account const &, btcnew::account const &, uint64_t = 0, bool = true);
	std::shared_ptr<btcnew::block> receive_action (btcnew::block const &, btcnew::account const &, btcnew::uint128_union const &, uint64_t = 0, bool = true);
	std::shared_ptr<btcnew::block> send_action (btcnew::account const &, btcnew::account const &, btcnew::uint128_t const &, uint64_t = 0, bool = true, boost::optional<std::string> = {});
	bool action_complete (std::shared_ptr<btcnew::block> const &, btcnew::account const &, bool const);
	wallet (bool &, btcnew::transaction &, btcnew::wallets &, std::string const &);
	wallet (bool &, btcnew::transaction &, btcnew::wallets &, std::string const &, std::string const &);
	void enter_initial_password ();
	bool enter_password (btcnew::transaction const &, std::string const &);
	btcnew::public_key insert_adhoc (btcnew::raw_key const &, bool = true);
	btcnew::public_key insert_adhoc (btcnew::transaction const &, btcnew::raw_key const &, bool = true);
	bool insert_watch (btcnew::transaction const &, btcnew::public_key const &);
	btcnew::public_key deterministic_insert (btcnew::transaction const &, bool = true);
	btcnew::public_key deterministic_insert (uint32_t, bool = true);
	btcnew::public_key deterministic_insert (bool = true);
	bool exists (btcnew::public_key const &);
	bool import (std::string const &, std::string const &);
	void serialize (std::string &);
	bool change_sync (btcnew::account const &, btcnew::account const &);
	void change_async (btcnew::account const &, btcnew::account const &, std::function<void (std::shared_ptr<btcnew::block>)> const &, uint64_t = 0, bool = true);
	bool receive_sync (std::shared_ptr<btcnew::block>, btcnew::account const &, btcnew::uint128_t const &);
	void receive_async (std::shared_ptr<btcnew::block>, btcnew::account const &, btcnew::uint128_t const &, std::function<void (std::shared_ptr<btcnew::block>)> const &, uint64_t = 0, bool = true);
	btcnew::block_hash send_sync (btcnew::account const &, btcnew::account const &, btcnew::uint128_t const &);
	void send_async (btcnew::account const &, btcnew::account const &, btcnew::uint128_t const &, std::function<void (std::shared_ptr<btcnew::block>)> const &, uint64_t = 0, bool = true, boost::optional<std::string> = {});
	void work_cache_blocking (btcnew::account const &, btcnew::root const &);
	void work_update (btcnew::transaction const &, btcnew::account const &, btcnew::root const &, uint64_t);
	void work_ensure (btcnew::account const &, btcnew::root const &);
	bool search_pending ();
	void init_free_accounts (btcnew::transaction const &);
	uint32_t deterministic_check (btcnew::transaction const & transaction_a, uint32_t index);
	/** Changes the wallet seed and returns the first account */
	btcnew::public_key change_seed (btcnew::transaction const & transaction_a, btcnew::raw_key const & prv_a, uint32_t count = 0);
	void deterministic_restore (btcnew::transaction const & transaction_a);
	bool live ();
	btcnew::network_params network_params;
	std::unordered_set<btcnew::account> free_accounts;
	std::function<void (bool, bool)> lock_observer;
	btcnew::wallet_store store;
	btcnew::wallets & wallets;
	std::mutex representatives_mutex;
	std::unordered_set<btcnew::account> representatives;
};

class work_watcher final : public std::enable_shared_from_this<btcnew::work_watcher>
{
public:
	work_watcher (btcnew::node &);
	~work_watcher ();
	void stop ();
	void add (std::shared_ptr<btcnew::block>);
	void update (btcnew::qualified_root const &, std::shared_ptr<btcnew::state_block>);
	void watching (btcnew::qualified_root const &, std::shared_ptr<btcnew::state_block>);
	void remove (std::shared_ptr<btcnew::block>);
	bool is_watched (btcnew::qualified_root const &);
	size_t size ();
	std::mutex mutex;
	btcnew::node & node;
	std::unordered_map<btcnew::qualified_root, std::shared_ptr<btcnew::state_block>> watched;
	std::atomic<bool> stopped;
};
/**
 * The wallets set is all the wallets a node controls.
 * A node may contain multiple wallets independently encrypted and operated.
 */
class wallets final
{
public:
	wallets (bool, btcnew::node &);
	~wallets ();
	std::shared_ptr<btcnew::wallet> open (btcnew::wallet_id const &);
	std::shared_ptr<btcnew::wallet> create (btcnew::wallet_id const &);
	bool search_pending (btcnew::wallet_id const &);
	void search_pending_all ();
	void destroy (btcnew::wallet_id const &);
	void reload ();
	void do_wallet_actions ();
	void queue_wallet_action (btcnew::uint128_t const &, std::shared_ptr<btcnew::wallet>, std::function<void (btcnew::wallet &)> const &);
	void foreach_representative (std::function<void (btcnew::public_key const &, btcnew::raw_key const &)> const &);
	bool exists (btcnew::transaction const &, btcnew::public_key const &);
	void stop ();
	void clear_send_ids (btcnew::transaction const &);
	bool check_rep (btcnew::account const &, btcnew::uint128_t const &);
	void compute_reps ();
	void ongoing_compute_reps ();
	void split_if_needed (btcnew::transaction &, btcnew::block_store &);
	void move_table (std::string const &, MDB_txn *, MDB_txn *);
	btcnew::network_params network_params;
	std::function<void (bool)> observer;
	std::unordered_map<btcnew::wallet_id, std::shared_ptr<btcnew::wallet>> items;
	std::multimap<btcnew::uint128_t, std::pair<std::shared_ptr<btcnew::wallet>, std::function<void (btcnew::wallet &)>>, std::greater<btcnew::uint128_t>> actions;
	std::mutex mutex;
	std::mutex action_mutex;
	btcnew::condition_variable condition;
	btcnew::kdf kdf;
	MDB_dbi handle;
	MDB_dbi send_action_ids;
	btcnew::node & node;
	btcnew::mdb_env & env;
	std::atomic<bool> stopped;
	std::shared_ptr<btcnew::work_watcher> watcher;
	boost::thread thread;
	static btcnew::uint128_t const generate_priority;
	static btcnew::uint128_t const high_priority;
	std::atomic<uint64_t> reps_count{ 0 };
	std::atomic<uint64_t> half_principal_reps_count{ 0 }; // Representatives with at least 50% of principal representative requirements

	/** Start read-write transaction */
	btcnew::write_transaction tx_begin_write ();

	/** Start read-only transaction */
	btcnew::read_transaction tx_begin_read ();
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (wallets & wallets, const std::string & name);

class wallets_store
{
public:
	virtual ~wallets_store () = default;
	virtual bool init_error () const = 0;
};
class mdb_wallets_store final : public wallets_store
{
public:
	mdb_wallets_store (boost::filesystem::path const &, int lmdb_max_dbs = 128);
	btcnew::mdb_env environment;
	bool init_error () const override;
	bool error{ false };
};
}

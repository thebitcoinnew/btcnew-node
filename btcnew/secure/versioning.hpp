#pragma once

#include <btcnew/lib/blocks.hpp>
#include <btcnew/secure/common.hpp>
#include <btcnew/secure/utility.hpp>

struct MDB_val;

namespace btcnew
{
class account_info_v1 final
{
public:
	account_info_v1 () = default;
	explicit account_info_v1 (MDB_val const &);
	account_info_v1 (btcnew::block_hash const &, btcnew::block_hash const &, btcnew::amount const &, uint64_t);
	btcnew::block_hash head{ 0 };
	btcnew::block_hash rep_block{ 0 };
	btcnew::amount balance{ 0 };
	uint64_t modified{ 0 };
};
class pending_info_v3 final
{
public:
	pending_info_v3 () = default;
	explicit pending_info_v3 (MDB_val const &);
	pending_info_v3 (btcnew::account const &, btcnew::amount const &, btcnew::account const &);
	btcnew::account source{ 0 };
	btcnew::amount amount{ 0 };
	btcnew::account destination{ 0 };
};
class pending_info_v14 final
{
public:
	pending_info_v14 () = default;
	pending_info_v14 (btcnew::account const &, btcnew::amount const &, btcnew::epoch);
	size_t db_size () const;
	bool deserialize (btcnew::stream &);
	bool operator== (btcnew::pending_info_v14 const &) const;
	btcnew::account source{ 0 };
	btcnew::amount amount{ 0 };
	btcnew::epoch epoch{ btcnew::epoch::epoch_0 };
};
class account_info_v5 final
{
public:
	account_info_v5 () = default;
	explicit account_info_v5 (MDB_val const &);
	account_info_v5 (btcnew::block_hash const &, btcnew::block_hash const &, btcnew::block_hash const &, btcnew::amount const &, uint64_t);
	btcnew::block_hash head{ 0 };
	btcnew::block_hash rep_block{ 0 };
	btcnew::block_hash open_block{ 0 };
	btcnew::amount balance{ 0 };
	uint64_t modified{ 0 };
};
class account_info_v13 final
{
public:
	account_info_v13 () = default;
	account_info_v13 (btcnew::block_hash const &, btcnew::block_hash const &, btcnew::block_hash const &, btcnew::amount const &, uint64_t, uint64_t, btcnew::epoch);
	size_t db_size () const;
	btcnew::block_hash head{ 0 };
	btcnew::block_hash rep_block{ 0 };
	btcnew::block_hash open_block{ 0 };
	btcnew::amount balance{ 0 };
	uint64_t modified{ 0 };
	uint64_t block_count{ 0 };
	btcnew::epoch epoch{ btcnew::epoch::epoch_0 };
};
class account_info_v14 final
{
public:
	account_info_v14 () = default;
	account_info_v14 (btcnew::block_hash const &, btcnew::block_hash const &, btcnew::block_hash const &, btcnew::amount const &, uint64_t, uint64_t, uint64_t, btcnew::epoch);
	size_t db_size () const;
	btcnew::block_hash head{ 0 };
	btcnew::block_hash rep_block{ 0 };
	btcnew::block_hash open_block{ 0 };
	btcnew::amount balance{ 0 };
	uint64_t modified{ 0 };
	uint64_t block_count{ 0 };
	uint64_t confirmation_height{ 0 };
	btcnew::epoch epoch{ btcnew::epoch::epoch_0 };
};
class block_sideband_v14 final
{
public:
	block_sideband_v14 () = default;
	block_sideband_v14 (btcnew::block_type, btcnew::account const &, btcnew::block_hash const &, btcnew::amount const &, uint64_t, uint64_t);
	void serialize (btcnew::stream &) const;
	bool deserialize (btcnew::stream &);
	static size_t size (btcnew::block_type);
	btcnew::block_type type{ btcnew::block_type::invalid };
	btcnew::block_hash successor{ 0 };
	btcnew::account account{ 0 };
	btcnew::amount balance{ 0 };
	uint64_t height{ 0 };
	uint64_t timestamp{ 0 };
};
class state_block_w_sideband_v14
{
public:
	std::shared_ptr<btcnew::state_block> state_block;
	btcnew::block_sideband_v14 sideband;
};
}

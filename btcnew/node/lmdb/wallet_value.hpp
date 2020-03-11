#pragma once

#include <btcnew/lib/numbers.hpp>
#include <btcnew/secure/blockstore.hpp>

#include <lmdb/libraries/liblmdb/lmdb.h>

namespace btcnew
{
class wallet_value
{
public:
	wallet_value () = default;
	wallet_value (btcnew::db_val<MDB_val> const &);
	wallet_value (btcnew::uint256_union const &, uint64_t);
	btcnew::db_val<MDB_val> val () const;
	btcnew::uint256_union key;
	uint64_t work;
};
}

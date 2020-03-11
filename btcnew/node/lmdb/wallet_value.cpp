#include <btcnew/node/lmdb/wallet_value.hpp>

btcnew::wallet_value::wallet_value (btcnew::db_val<MDB_val> const & val_a)
{
	assert (val_a.size () == sizeof (*this));
	std::copy (reinterpret_cast<uint8_t const *> (val_a.data ()), reinterpret_cast<uint8_t const *> (val_a.data ()) + sizeof (key), key.chars.begin ());
	std::copy (reinterpret_cast<uint8_t const *> (val_a.data ()) + sizeof (key), reinterpret_cast<uint8_t const *> (val_a.data ()) + sizeof (key) + sizeof (work), reinterpret_cast<char *> (&work));
}

btcnew::wallet_value::wallet_value (btcnew::uint256_union const & key_a, uint64_t work_a) :
key (key_a),
work (work_a)
{
}

btcnew::db_val<MDB_val> btcnew::wallet_value::val () const
{
	static_assert (sizeof (*this) == sizeof (key) + sizeof (work), "Class not packed");
	return btcnew::db_val<MDB_val> (sizeof (*this), const_cast<btcnew::wallet_value *> (this));
}

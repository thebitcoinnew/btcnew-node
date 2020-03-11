#pragma once

#include <btcnew/lib/numbers.hpp>

#include <type_traits>
#include <unordered_map>

namespace btcnew
{
/**
 * Tag for which epoch an entry belongs to
 */
enum class epoch : uint8_t
{
	invalid = 0,
	unspecified = 1,
	epoch_begin = 2,
	epoch_0 = 2,
	epoch_1 = 3,
	epoch_2 = 4,
	max = epoch_2
};

/* This turns epoch_0 into 0 for instance */
std::underlying_type_t<btcnew::epoch> normalized_epoch (btcnew::epoch epoch_a);
}
namespace std
{
template <>
struct hash<::btcnew::epoch>
{
	std::size_t operator() (::btcnew::epoch const & epoch_a) const
	{
		std::hash<std::underlying_type_t<::btcnew::epoch>> hash;
		return hash (static_cast<std::underlying_type_t<::btcnew::epoch>> (epoch_a));
	}
};
}
namespace btcnew
{
class epoch_info
{
public:
	btcnew::public_key signer;
	btcnew::link link;
};
class epochs
{
public:
	bool is_epoch_link (btcnew::link const & link_a) const;
	btcnew::link const & link (btcnew::epoch epoch_a) const;
	btcnew::public_key const & signer (btcnew::epoch epoch_a) const;
	btcnew::epoch epoch (btcnew::link const & link_a) const;
	void add (btcnew::epoch epoch_a, btcnew::public_key const & signer_a, btcnew::link const & link_a);
	/** Checks that new_epoch is 1 version higher than epoch */
	static bool is_sequential (btcnew::epoch epoch_a, btcnew::epoch new_epoch_a);

private:
	std::unordered_map<btcnew::epoch, btcnew::epoch_info> epochs_m;
};
}

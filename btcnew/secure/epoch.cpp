#include <btcnew/secure/epoch.hpp>

btcnew::link const & btcnew::epochs::link (btcnew::epoch epoch_a) const
{
	return epochs_m.at (epoch_a).link;
}

bool btcnew::epochs::is_epoch_link (btcnew::link const & link_a) const
{
	return std::any_of (epochs_m.begin (), epochs_m.end (), [&link_a] (auto const & item_a) { return item_a.second.link == link_a; });
}

btcnew::public_key const & btcnew::epochs::signer (btcnew::epoch epoch_a) const
{
	return epochs_m.at (epoch_a).signer;
}

btcnew::epoch btcnew::epochs::epoch (btcnew::link const & link_a) const
{
	auto existing (std::find_if (epochs_m.begin (), epochs_m.end (), [&link_a] (auto const & item_a) { return item_a.second.link == link_a; }));
	assert (existing != epochs_m.end ());
	return existing->first;
}

void btcnew::epochs::add (btcnew::epoch epoch_a, btcnew::public_key const & signer_a, btcnew::link const & link_a)
{
	assert (epochs_m.find (epoch_a) == epochs_m.end ());
	epochs_m[epoch_a] = { signer_a, link_a };
}

bool btcnew::epochs::is_sequential (btcnew::epoch epoch_a, btcnew::epoch new_epoch_a)
{
	auto head_epoch = std::underlying_type_t<btcnew::epoch> (epoch_a);
	bool is_valid_epoch (head_epoch >= std::underlying_type_t<btcnew::epoch> (btcnew::epoch::epoch_0));
	return is_valid_epoch && (std::underlying_type_t<btcnew::epoch> (new_epoch_a) == (head_epoch + 1));
}

std::underlying_type_t<btcnew::epoch> btcnew::normalized_epoch (btcnew::epoch epoch_a)
{
	// Currently assumes that the epoch versions in the enum are sequential.
	auto start = std::underlying_type_t<btcnew::epoch> (btcnew::epoch::epoch_0);
	auto end = std::underlying_type_t<btcnew::epoch> (epoch_a);
	assert (end >= start);
	return end - start;
}

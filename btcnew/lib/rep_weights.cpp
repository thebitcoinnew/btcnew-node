#include <btcnew/lib/rep_weights.hpp>
#include <btcnew/secure/blockstore.hpp>

void btcnew::rep_weights::representation_add (btcnew::account const & source_rep, btcnew::uint128_t const & amount_a)
{
	btcnew::lock_guard<std::mutex> guard (mutex);
	auto source_previous (get (source_rep));
	put (source_rep, source_previous + amount_a);
}

void btcnew::rep_weights::representation_put (btcnew::account const & account_a, btcnew::uint128_union const & representation_a)
{
	btcnew::lock_guard<std::mutex> guard (mutex);
	put (account_a, representation_a);
}

btcnew::uint128_t btcnew::rep_weights::representation_get (btcnew::account const & account_a)
{
	btcnew::lock_guard<std::mutex> lk (mutex);
	return get (account_a);
}

/** Makes a copy */
std::unordered_map<btcnew::account, btcnew::uint128_t> btcnew::rep_weights::get_rep_amounts ()
{
	btcnew::lock_guard<std::mutex> guard (mutex);
	return rep_amounts;
}

void btcnew::rep_weights::put (btcnew::account const & account_a, btcnew::uint128_union const & representation_a)
{
	auto it = rep_amounts.find (account_a);
	auto amount = representation_a.number ();
	if (it != rep_amounts.end ())
	{
		it->second = amount;
	}
	else
	{
		rep_amounts.emplace (account_a, amount);
	}
}

btcnew::uint128_t btcnew::rep_weights::get (btcnew::account const & account_a)
{
	auto it = rep_amounts.find (account_a);
	if (it != rep_amounts.end ())
	{
		return it->second;
	}
	else
	{
		return btcnew::uint128_t{ 0 };
	}
}

std::unique_ptr<btcnew::seq_con_info_component> btcnew::collect_seq_con_info (btcnew::rep_weights & rep_weights, const std::string & name)
{
	size_t rep_amounts_count = 0;

	{
		btcnew::lock_guard<std::mutex> guard (rep_weights.mutex);
		rep_amounts_count = rep_weights.rep_amounts.size ();
	}
	auto sizeof_element = sizeof (decltype (rep_weights.rep_amounts)::value_type);
	auto composite = std::make_unique<btcnew::seq_con_info_composite> (name);
	composite->add_component (std::make_unique<btcnew::seq_con_info_leaf> (seq_con_info{ "rep_amounts", rep_amounts_count, sizeof_element }));
	return composite;
}

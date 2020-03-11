#include <btcnew/node/node.hpp>
#include <btcnew/node/online_reps.hpp>

#include <cassert>

btcnew::online_reps::online_reps (btcnew::node & node_a, btcnew::uint128_t minimum_a) :
node (node_a),
minimum (minimum_a)
{
	if (!node.ledger.store.init_error ())
	{
		auto transaction (node.ledger.store.tx_begin_read ());
		online = trend (transaction);
	}
}

void btcnew::online_reps::observe (btcnew::account const & rep_a)
{
	if (node.ledger.weight (rep_a) > 0)
	{
		btcnew::lock_guard<std::mutex> lock (mutex);
		reps.insert (rep_a);
	}
}

void btcnew::online_reps::sample ()
{
	auto transaction (node.ledger.store.tx_begin_write ());
	// Discard oldest entries
	while (node.ledger.store.online_weight_count (transaction) >= node.network_params.node.max_weight_samples)
	{
		auto oldest (node.ledger.store.online_weight_begin (transaction));
		assert (oldest != node.ledger.store.online_weight_end ());
		node.ledger.store.online_weight_del (transaction, oldest->first);
	}
	// Calculate current active rep weight
	btcnew::uint128_t current;
	std::unordered_set<btcnew::account> reps_copy;
	{
		btcnew::lock_guard<std::mutex> lock (mutex);
		reps_copy.swap (reps);
	}
	for (auto & i : reps_copy)
	{
		current += node.ledger.weight (i);
	}
	node.ledger.store.online_weight_put (transaction, std::chrono::system_clock::now ().time_since_epoch ().count (), current);
	auto trend_l (trend (transaction));
	btcnew::lock_guard<std::mutex> lock (mutex);
	online = trend_l;
}

btcnew::uint128_t btcnew::online_reps::trend (btcnew::transaction & transaction_a)
{
	std::vector<btcnew::uint128_t> items;
	items.reserve (node.network_params.node.max_weight_samples + 1);
	items.push_back (minimum);
	for (auto i (node.ledger.store.online_weight_begin (transaction_a)), n (node.ledger.store.online_weight_end ()); i != n; ++i)
	{
		items.push_back (i->second.number ());
	}

	// Pick median value for our target vote weight
	auto median_idx = items.size () / 2;
	nth_element (items.begin (), items.begin () + median_idx, items.end ());
	return btcnew::uint128_t{ items[median_idx] };
}

btcnew::uint128_t btcnew::online_reps::online_stake () const
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	return std::max (online, minimum);
}

std::vector<btcnew::account> btcnew::online_reps::list ()
{
	std::vector<btcnew::account> result;
	btcnew::lock_guard<std::mutex> lock (mutex);
	for (auto & i : reps)
	{
		result.push_back (i);
	}
	return result;
}

namespace btcnew
{
std::unique_ptr<seq_con_info_component> collect_seq_con_info (online_reps & online_reps, const std::string & name)
{
	size_t count = 0;
	{
		btcnew::lock_guard<std::mutex> guard (online_reps.mutex);
		count = online_reps.reps.size ();
	}

	auto sizeof_element = sizeof (decltype (online_reps.reps)::value_type);
	auto composite = std::make_unique<seq_con_info_composite> (name);
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "arrival", count, sizeof_element }));
	return composite;
}
}

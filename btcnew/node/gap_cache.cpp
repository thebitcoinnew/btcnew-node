#include <btcnew/node/gap_cache.hpp>
#include <btcnew/node/node.hpp>
#include <btcnew/secure/blockstore.hpp>

btcnew::gap_cache::gap_cache (btcnew::node & node_a) :
node (node_a)
{
}

void btcnew::gap_cache::add (btcnew::block_hash const & hash_a, std::chrono::steady_clock::time_point time_point_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	auto existing (blocks.get<1> ().find (hash_a));
	if (existing != blocks.get<1> ().end ())
	{
		blocks.get<1> ().modify (existing, [time_point_a] (btcnew::gap_information & info) {
			info.arrival = time_point_a;
		});
	}
	else
	{
		blocks.insert ({ time_point_a, hash_a, std::vector<btcnew::account> () });
		if (blocks.size () > max)
		{
			blocks.get<0> ().erase (blocks.get<0> ().begin ());
		}
	}
}

void btcnew::gap_cache::erase (btcnew::block_hash const & hash_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	blocks.get<1> ().erase (hash_a);
}

void btcnew::gap_cache::vote (std::shared_ptr<btcnew::vote> vote_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	for (auto hash : *vote_a)
	{
		auto existing (blocks.get<1> ().find (hash));
		if (existing != blocks.get<1> ().end () && !existing->confirmed)
		{
			auto is_new (false);
			blocks.get<1> ().modify (existing, [&is_new, &vote_a] (btcnew::gap_information & info) {
				auto it = std::find (info.voters.begin (), info.voters.end (), vote_a->account);
				is_new = (it == info.voters.end ());
				if (is_new)
				{
					info.voters.push_back (vote_a->account);
				}
			});

			if (is_new)
			{
				if (bootstrap_check (existing->voters, hash))
				{
					blocks.get<1> ().modify (existing, [] (btcnew::gap_information & info) {
						info.confirmed = true;
					});
				}
			}
		}
	}
}

bool btcnew::gap_cache::bootstrap_check (std::vector<btcnew::account> const & voters_a, btcnew::block_hash const & hash_a)
{
	uint128_t tally;
	for (auto & voter : voters_a)
	{
		tally += node.ledger.weight (voter);
	}
	bool start_bootstrap (false);
	if (!node.flags.disable_lazy_bootstrap)
	{
		if (tally >= node.config.online_weight_minimum.number ())
		{
			start_bootstrap = true;
		}
	}
	else if (!node.flags.disable_legacy_bootstrap && tally > bootstrap_threshold ())
	{
		start_bootstrap = true;
	}
	if (start_bootstrap)
	{
		auto node_l (node.shared ());
		auto now (std::chrono::steady_clock::now ());
		node.alarm.add (node_l->network_params.network.is_test_network () ? now + std::chrono::milliseconds (5) : now + std::chrono::seconds (5), [node_l, hash_a] () {
			auto transaction (node_l->store.tx_begin_read ());
			if (!node_l->store.block_exists (transaction, hash_a))
			{
				if (!node_l->bootstrap_initiator.in_progress ())
				{
					node_l->logger.try_log (boost::str (boost::format ("Missing block %1% which has enough votes to warrant lazy bootstrapping it") % hash_a.to_string ()));
				}
				if (!node_l->flags.disable_lazy_bootstrap)
				{
					node_l->bootstrap_initiator.bootstrap_lazy (hash_a);
				}
				else if (!node_l->flags.disable_legacy_bootstrap)
				{
					node_l->bootstrap_initiator.bootstrap ();
				}
			}
		});
	}
	return start_bootstrap;
}

btcnew::uint128_t btcnew::gap_cache::bootstrap_threshold ()
{
	auto result ((node.online_reps.online_stake () / 256) * node.config.bootstrap_fraction_numerator);
	return result;
}

size_t btcnew::gap_cache::size ()
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	return blocks.size ();
}

namespace btcnew
{
std::unique_ptr<seq_con_info_component> collect_seq_con_info (gap_cache & gap_cache, const std::string & name)
{
	auto count = gap_cache.size ();
	auto sizeof_element = sizeof (decltype (gap_cache.blocks)::value_type);
	auto composite = std::make_unique<seq_con_info_composite> (name);
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "blocks", count, sizeof_element }));
	return composite;
}
}

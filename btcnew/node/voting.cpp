#include <btcnew/node/node.hpp>
#include <btcnew/node/voting.hpp>

#include <chrono>

btcnew::vote_generator::vote_generator (btcnew::node & node_a) :
node (node_a),
thread ([this] () { run (); })
{
	btcnew::unique_lock<std::mutex> lock (mutex);
	condition.wait (lock, [& started = started] { return started; });
}

void btcnew::vote_generator::add (btcnew::block_hash const & hash_a)
{
	btcnew::unique_lock<std::mutex> lock (mutex);
	hashes.push_back (hash_a);
	if (hashes.size () >= 12)
	{
		lock.unlock ();
		condition.notify_all ();
	}
}

void btcnew::vote_generator::stop ()
{
	btcnew::unique_lock<std::mutex> lock (mutex);
	stopped = true;

	lock.unlock ();
	condition.notify_all ();

	if (thread.joinable ())
	{
		thread.join ();
	}
}

void btcnew::vote_generator::send (btcnew::unique_lock<std::mutex> & lock_a)
{
	std::vector<btcnew::block_hash> hashes_l;
	hashes_l.reserve (12);
	while (!hashes.empty () && hashes_l.size () < 12)
	{
		hashes_l.push_back (hashes.front ());
		hashes.pop_front ();
	}
	lock_a.unlock ();
	{
		auto transaction (node.store.tx_begin_read ());
		node.wallets.foreach_representative ([this, &hashes_l, &transaction] (btcnew::public_key const & pub_a, btcnew::raw_key const & prv_a) {
			auto vote (this->node.store.vote_generate (transaction, pub_a, prv_a, hashes_l));
			this->node.vote_processor.vote (vote, std::make_shared<btcnew::transport::channel_udp> (this->node.network.udp_channels, this->node.network.endpoint (), this->node.network_params.protocol.protocol_version));
			this->node.votes_cache.add (vote);
		});
	}
	lock_a.lock ();
}

void btcnew::vote_generator::run ()
{
	btcnew::thread_role::set (btcnew::thread_role::name::voting);
	btcnew::unique_lock<std::mutex> lock (mutex);
	started = true;
	lock.unlock ();
	condition.notify_all ();
	lock.lock ();
	while (!stopped)
	{
		if (hashes.size () >= 12)
		{
			send (lock);
		}
		else
		{
			condition.wait_for (lock, node.config.vote_generator_delay, [this] () { return this->hashes.size () >= 12; });
			if (hashes.size () >= node.config.vote_generator_threshold && hashes.size () < 12)
			{
				condition.wait_for (lock, node.config.vote_generator_delay, [this] () { return this->hashes.size () >= 12; });
			}
			if (!hashes.empty ())
			{
				send (lock);
			}
		}
	}
}

void btcnew::votes_cache::add (std::shared_ptr<btcnew::vote> const & vote_a)
{
	btcnew::lock_guard<std::mutex> lock (cache_mutex);
	for (auto & block : vote_a->blocks)
	{
		auto hash (boost::get<btcnew::block_hash> (block));
		auto existing (cache.get<1> ().find (hash));
		if (existing == cache.get<1> ().end ())
		{
			// Clean old votes
			if (cache.size () >= network_params.voting.max_cache)
			{
				cache.erase (cache.begin ());
			}
			// Insert new votes (new hash)
			auto inserted (cache.insert (btcnew::cached_votes{ std::chrono::steady_clock::now (), hash, std::vector<std::shared_ptr<btcnew::vote>> (1, vote_a) }));
			(void)inserted;
			assert (inserted.second);
		}
		else
		{
			// Insert new votes (old hash)
			cache.get<1> ().modify (existing, [vote_a] (btcnew::cached_votes & cache_a) {
				// Replace old vote for same representative & hash
				bool replaced (false);
				for (auto i (cache_a.votes.begin ()), n (cache_a.votes.end ()); i != n && !replaced; ++i)
				{
					if ((*i)->account == vote_a->account)
					{
						*i = vote_a;
						replaced = true;
					}
				}
				// Insert new vote
				if (!replaced)
				{
					cache_a.votes.push_back (vote_a);
				}
			});
		}
	}
}

std::vector<std::shared_ptr<btcnew::vote>> btcnew::votes_cache::find (btcnew::block_hash const & hash_a)
{
	std::vector<std::shared_ptr<btcnew::vote>> result;
	btcnew::lock_guard<std::mutex> lock (cache_mutex);
	auto existing (cache.get<1> ().find (hash_a));
	if (existing != cache.get<1> ().end ())
	{
		result = existing->votes;
	}
	return result;
}

void btcnew::votes_cache::remove (btcnew::block_hash const & hash_a)
{
	btcnew::lock_guard<std::mutex> lock (cache_mutex);
	cache.get<1> ().erase (hash_a);
}

namespace btcnew
{
std::unique_ptr<seq_con_info_component> collect_seq_con_info (vote_generator & vote_generator, const std::string & name)
{
	size_t hashes_count = 0;

	{
		btcnew::lock_guard<std::mutex> guard (vote_generator.mutex);
		hashes_count = vote_generator.hashes.size ();
	}
	auto sizeof_element = sizeof (decltype (vote_generator.hashes)::value_type);
	auto composite = std::make_unique<seq_con_info_composite> (name);
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "state_blocks", hashes_count, sizeof_element }));
	return composite;
}

std::unique_ptr<seq_con_info_component> collect_seq_con_info (votes_cache & votes_cache, const std::string & name)
{
	size_t cache_count = 0;

	{
		btcnew::lock_guard<std::mutex> guard (votes_cache.cache_mutex);
		cache_count = votes_cache.cache.size ();
	}
	auto sizeof_element = sizeof (decltype (votes_cache.cache)::value_type);
	auto composite = std::make_unique<seq_con_info_composite> (name);
	/* This does not currently loop over each element inside the cache to get the sizes of the votes inside cached_votes */
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "cache", cache_count, sizeof_element }));
	return composite;
}
}

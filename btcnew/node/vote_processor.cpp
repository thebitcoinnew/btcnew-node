#include <btcnew/lib/timer.hpp>
#include <btcnew/node/node.hpp>
#include <btcnew/node/vote_processor.hpp>

btcnew::vote_processor::vote_processor (btcnew::node & node_a) :
node (node_a),
started (false),
stopped (false),
active (false),
thread ([this] () {
	btcnew::thread_role::set (btcnew::thread_role::name::vote_processing);
	process_loop ();
})
{
	btcnew::unique_lock<std::mutex> lock (mutex);
	condition.wait (lock, [& started = started] { return started; });
}

void btcnew::vote_processor::process_loop ()
{
	btcnew::timer<std::chrono::milliseconds> elapsed;
	bool log_this_iteration;

	btcnew::unique_lock<std::mutex> lock (mutex);
	started = true;

	lock.unlock ();
	condition.notify_all ();
	lock.lock ();

	while (!stopped)
	{
		if (!votes.empty ())
		{
			std::deque<std::pair<std::shared_ptr<btcnew::vote>, std::shared_ptr<btcnew::transport::channel>>> votes_l;
			votes_l.swap (votes);

			log_this_iteration = false;
			if (node.config.logging.network_logging () && votes_l.size () > 50)
			{
				/*
				 * Only log the timing information for this iteration if
				 * there are a sufficient number of items for it to be relevant
				 */
				log_this_iteration = true;
				elapsed.restart ();
			}
			active = true;
			lock.unlock ();
			verify_votes (votes_l);
			{
				btcnew::unique_lock<std::mutex> active_single_lock (node.active.mutex);
				auto transaction (node.store.tx_begin_read ());
				uint64_t count (1);
				for (auto & i : votes_l)
				{
					vote_blocking (transaction, i.first, i.second, true);
					// Free active_transactions mutex each 100 processed votes
					if (count % 100 == 0)
					{
						active_single_lock.unlock ();
						transaction.refresh ();
						active_single_lock.lock ();
					}
					count++;
				}
			}
			lock.lock ();
			active = false;

			lock.unlock ();
			condition.notify_all ();
			lock.lock ();

			if (log_this_iteration && elapsed.stop () > std::chrono::milliseconds (100))
			{
				node.logger.try_log (boost::str (boost::format ("Processed %1% votes in %2% milliseconds (rate of %3% votes per second)") % votes_l.size () % elapsed.value ().count () % ((votes_l.size () * 1000ULL) / elapsed.value ().count ())));
			}
		}
		else
		{
			condition.wait (lock);
		}
	}
}

void btcnew::vote_processor::vote (std::shared_ptr<btcnew::vote> vote_a, std::shared_ptr<btcnew::transport::channel> channel_a)
{
	btcnew::unique_lock<std::mutex> lock (mutex);
	if (!stopped)
	{
		bool process (false);
		/* Random early delection levels
		 Always process votes for test network (process = true)
		 Stop processing with max 144 * 1024 votes */
		if (!node.network_params.network.is_test_network ())
		{
			// Level 0 (< 0.1%)
			if (votes.size () < 96 * 1024)
			{
				process = true;
			}
			// Level 1 (0.1-1%)
			else if (votes.size () < 112 * 1024)
			{
				process = (representatives_1.find (vote_a->account) != representatives_1.end ());
			}
			// Level 2 (1-5%)
			else if (votes.size () < 128 * 1024)
			{
				process = (representatives_2.find (vote_a->account) != representatives_2.end ());
			}
			// Level 3 (> 5%)
			else if (votes.size () < 144 * 1024)
			{
				process = (representatives_3.find (vote_a->account) != representatives_3.end ());
			}
		}
		else
		{
			// Process for test network
			process = true;
		}
		if (process)
		{
			votes.push_back (std::make_pair (vote_a, channel_a));

			lock.unlock ();
			condition.notify_all ();
			lock.lock ();
		}
		else
		{
			node.stats.inc (btcnew::stat::type::vote, btcnew::stat::detail::vote_overflow);
		}
	}
}

void btcnew::vote_processor::verify_votes (std::deque<std::pair<std::shared_ptr<btcnew::vote>, std::shared_ptr<btcnew::transport::channel>>> & votes_a)
{
	auto size (votes_a.size ());
	std::vector<unsigned char const *> messages;
	messages.reserve (size);
	std::vector<btcnew::block_hash> hashes;
	hashes.reserve (size);
	std::vector<size_t> lengths (size, sizeof (btcnew::block_hash));
	std::vector<unsigned char const *> pub_keys;
	pub_keys.reserve (size);
	std::vector<unsigned char const *> signatures;
	signatures.reserve (size);
	std::vector<int> verifications;
	verifications.resize (size);
	for (auto & vote : votes_a)
	{
		hashes.push_back (vote.first->hash ());
		messages.push_back (hashes.back ().bytes.data ());
		pub_keys.push_back (vote.first->account.bytes.data ());
		signatures.push_back (vote.first->signature.bytes.data ());
	}
	btcnew::signature_check_set check = { size, messages.data (), lengths.data (), pub_keys.data (), signatures.data (), verifications.data () };
	node.checker.verify (check);
	std::remove_reference_t<decltype (votes_a)> result;
	auto i (0);
	for (auto & vote : votes_a)
	{
		assert (verifications[i] == 1 || verifications[i] == 0);
		if (verifications[i] == 1)
		{
			result.push_back (vote);
		}
		++i;
	}
	votes_a.swap (result);
}

// node.active.mutex lock required
btcnew::vote_code btcnew::vote_processor::vote_blocking (btcnew::transaction const & transaction_a, std::shared_ptr<btcnew::vote> vote_a, std::shared_ptr<btcnew::transport::channel> channel_a, bool validated)
{
	assert (!node.active.mutex.try_lock ());
	auto result (btcnew::vote_code::invalid);
	if (validated || !vote_a->validate ())
	{
		auto max_vote (node.store.vote_max (transaction_a, vote_a));
		result = btcnew::vote_code::replay;
		if (!node.active.vote (vote_a, true))
		{
			result = btcnew::vote_code::vote;
		}
		switch (result)
		{
			case btcnew::vote_code::vote:
				node.observers.vote.notify (vote_a, channel_a);
			case btcnew::vote_code::replay:
				// This tries to assist rep nodes that have lost track of their highest sequence number by replaying our highest known vote back to them
				// Only do this if the sequence number is significantly different to account for network reordering
				// Amplify attack considerations: We're sending out a confirm_ack in response to a confirm_ack for no net traffic increase
				if (max_vote->sequence > vote_a->sequence + 10000)
				{
					btcnew::confirm_ack confirm (max_vote);
					channel_a->send (confirm); // this is non essential traffic as it will be resolicited if not received
				}
				break;
			case btcnew::vote_code::invalid:
				assert (false);
				break;
		}
	}
	std::string status;
	switch (result)
	{
		case btcnew::vote_code::invalid:
			status = "Invalid";
			node.stats.inc (btcnew::stat::type::vote, btcnew::stat::detail::vote_invalid);
			break;
		case btcnew::vote_code::replay:
			status = "Replay";
			node.stats.inc (btcnew::stat::type::vote, btcnew::stat::detail::vote_replay);
			break;
		case btcnew::vote_code::vote:
			status = "Vote";
			node.stats.inc (btcnew::stat::type::vote, btcnew::stat::detail::vote_valid);
			break;
	}
	if (node.config.logging.vote_logging ())
	{
		node.logger.try_log (boost::str (boost::format ("Vote from: %1% sequence: %2% block(s): %3%status: %4%") % vote_a->account.to_account () % std::to_string (vote_a->sequence) % vote_a->hashes_string () % status));
	}
	return result;
}

void btcnew::vote_processor::stop ()
{
	{
		btcnew::lock_guard<std::mutex> lock (mutex);
		stopped = true;
	}
	condition.notify_all ();
	if (thread.joinable ())
	{
		thread.join ();
	}
}

void btcnew::vote_processor::flush ()
{
	btcnew::unique_lock<std::mutex> lock (mutex);
	while (active || !votes.empty ())
	{
		condition.wait (lock);
	}
}

void btcnew::vote_processor::calculate_weights ()
{
	btcnew::unique_lock<std::mutex> lock (mutex);
	if (!stopped)
	{
		representatives_1.clear ();
		representatives_2.clear ();
		representatives_3.clear ();
		auto supply (node.online_reps.online_stake ());
		auto rep_amounts = node.ledger.rep_weights.get_rep_amounts ();
		for (auto const & rep_amount : rep_amounts)
		{
			btcnew::account const & representative (rep_amount.first);
			auto weight (node.ledger.weight (representative));
			if (weight > supply / 1000) // 0.1% or above (level 1)
			{
				representatives_1.insert (representative);
				if (weight > supply / 100) // 1% or above (level 2)
				{
					representatives_2.insert (representative);
					if (weight > supply / 20) // 5% or above (level 3)
					{
						representatives_3.insert (representative);
					}
				}
			}
		}
	}
}

namespace btcnew
{
std::unique_ptr<seq_con_info_component> collect_seq_con_info (vote_processor & vote_processor, const std::string & name)
{
	size_t votes_count = 0;
	size_t representatives_1_count = 0;
	size_t representatives_2_count = 0;
	size_t representatives_3_count = 0;

	{
		btcnew::lock_guard<std::mutex> guard (vote_processor.mutex);
		votes_count = vote_processor.votes.size ();
		representatives_1_count = vote_processor.representatives_1.size ();
		representatives_2_count = vote_processor.representatives_2.size ();
		representatives_3_count = vote_processor.representatives_3.size ();
	}

	auto composite = std::make_unique<seq_con_info_composite> (name);
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "votes", votes_count, sizeof (decltype (vote_processor.votes)::value_type) }));
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "representatives_1", representatives_1_count, sizeof (decltype (vote_processor.representatives_1)::value_type) }));
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "representatives_2", representatives_2_count, sizeof (decltype (vote_processor.representatives_2)::value_type) }));
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "representatives_3", representatives_3_count, sizeof (decltype (vote_processor.representatives_3)::value_type) }));
	return composite;
}
}

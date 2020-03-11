#include <btcnew/node/json_payment_observer.hpp>
#include <btcnew/node/payment_observer_processor.hpp>

btcnew::payment_observer_processor::payment_observer_processor (btcnew::node_observers::blocks_t & blocks)
{
	blocks.add ([this] (btcnew::election_status const &, btcnew::account const & account_a, btcnew::uint128_t const &, bool) {
		observer_action (account_a);
	});
}

void btcnew::payment_observer_processor::observer_action (btcnew::account const & account_a)
{
	std::shared_ptr<btcnew::json_payment_observer> observer;
	{
		btcnew::lock_guard<std::mutex> lock (mutex);
		auto existing (payment_observers.find (account_a));
		if (existing != payment_observers.end ())
		{
			observer = existing->second;
		}
	}
	if (observer != nullptr)
	{
		observer->observe ();
	}
}

void btcnew::payment_observer_processor::add (btcnew::account const & account_a, std::shared_ptr<btcnew::json_payment_observer> payment_observer_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	assert (payment_observers.find (account_a) == payment_observers.end ());
	payment_observers[account_a] = payment_observer_a;
}

void btcnew::payment_observer_processor::erase (btcnew::account & account_a)
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	assert (payment_observers.find (account_a) != payment_observers.end ());
	payment_observers.erase (account_a);
}

#pragma once

#include <btcnew/node/node_observers.hpp>

namespace btcnew
{
class json_payment_observer;

class payment_observer_processor final
{
public:
	explicit payment_observer_processor (btcnew::node_observers::blocks_t & blocks);
	void observer_action (btcnew::account const & account_a);
	void add (btcnew::account const & account_a, std::shared_ptr<btcnew::json_payment_observer> payment_observer_a);
	void erase (btcnew::account & account_a);

private:
	std::mutex mutex;
	std::unordered_map<btcnew::account, std::shared_ptr<btcnew::json_payment_observer>> payment_observers;
};
}

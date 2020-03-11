#pragma once

#include <btcnew/node/node_observers.hpp>
#include <btcnew/node/wallet.hpp>

#include <string>
#include <vector>

namespace btcnew
{
class node;

enum class payment_status
{
	not_a_status,
	unknown,
	nothing, // Timeout and nothing was received
	//insufficient, // Timeout and not enough was received
	//over, // More than requested received
	//success_fork, // Amount received but it involved a fork
	success // Amount received
};
class json_payment_observer final : public std::enable_shared_from_this<btcnew::json_payment_observer>
{
public:
	json_payment_observer (btcnew::node &, std::function<void (std::string const &)> const &, btcnew::account const &, btcnew::amount const &);
	void start (uint64_t);
	void observe ();
	void complete (btcnew::payment_status);
	std::mutex mutex;
	btcnew::condition_variable condition;
	btcnew::node & node;
	btcnew::account account;
	btcnew::amount amount;
	std::function<void (std::string const &)> response;
	std::atomic_flag completed;
};
}

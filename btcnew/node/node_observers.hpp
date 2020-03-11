#pragma once

#include <btcnew/lib/blocks.hpp>
#include <btcnew/lib/numbers.hpp>
#include <btcnew/lib/utility.hpp>
#include <btcnew/node/active_transactions.hpp>
#include <btcnew/node/transport/transport.hpp>
#include <btcnew/secure/blockstore.hpp>

namespace btcnew
{
class node_observers final
{
public:
	using blocks_t = btcnew::observer_set<btcnew::election_status const &, btcnew::account const &, btcnew::uint128_t const &, bool>;
	blocks_t blocks;
	btcnew::observer_set<bool> wallet;
	btcnew::observer_set<std::shared_ptr<btcnew::vote>, std::shared_ptr<btcnew::transport::channel>> vote;
	btcnew::observer_set<btcnew::block_hash const &> active_stopped;
	btcnew::observer_set<btcnew::account const &, bool> account_balance;
	btcnew::observer_set<std::shared_ptr<btcnew::transport::channel>> endpoint;
	btcnew::observer_set<> disconnect;
	btcnew::observer_set<uint64_t> difficulty;
	btcnew::observer_set<btcnew::root const &> work_cancel;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (node_observers & node_observers, const std::string & name);
}

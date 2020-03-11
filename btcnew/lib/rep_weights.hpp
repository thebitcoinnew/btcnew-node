#pragma once

#include <btcnew/lib/numbers.hpp>
#include <btcnew/lib/utility.hpp>

#include <memory>
#include <mutex>
#include <unordered_map>

namespace btcnew
{
class block_store;
class transaction;

class rep_weights
{
public:
	void representation_add (btcnew::account const & source_a, btcnew::uint128_t const & amount_a);
	btcnew::uint128_t representation_get (btcnew::account const & account_a);
	void representation_put (btcnew::account const & account_a, btcnew::uint128_union const & representation_a);
	std::unordered_map<btcnew::account, btcnew::uint128_t> get_rep_amounts ();

private:
	std::mutex mutex;
	std::unordered_map<btcnew::account, btcnew::uint128_t> rep_amounts;
	void put (btcnew::account const & account_a, btcnew::uint128_union const & representation_a);
	btcnew::uint128_t get (btcnew::account const & account_a);

	friend std::unique_ptr<seq_con_info_component> collect_seq_con_info (rep_weights &, const std::string &);
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (rep_weights &, const std::string &);
}

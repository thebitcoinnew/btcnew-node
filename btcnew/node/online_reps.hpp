#pragma once

#include <btcnew/lib/numbers.hpp>
#include <btcnew/lib/utility.hpp>

#include <memory>
#include <unordered_set>
#include <vector>

namespace btcnew
{
class node;
class transaction;

/** Track online representatives and trend online weight */
class online_reps final
{
public:
	online_reps (btcnew::node &, btcnew::uint128_t);

	/** Add voting account \p rep_account to the set of online representatives */
	void observe (btcnew::account const & rep_account);
	/** Called periodically to sample online weight */
	void sample ();
	/** Returns the trended online stake, but never less than configured minimum */
	btcnew::uint128_t online_stake () const;
	/** List of online representatives */
	std::vector<btcnew::account> list ();

private:
	btcnew::uint128_t trend (btcnew::transaction &);
	mutable std::mutex mutex;
	btcnew::node & node;
	std::unordered_set<btcnew::account> reps;
	btcnew::uint128_t online;
	btcnew::uint128_t minimum;

	friend std::unique_ptr<seq_con_info_component> collect_seq_con_info (online_reps & online_reps, const std::string & name);
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (online_reps & online_reps, const std::string & name);
}

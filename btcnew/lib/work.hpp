#pragma once

#include <btcnew/lib/config.hpp>
#include <btcnew/lib/numbers.hpp>
#include <btcnew/lib/utility.hpp>

#include <boost/optional.hpp>
#include <boost/thread/thread.hpp>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <thread>

namespace btcnew
{
class block;
bool work_validate (btcnew::root const &, uint64_t, uint64_t * = nullptr);
bool work_validate (btcnew::block const &, uint64_t * = nullptr);
uint64_t work_value (btcnew::root const &, uint64_t);
class opencl_work;
class work_item final
{
public:
	work_item (btcnew::root const & item_a, std::function<void (boost::optional<uint64_t> const &)> const & callback_a, uint64_t difficulty_a) :
	item (item_a), callback (callback_a), difficulty (difficulty_a)
	{
	}

	btcnew::root item;
	std::function<void (boost::optional<uint64_t> const &)> callback;
	uint64_t difficulty;
};
class work_pool final
{
public:
	work_pool (unsigned, std::chrono::nanoseconds = std::chrono::nanoseconds (0), std::function<boost::optional<uint64_t> (btcnew::root const &, uint64_t, std::atomic<int> &)> = nullptr);
	~work_pool ();
	void loop (uint64_t);
	void stop ();
	void cancel (btcnew::root const &);
	void generate (btcnew::root const &, std::function<void (boost::optional<uint64_t> const &)>);
	void generate (btcnew::root const &, std::function<void (boost::optional<uint64_t> const &)>, uint64_t);
	boost::optional<uint64_t> generate (btcnew::root const &);
	boost::optional<uint64_t> generate (btcnew::root const &, uint64_t);
	size_t size ();
	btcnew::network_constants network_constants;
	std::atomic<int> ticket;
	bool done;
	std::vector<boost::thread> threads;
	std::list<btcnew::work_item> pending;
	std::mutex mutex;
	btcnew::condition_variable producer_condition;
	std::chrono::nanoseconds pow_rate_limiter;
	std::function<boost::optional<uint64_t> (btcnew::root const &, uint64_t, std::atomic<int> &)> opencl;
	btcnew::observer_set<bool> work_observers;
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (work_pool & work_pool, const std::string & name);
}

#include <btcnew/core_test/testutil.hpp>
#include <btcnew/lib/locks.hpp>

#include <gtest/gtest.h>

#include <regex>

#if BTCNEW_TIMED_LOCKS > 0
namespace
{
unsigned num_matches (std::string const & str)
{
	std::regex regexpr (R"(( \d+)ms)"); // matches things like " 12312ms"
	std::smatch matches;

	auto count = 0u;
	std::string::const_iterator search_start (str.cbegin ());
	while (std::regex_search (search_start, str.cend (), matches, regexpr))
	{
		++count;
		search_start = matches.suffix ().first;
	}
	return count;
}
}

TEST (locks, no_conflicts)
{
	std::stringstream ss;
	btcnew::cout_redirect (ss.rdbuf ());

	std::mutex guard_mutex;
	btcnew::lock_guard<std::mutex> guard (guard_mutex);

	std::mutex lk_mutex;
	btcnew::unique_lock<std::mutex> lk (lk_mutex);

	// This could fail if BTCNEW_TIMED_LOCKS is such a low value that the above mutexes are held longer than that before reaching this statement
	ASSERT_EQ (ss.str (), "");
}

TEST (locks, lock_guard)
{
	std::stringstream ss;
	btcnew::cout_redirect redirect (ss.rdbuf ());

	std::mutex mutex;

	// Depending on timing the mutex could be reached first in
	std::promise<void> promise;
	std::thread t;
	{
		t = std::thread ([&mutex, &promise] {
			btcnew::lock_guard<std::mutex> guard (mutex);
			promise.set_value ();
			// Tries to make sure that the other guard to held for a minimum of BTCNEW_TIMED_LOCKS, may need to increase this for low BTCNEW_TIMED_LOCKS values
			std::this_thread::sleep_for (std::chrono::milliseconds (BTCNEW_TIMED_LOCKS * 2));
		});
	}

	// Wait until the lock_guard has been reached in the other thread
	promise.get_future ().wait ();
	{
		btcnew::lock_guard<std::mutex> guard (mutex);
		t.join ();
	}

	// 2 mutexes held and 1 blocked
	ASSERT_EQ (num_matches (ss.str ()), 3);
}

TEST (locks, unique_lock)
{
	std::stringstream ss;
	btcnew::cout_redirect redirect (ss.rdbuf ());

	std::mutex mutex;

	// Depending on timing the mutex could be reached first in
	std::promise<void> promise;
	std::thread t ([&mutex, &promise] {
		btcnew::unique_lock<std::mutex> lk (mutex);
		std::this_thread::sleep_for (std::chrono::milliseconds (BTCNEW_TIMED_LOCKS));
		lk.unlock ();
		lk.lock ();

		promise.set_value ();
		// Tries to make sure that the other guard to held for a minimum of BTCNEW_TIMED_LOCKS, may need to increase this for low BTCNEW_TIMED_LOCKS values
		std::this_thread::sleep_for (std::chrono::milliseconds (BTCNEW_TIMED_LOCKS * 2));
	});

	// Wait until the lock_guard has been reached in the other thread
	promise.get_future ().wait ();
	{
		btcnew::unique_lock<std::mutex> lk (mutex);
		t.join ();
	}

	// 3 mutexes held and 1 blocked
	ASSERT_EQ (num_matches (ss.str ()), 4);
}

TEST (locks, condition_variable)
{
	btcnew::condition_variable cv;
	std::mutex mutex;
	std::promise<void> promise;
	std::atomic<bool> finished{ false };
	std::atomic<bool> notified{ false };
	std::thread t ([&cv, &notified, &finished] {
		while (!finished)
		{
			notified = true;
			cv.notify_one ();
		}
	});

	btcnew::unique_lock<std::mutex> lk (mutex);
	cv.wait (lk, [&notified] {
		return notified.load ();
	});

	finished = true;
	t.join ();
}
#endif

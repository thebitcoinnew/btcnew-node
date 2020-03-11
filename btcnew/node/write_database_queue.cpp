#include <btcnew/lib/utility.hpp>
#include <btcnew/node/write_database_queue.hpp>

#include <algorithm>

btcnew::write_guard::write_guard (btcnew::condition_variable & cv_a, std::function<void ()> guard_finish_callback_a) :
cv (cv_a),
guard_finish_callback (guard_finish_callback_a)
{
}

btcnew::write_guard::~write_guard ()
{
	guard_finish_callback ();
	cv.notify_all ();
}

btcnew::write_database_queue::write_database_queue () :
// clang-format off
guard_finish_callback ([&queue = queue, &mutex = mutex]() {
	btcnew::lock_guard<std::mutex> guard (mutex);
	queue.pop_front ();
})
// clang-format on
{
}

btcnew::write_guard btcnew::write_database_queue::wait (btcnew::writer writer)
{
	btcnew::unique_lock<std::mutex> lk (mutex);
	// Add writer to the end of the queue if it's not already waiting
	auto exists = std::find (queue.cbegin (), queue.cend (), writer) != queue.cend ();
	if (!exists)
	{
		queue.push_back (writer);
	}

	while (!stopped && queue.front () != writer)
	{
		cv.wait (lk);
	}

	return write_guard (cv, guard_finish_callback);
}

bool btcnew::write_database_queue::contains (btcnew::writer writer)
{
	btcnew::lock_guard<std::mutex> guard (mutex);
	return std::find (queue.cbegin (), queue.cend (), writer) != queue.cend ();
}

bool btcnew::write_database_queue::process (btcnew::writer writer)
{
	auto result = false;
	{
		btcnew::lock_guard<std::mutex> guard (mutex);
		// Add writer to the end of the queue if it's not already waiting
		auto exists = std::find (queue.cbegin (), queue.cend (), writer) != queue.cend ();
		if (!exists)
		{
			queue.push_back (writer);
		}

		result = (queue.front () == writer);
	}

	if (!result)
	{
		cv.notify_all ();
	}

	return result;
}

btcnew::write_guard btcnew::write_database_queue::pop ()
{
	return write_guard (cv, guard_finish_callback);
}

void btcnew::write_database_queue::stop ()
{
	stopped = true;
	cv.notify_all ();
}

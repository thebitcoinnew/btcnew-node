#include <btcnew/lib/alarm.hpp>
#include <btcnew/lib/utility.hpp>

bool btcnew::operation::operator> (btcnew::operation const & other_a) const
{
	return wakeup > other_a.wakeup;
}

btcnew::alarm::alarm (boost::asio::io_context & io_ctx_a) :
io_ctx (io_ctx_a),
thread ([this] () {
	btcnew::thread_role::set (btcnew::thread_role::name::alarm);
	run ();
})
{
}

btcnew::alarm::~alarm ()
{
	add (std::chrono::steady_clock::now (), nullptr);
	thread.join ();
}

void btcnew::alarm::run ()
{
	btcnew::unique_lock<std::mutex> lock (mutex);
	auto done (false);
	while (!done)
	{
		if (!operations.empty ())
		{
			auto & operation (operations.top ());
			if (operation.function)
			{
				if (operation.wakeup <= std::chrono::steady_clock::now ())
				{
					io_ctx.post (operation.function);
					operations.pop ();
				}
				else
				{
					auto wakeup (operation.wakeup);
					condition.wait_until (lock, wakeup);
				}
			}
			else
			{
				done = true;
			}
		}
		else
		{
			condition.wait (lock);
		}
	}
}

void btcnew::alarm::add (std::chrono::steady_clock::time_point const & wakeup_a, std::function<void ()> const & operation)
{
	{
		btcnew::lock_guard<std::mutex> guard (mutex);
		operations.push (btcnew::operation ({ wakeup_a, operation }));
	}
	condition.notify_all ();
}

namespace btcnew
{
std::unique_ptr<seq_con_info_component> collect_seq_con_info (alarm & alarm, const std::string & name)
{
	auto composite = std::make_unique<seq_con_info_composite> (name);
	size_t count = 0;
	{
		btcnew::lock_guard<std::mutex> guard (alarm.mutex);
		count = alarm.operations.size ();
	}
	auto sizeof_element = sizeof (decltype (alarm.operations)::value_type);
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "operations", count, sizeof_element }));
	return composite;
}
}

#pragma once

#include <btcnew/boost/asio.hpp>
#include <btcnew/lib/locks.hpp>

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread/thread.hpp>

#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace btcnew
{
/* These containers are used to collect information about sequence containers.
 * It makes use of the composite design pattern to collect information
 * from sequence containers and sequence containers inside member variables.
 */
struct seq_con_info
{
	std::string name;
	size_t count;
	size_t sizeof_element;
};

class seq_con_info_component
{
public:
	virtual ~seq_con_info_component () = default;
	virtual bool is_composite () const = 0;
};

class seq_con_info_composite : public seq_con_info_component
{
public:
	seq_con_info_composite (const std::string & name);
	bool is_composite () const override;
	void add_component (std::unique_ptr<seq_con_info_component> child);
	const std::vector<std::unique_ptr<seq_con_info_component>> & get_children () const;
	const std::string & get_name () const;

private:
	std::string name;
	std::vector<std::unique_ptr<seq_con_info_component>> children;
};

class seq_con_info_leaf : public seq_con_info_component
{
public:
	seq_con_info_leaf (const seq_con_info & info);
	bool is_composite () const override;
	const seq_con_info & get_info () const;

private:
	seq_con_info info;
};

// Lower priority of calling work generating thread
void work_thread_reprioritize ();

/*
 * Functions for managing filesystem permissions, platform specific
 */
void set_umask ();
void set_secure_perm_directory (boost::filesystem::path const & path);
void set_secure_perm_directory (boost::filesystem::path const & path, boost::system::error_code & ec);
void set_secure_perm_file (boost::filesystem::path const & path);
void set_secure_perm_file (boost::filesystem::path const & path, boost::system::error_code & ec);

/*
 * Function to check if running Windows as an administrator
 */
bool is_windows_elevated ();

/*
 * Function to check if the Windows Event log registry key exists
 */
bool event_log_reg_entry_exists ();

/*
 * Create the load memory addresses for the executable and shared libraries.
 */
void create_load_memory_address_files ();

/*
 * Dumps a stacktrace file which can be read using the --debug_output_last_backtrace_dump CLI command
 */
void dump_crash_stacktrace ();

/*
 * Generates the current stacktrace
 */
std::string generate_stacktrace ();

/*
 * Functions for understanding the role of the current thread
 */
namespace thread_role
{
	enum class name
	{
		unknown,
		io,
		work,
		packet_processing,
		alarm,
		vote_processing,
		block_processing,
		request_loop,
		wallet_actions,
		bootstrap_initiator,
		voting,
		signature_checking,
		rpc_request_processor,
		rpc_process_container,
		work_watcher,
		confirmation_height_processing,
		worker
	};
	/*
	 * Get/Set the identifier for the current thread
	 */
	btcnew::thread_role::name get ();
	void set (btcnew::thread_role::name);

	/*
	 * Get the thread name as a string from enum
	 */
	std::string get_string (btcnew::thread_role::name);

	/*
	 * Get the current thread's role as a string
	 */
	std::string get_string ();

	/*
	 * Internal only, should not be called directly
	 */
	void set_os_name (std::string const &);
}

namespace thread_attributes
{
	void set (boost::thread::attributes &);
}

class thread_runner final
{
public:
	thread_runner (boost::asio::io_context &, unsigned);
	~thread_runner ();
	/** Tells the IO context to stop processing events.*/
	void stop_event_processing ();
	/** Wait for IO threads to complete */
	void join ();
	std::vector<boost::thread> threads;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> io_guard;
};

class worker final
{
public:
	worker ();
	~worker ();
	void run ();
	void push_task (std::function<void ()> func);
	void stop ();

private:
	btcnew::condition_variable cv;
	std::deque<std::function<void ()>> queue;
	std::mutex mutex;
	bool stopped{ false };
	std::thread thread;

	friend std::unique_ptr<seq_con_info_component> collect_seq_con_info (worker &, const std::string &);
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (worker & worker, const std::string & name);

/**
 * Returns seconds passed since unix epoch (posix time)
 */
inline uint64_t seconds_since_epoch ()
{
	return std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now ().time_since_epoch ()).count ();
}

template <typename... T>
class observer_set final
{
public:
	void add (std::function<void (T...)> const & observer_a)
	{
		btcnew::lock_guard<std::mutex> lock (mutex);
		observers.push_back (observer_a);
	}
	void notify (T... args)
	{
		btcnew::lock_guard<std::mutex> lock (mutex);
		for (auto & i : observers)
		{
			i (args...);
		}
	}
	std::mutex mutex;
	std::vector<std::function<void (T...)>> observers;
};

template <typename... T>
std::unique_ptr<seq_con_info_component> collect_seq_con_info (observer_set<T...> & observer_set, const std::string & name)
{
	size_t count = 0;
	{
		btcnew::lock_guard<std::mutex> lock (observer_set.mutex);
		count = observer_set.observers.size ();
	}

	auto sizeof_element = sizeof (typename decltype (observer_set.observers)::value_type);
	auto composite = std::make_unique<seq_con_info_composite> (name);
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "observers", count, sizeof_element }));
	return composite;
}

void remove_all_files_in_dir (boost::filesystem::path const & dir);
void move_all_files_to_dir (boost::filesystem::path const & from, boost::filesystem::path const & to);
}
// Have our own async_write which we must use?

void release_assert_internal (bool check, const char * check_expr, const char * file, unsigned int line);
#define release_assert(check) release_assert_internal (check, #check, __FILE__, __LINE__)

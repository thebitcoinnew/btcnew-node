#pragma once

#include <btcnew/lib/numbers.hpp>
#include <btcnew/lib/utility.hpp>
#include <btcnew/secure/common.hpp>

#include <boost/thread/thread.hpp>

#include <deque>
#include <memory>
#include <mutex>
#include <unordered_set>

namespace btcnew
{
class node;
class transaction;
namespace transport
{
	class channel;
}

class vote_processor final
{
public:
	explicit vote_processor (btcnew::node &);
	void vote (std::shared_ptr<btcnew::vote>, std::shared_ptr<btcnew::transport::channel>);
	/** Note: node.active.mutex lock is required */
	btcnew::vote_code vote_blocking (btcnew::transaction const &, std::shared_ptr<btcnew::vote>, std::shared_ptr<btcnew::transport::channel>, bool = false);
	void verify_votes (std::deque<std::pair<std::shared_ptr<btcnew::vote>, std::shared_ptr<btcnew::transport::channel>>> &);
	void flush ();
	void calculate_weights ();
	btcnew::node & node;
	void stop ();

private:
	void process_loop ();
	std::deque<std::pair<std::shared_ptr<btcnew::vote>, std::shared_ptr<btcnew::transport::channel>>> votes;
	/** Representatives levels for random early detection */
	std::unordered_set<btcnew::account> representatives_1;
	std::unordered_set<btcnew::account> representatives_2;
	std::unordered_set<btcnew::account> representatives_3;
	btcnew::condition_variable condition;
	std::mutex mutex;
	bool started;
	bool stopped;
	bool active;
	boost::thread thread;

	friend std::unique_ptr<seq_con_info_component> collect_seq_con_info (vote_processor & vote_processor, const std::string & name);
};

std::unique_ptr<seq_con_info_component> collect_seq_con_info (vote_processor & vote_processor, const std::string & name);
}

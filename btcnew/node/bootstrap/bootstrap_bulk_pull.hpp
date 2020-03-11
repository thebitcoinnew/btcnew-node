#pragma once

#include <btcnew/node/common.hpp>
#include <btcnew/node/socket.hpp>

#include <unordered_set>

namespace btcnew
{
class pull_info
{
public:
	using count_t = btcnew::bulk_pull::count_t;
	pull_info () = default;
	pull_info (btcnew::hash_or_account const &, btcnew::block_hash const &, btcnew::block_hash const &, count_t = 0, unsigned = 16);
	btcnew::hash_or_account account_or_head{ 0 };
	btcnew::block_hash head{ 0 };
	btcnew::block_hash head_original{ 0 };
	btcnew::block_hash end{ 0 };
	count_t count{ 0 };
	unsigned attempts{ 0 };
	uint64_t processed{ 0 };
	unsigned retry_limit{ 0 };
};
class bootstrap_client;
class bulk_pull_client final : public std::enable_shared_from_this<btcnew::bulk_pull_client>
{
public:
	bulk_pull_client (std::shared_ptr<btcnew::bootstrap_client>, btcnew::pull_info const &);
	~bulk_pull_client ();
	void request ();
	void receive_block ();
	void throttled_receive_block ();
	void received_type ();
	void received_block (boost::system::error_code const &, size_t, btcnew::block_type);
	btcnew::block_hash first ();
	std::shared_ptr<btcnew::bootstrap_client> connection;
	btcnew::block_hash expected;
	btcnew::account known_account;
	btcnew::pull_info pull;
	uint64_t pull_blocks;
	uint64_t unexpected_count;
	bool network_error{ false };
};
class bulk_pull_account_client final : public std::enable_shared_from_this<btcnew::bulk_pull_account_client>
{
public:
	bulk_pull_account_client (std::shared_ptr<btcnew::bootstrap_client>, btcnew::account const &);
	~bulk_pull_account_client ();
	void request ();
	void receive_pending ();
	std::shared_ptr<btcnew::bootstrap_client> connection;
	btcnew::account account;
	uint64_t pull_blocks;
};
class bootstrap_server;
class bulk_pull;
class bulk_pull_server final : public std::enable_shared_from_this<btcnew::bulk_pull_server>
{
public:
	bulk_pull_server (std::shared_ptr<btcnew::bootstrap_server> const &, std::unique_ptr<btcnew::bulk_pull>);
	void set_current_end ();
	std::shared_ptr<btcnew::block> get_next ();
	void send_next ();
	void sent_action (boost::system::error_code const &, size_t);
	void send_finished ();
	void no_block_sent (boost::system::error_code const &, size_t);
	std::shared_ptr<btcnew::bootstrap_server> connection;
	std::unique_ptr<btcnew::bulk_pull> request;
	btcnew::block_hash current;
	bool include_start;
	btcnew::bulk_pull::count_t max_count;
	btcnew::bulk_pull::count_t sent_count;
};
class bulk_pull_account;
class bulk_pull_account_server final : public std::enable_shared_from_this<btcnew::bulk_pull_account_server>
{
public:
	bulk_pull_account_server (std::shared_ptr<btcnew::bootstrap_server> const &, std::unique_ptr<btcnew::bulk_pull_account>);
	void set_params ();
	std::pair<std::unique_ptr<btcnew::pending_key>, std::unique_ptr<btcnew::pending_info>> get_next ();
	void send_frontier ();
	void send_next_block ();
	void sent_action (boost::system::error_code const &, size_t);
	void send_finished ();
	void complete (boost::system::error_code const &, size_t);
	std::shared_ptr<btcnew::bootstrap_server> connection;
	std::unique_ptr<btcnew::bulk_pull_account> request;
	std::unordered_set<btcnew::uint256_union> deduplication;
	btcnew::pending_key current_key;
	bool pending_address_only;
	bool pending_include_address;
	bool invalid_request;
};
}

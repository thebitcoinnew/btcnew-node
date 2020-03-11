#pragma once

#include <btcnew/lib/errors.hpp>
#include <btcnew/lib/utility.hpp>
#include <btcnew/node/node.hpp>

#include <chrono>

namespace btcnew
{
/** Test-system related error codes */
enum class error_system
{
	generic = 1,
	deadline_expired
};
class system final
{
public:
	system ();
	system (uint16_t, uint16_t, btcnew::transport::transport_type = btcnew::transport::transport_type::tcp);
	~system ();
	void generate_activity (btcnew::node &, std::vector<btcnew::account> &);
	void generate_mass_activity (uint32_t, btcnew::node &);
	void generate_usage_traffic (uint32_t, uint32_t, size_t);
	void generate_usage_traffic (uint32_t, uint32_t);
	btcnew::account get_random_account (std::vector<btcnew::account> &);
	btcnew::uint128_t get_random_amount (btcnew::transaction const &, btcnew::node &, btcnew::account const &);
	void generate_rollback (btcnew::node &, std::vector<btcnew::account> &);
	void generate_change_known (btcnew::node &, std::vector<btcnew::account> &);
	void generate_change_unknown (btcnew::node &, std::vector<btcnew::account> &);
	void generate_receive (btcnew::node &);
	void generate_send_new (btcnew::node &, std::vector<btcnew::account> &);
	void generate_send_existing (btcnew::node &, std::vector<btcnew::account> &);
	std::shared_ptr<btcnew::wallet> wallet (size_t);
	btcnew::account account (btcnew::transaction const &, size_t);
	/**
	 * Polls, sleep if there's no work to be done (default 50ms), then check the deadline
	 * @returns 0 or btcnew::deadline_expired
	 */
	std::error_code poll (const std::chrono::nanoseconds & sleep_time = std::chrono::milliseconds (50));
	void stop ();
	void deadline_set (const std::chrono::duration<double, std::nano> & delta);
	std::shared_ptr<btcnew::node> add_node (btcnew::node_config const &, btcnew::node_flags = btcnew::node_flags (), btcnew::transport::transport_type = btcnew::transport::transport_type::tcp);
	boost::asio::io_context io_ctx;
	btcnew::alarm alarm{ io_ctx };
	std::vector<std::shared_ptr<btcnew::node>> nodes;
	btcnew::logging logging;
	btcnew::work_pool work{ 1 };
	std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<double>> deadline{ std::chrono::steady_clock::time_point::max () };
	double deadline_scaling_factor{ 1.0 };
};
}
REGISTER_ERROR_CODES (btcnew, error_system);

#pragma once

#include <btcnew/lib/errors.hpp>
#include <btcnew/lib/numbers.hpp>

#include <boost/filesystem.hpp>

#include <array>
#include <chrono>
#include <string>

#define xstr(a) ver_str (a)
#define ver_str(a) #a

/**
* Returns build version information
*/
static const char * BTCNEW_VERSION_STRING = xstr (TAG_VERSION_STRING);

static const char * BUILD_INFO = xstr (GIT_COMMIT_HASH BOOST_COMPILER) " \"BOOST " xstr (BOOST_VERSION) "\" BUILT " xstr (__DATE__);

/** Is TSAN/ASAN test build */
#if defined(__has_feature)
#if __has_feature(thread_sanitizer) || __has_feature(address_sanitizer)
static const bool is_sanitizer_build = true;
#else
static const bool is_sanitizer_build = false;
#endif
#else
static const bool is_sanitizer_build = false;
#endif

namespace btcnew
{
/**
 * Network variants with different genesis blocks and network parameters
 * @warning Enum values are used in integral comparisons; do not change.
 */
enum class btcnew_networks
{
	// Low work parameters, publicly known genesis key, test IP ports
	btcnew_test_network = 0,
	rai_test_network = 0,
	// Normal work parameters, secret beta genesis key, beta IP ports
	btcnew_beta_network = 1,
	rai_beta_network = 1,
	// Normal work parameters, secret live key, live IP ports
	btcnew_live_network = 2,
	rai_live_network = 2,
};

class network_constants
{
public:
	network_constants () :
	network_constants (network_constants::active_network)
	{
	}

	network_constants (btcnew_networks network_a) :
	current_network (network_a)
	{
		publish_threshold = is_test_network () ? publish_test_threshold : is_beta_network () ? publish_beta_threshold : publish_full_threshold;

		// A representative is classified as principal based on its weight and this factor
		principal_weight_factor = 1000; // 0.1%

		default_node_port = is_live_network () ? 9075 : is_beta_network () ? 51000 : 41000;
		default_rpc_port = is_live_network () ? 9076 : is_beta_network () ? 52000 : 42000;
		default_ipc_port = is_live_network () ? 9077 : is_beta_network () ? 53000 : 43000;
		default_websocket_port = is_live_network () ? 9078 : is_beta_network () ? 54000 : 44000;
		request_interval_ms = is_test_network () ? (is_sanitizer_build ? 100 : 20) : 500;
	}

	/** Network work thresholds. ~5 seconds of work for the live network */
	static uint64_t const publish_full_threshold{ 0xffffffc000000000 };
	static uint64_t const publish_beta_threshold{ 0xfffffc0000000000 }; // 16x lower than full
	static uint64_t const publish_test_threshold{ 0xff00000000000000 }; // very low for tests

	/** The network this param object represents. This may differ from the global active network; this is needed for certain --debug... commands */
	btcnew_networks current_network;
	uint64_t publish_threshold;
	unsigned principal_weight_factor;
	uint16_t default_node_port;
	uint16_t default_rpc_port;
	uint16_t default_ipc_port;
	uint16_t default_websocket_port;
	unsigned request_interval_ms;

	/** Returns the network this object contains values for */
	btcnew_networks network () const
	{
		return current_network;
	}

	/**
	 * Optionally called on startup to override the global active network.
	 * If not called, the compile-time option will be used.
	 * @param network_a The new active network
	 */
	static void set_active_network (btcnew_networks network_a)
	{
		active_network = network_a;
	}

	/**
	 * Optionally called on startup to override the global active network.
	 * If not called, the compile-time option will be used.
	 * @param network_a The new active network. Valid values are "live", "beta" and "test"
	 */
	static btcnew::error set_active_network (std::string network_a)
	{
		btcnew::error err;
		if (network_a == "live")
		{
			active_network = btcnew::btcnew_networks::btcnew_live_network;
		}
		else if (network_a == "beta")
		{
			active_network = btcnew::btcnew_networks::btcnew_beta_network;
		}
		else if (network_a == "test")
		{
			active_network = btcnew::btcnew_networks::btcnew_test_network;
		}
		else
		{
			err = "Invalid network. Valid values are live, beta and test.";
		}
		return err;
	}

	const char * get_current_network_as_string () const
	{
		return is_live_network () ? "live" : is_beta_network () ? "beta" : "test";
	}

	bool is_live_network () const
	{
		return current_network == btcnew_networks::btcnew_live_network;
	}
	bool is_beta_network () const
	{
		return current_network == btcnew_networks::btcnew_beta_network;
	}
	bool is_test_network () const
	{
		return current_network == btcnew_networks::btcnew_test_network;
	}

	/** Initial value is ACTIVE_NETWORK compile flag, but can be overridden by a CLI flag */
	static btcnew::btcnew_networks active_network;
};

inline boost::filesystem::path get_config_path (boost::filesystem::path const & data_path)
{
	return data_path / "config.json";
}

inline boost::filesystem::path get_rpc_config_path (boost::filesystem::path const & data_path)
{
	return data_path / "rpc_config.json";
}

inline boost::filesystem::path get_node_toml_config_path (boost::filesystem::path const & data_path)
{
	return data_path / "config-node.toml";
}

inline boost::filesystem::path get_rpc_toml_config_path (boost::filesystem::path const & data_path)
{
	return data_path / "config-rpc.toml";
}

inline boost::filesystem::path get_qtwallet_toml_config_path (boost::filesystem::path const & data_path)
{
	return data_path / "config-qtwallet.toml";
}

/** Called by gtest_main to enforce test network */
void force_btcnew_test_network ();

/** Checks if we are running inside a valgrind instance */
bool running_within_valgrind ();
}

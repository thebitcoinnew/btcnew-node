#pragma once

#include <btcnew/boost/asio.hpp>
#include <btcnew/lib/config.hpp>
#include <btcnew/lib/errors.hpp>

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

#include <string>
#include <vector>

namespace btcnew
{
class jsonconfig;
class tomlconfig;

/** Configuration options for RPC TLS */
class rpc_secure_config final
{
public:
	btcnew::error serialize_json (btcnew::jsonconfig &) const;
	btcnew::error deserialize_json (btcnew::jsonconfig &);
	btcnew::error serialize_toml (btcnew::tomlconfig &) const;
	btcnew::error deserialize_toml (btcnew::tomlconfig &);

	/** If true, enable TLS */
	bool enable{ false };
	/** If true, log certificate verification details */
	bool verbose_logging{ false };
	/** Must be set if the private key PEM is password protected */
	std::string server_key_passphrase;
	/** Path to certificate- or chain file. Must be PEM formatted. */
	std::string server_cert_path;
	/** Path to private key file. Must be PEM formatted.*/
	std::string server_key_path;
	/** Path to dhparam file */
	std::string server_dh_path;
	/** Optional path to directory containing client certificates */
	std::string client_certs_path;
};

class rpc_process_config final
{
public:
	btcnew::network_constants network_constants;
	unsigned io_threads{ std::max<unsigned> (4, boost::thread::hardware_concurrency ()) };
	boost::asio::ip::address_v6 ipc_address{ boost::asio::ip::address_v6::loopback () };
	uint16_t ipc_port{ network_constants.default_ipc_port };
	unsigned num_ipc_connections{ network_constants.is_live_network () ? 8u : network_constants.is_beta_network () ? 4u : 1u };
	static unsigned json_version ()
	{
		return 1;
	}
};

class rpc_config final
{
public:
	explicit rpc_config (bool = false);
	btcnew::error serialize_json (btcnew::jsonconfig &) const;
	btcnew::error deserialize_json (bool & upgraded_a, btcnew::jsonconfig &);
	btcnew::error serialize_toml (btcnew::tomlconfig &) const;
	btcnew::error deserialize_toml (btcnew::tomlconfig &);

	btcnew::rpc_process_config rpc_process;
	boost::asio::ip::address_v6 address{ boost::asio::ip::address_v6::loopback () };
	uint16_t port{ rpc_process.network_constants.default_rpc_port };
	bool enable_control;
	rpc_secure_config secure;
	uint8_t max_json_depth{ 20 };
	uint64_t max_request_size{ 32 * 1024 * 1024 };
	static unsigned json_version ()
	{
		return 1;
	}
};

btcnew::error read_rpc_config_toml (boost::filesystem::path const & data_path_a, btcnew::rpc_config & config_a, std::vector<std::string> const & config_overrides = std::vector<std::string> ());
btcnew::error read_and_update_rpc_config (boost::filesystem::path const & data_path, btcnew::rpc_config & config_a);

std::string get_default_rpc_filepath ();
}

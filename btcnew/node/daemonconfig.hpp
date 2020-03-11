#pragma once

#include <btcnew/lib/errors.hpp>
#include <btcnew/node/node_pow_server_config.hpp>
#include <btcnew/node/node_rpc_config.hpp>
#include <btcnew/node/nodeconfig.hpp>
#include <btcnew/node/openclconfig.hpp>

#include <vector>

namespace btcnew
{
class jsonconfig;
class tomlconfig;
class daemon_config
{
public:
	daemon_config () = default;
	daemon_config (boost::filesystem::path const & data_path);
	btcnew::error deserialize_json (bool &, btcnew::jsonconfig &);
	btcnew::error serialize_json (btcnew::jsonconfig &);
	btcnew::error deserialize_toml (btcnew::tomlconfig &);
	btcnew::error serialize_toml (btcnew::tomlconfig &);
	bool rpc_enable{ false };
	btcnew::node_rpc_config rpc;
	btcnew::node_config node;
	bool opencl_enable{ false };
	btcnew::opencl_config opencl;
	btcnew::node_pow_server_config pow_server;
	boost::filesystem::path data_path;
	unsigned json_version () const
	{
		return 2;
	}
};

btcnew::error read_node_config_toml (boost::filesystem::path const &, btcnew::daemon_config & config_a, std::vector<std::string> const & config_overrides = std::vector<std::string> ());
btcnew::error read_and_update_daemon_config (boost::filesystem::path const &, btcnew::daemon_config & config_a, btcnew::jsonconfig & json_a);
}

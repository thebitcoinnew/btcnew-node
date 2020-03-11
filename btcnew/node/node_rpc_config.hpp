#pragma once

#include <btcnew/lib/rpcconfig.hpp>

#include <boost/filesystem.hpp>

#include <string>

namespace btcnew
{
class tomlconfig;
class rpc_child_process_config final
{
public:
	bool enable{ false };
	std::string rpc_path{ get_default_rpc_filepath () };
};

class node_rpc_config final
{
public:
	btcnew::error serialize_json (btcnew::jsonconfig &) const;
	btcnew::error deserialize_json (bool & upgraded_a, btcnew::jsonconfig &, boost::filesystem::path const & data_path);
	btcnew::error serialize_toml (btcnew::tomlconfig & toml) const;
	btcnew::error deserialize_toml (btcnew::tomlconfig & toml);

	bool enable_sign_hash{ false };
	btcnew::rpc_child_process_config child_process;
	static unsigned json_version ()
	{
		return 1;
	}

private:
	void migrate (btcnew::jsonconfig & json, boost::filesystem::path const & data_path);
};
}

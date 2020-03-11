#include <btcnew/lib/config.hpp>
#include <btcnew/lib/rpcconfig.hpp>
#include <btcnew/lib/tomlconfig.hpp>
#include <btcnew/node/node_pow_server_config.hpp>

btcnew::error btcnew::node_pow_server_config::serialize_toml (btcnew::tomlconfig & toml) const
{
	toml.put ("enable", enable, "Enable or disable starting Bitcoin New PoW Server as a child process.\ntype:bool");
	toml.put ("btcnew_pow_server_path", pow_server_path, "Path to the btcnew_pow_server executable.\ntype:string,path");
	return toml.get_error ();
}

btcnew::error btcnew::node_pow_server_config::deserialize_toml (btcnew::tomlconfig & toml)
{
	toml.get_optional<bool> ("enable", enable);
	toml.get_optional<std::string> ("btcnew_pow_server_path", pow_server_path);

	return toml.get_error ();
}

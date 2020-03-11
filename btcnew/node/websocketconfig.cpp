#include <btcnew/lib/jsonconfig.hpp>
#include <btcnew/lib/tomlconfig.hpp>
#include <btcnew/node/websocketconfig.hpp>

btcnew::websocket::config::config () :
port (network_constants.default_websocket_port)
{
}

btcnew::error btcnew::websocket::config::serialize_toml (btcnew::tomlconfig & toml) const
{
	toml.put ("enable", enabled, "Enable or disable WebSocket server.\ntype:bool");
	toml.put ("address", address.to_string (), "WebSocket server bind address.\ntype:string,ip");
	toml.put ("port", port, "WebSocket server listening port.\ntype:uint16");
	return toml.get_error ();
}

btcnew::error btcnew::websocket::config::deserialize_toml (btcnew::tomlconfig & toml)
{
	toml.get<bool> ("enable", enabled);
	toml.get<boost::asio::ip::address_v6> ("address", address);
	toml.get<uint16_t> ("port", port);
	return toml.get_error ();
}

btcnew::error btcnew::websocket::config::serialize_json (btcnew::jsonconfig & json) const
{
	json.put ("enable", enabled);
	json.put ("address", address.to_string ());
	json.put ("port", port);
	return json.get_error ();
}

btcnew::error btcnew::websocket::config::deserialize_json (btcnew::jsonconfig & json)
{
	json.get<bool> ("enable", enabled);
	json.get_required<boost::asio::ip::address_v6> ("address", address);
	json.get<uint16_t> ("port", port);
	return json.get_error ();
}

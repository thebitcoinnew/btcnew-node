#pragma once

#include <btcnew/boost/asio.hpp>
#include <btcnew/lib/config.hpp>
#include <btcnew/lib/errors.hpp>

namespace btcnew
{
class jsonconfig;
class tomlconfig;
namespace websocket
{
	/** websocket configuration */
	class config final
	{
	public:
		config ();
		btcnew::error deserialize_json (btcnew::jsonconfig & json_a);
		btcnew::error serialize_json (btcnew::jsonconfig & json) const;
		btcnew::error deserialize_toml (btcnew::tomlconfig & toml_a);
		btcnew::error serialize_toml (btcnew::tomlconfig & toml) const;
		btcnew::network_constants network_constants;
		bool enabled{ false };
		uint16_t port;
		boost::asio::ip::address_v6 address{ boost::asio::ip::address_v6::loopback () };
	};
}
}

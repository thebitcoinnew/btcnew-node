#pragma once

#include <btcnew/lib/errors.hpp>
#include <btcnew/lib/numbers.hpp>

#include <string>

namespace btcnew
{
class tomlconfig;

/** Configuration options for the Qt wallet */
class wallet_config final
{
public:
	wallet_config ();
	/** Update this instance by parsing the given wallet and account */
	btcnew::error parse (std::string const & wallet_a, std::string const & account_a);
	btcnew::error serialize_toml (btcnew::tomlconfig & toml_a) const;
	btcnew::error deserialize_toml (btcnew::tomlconfig & toml_a);
	btcnew::wallet_id wallet;
	btcnew::account account{ 0 };
};
}

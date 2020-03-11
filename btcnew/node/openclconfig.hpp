#pragma once

#include <btcnew/lib/errors.hpp>

namespace btcnew
{
class jsonconfig;
class tomlconfig;
class opencl_config
{
public:
	opencl_config () = default;
	opencl_config (unsigned, unsigned, unsigned);
	btcnew::error serialize_json (btcnew::jsonconfig &) const;
	btcnew::error deserialize_json (btcnew::jsonconfig &);
	btcnew::error serialize_toml (btcnew::tomlconfig &) const;
	btcnew::error deserialize_toml (btcnew::tomlconfig &);
	unsigned platform{ 0 };
	unsigned device{ 0 };
	unsigned threads{ 1024 * 1024 };
};
}

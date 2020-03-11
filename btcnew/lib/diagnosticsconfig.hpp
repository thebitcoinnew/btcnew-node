#pragma once

#include <btcnew/lib/errors.hpp>

#include <chrono>

namespace btcnew
{
class jsonconfig;
class tomlconfig;
class txn_tracking_config final
{
public:
	/** If true, enable tracking for transaction read/writes held open longer than the min time variables */
	bool enable{ false };
	std::chrono::milliseconds min_read_txn_time{ 5000 };
	std::chrono::milliseconds min_write_txn_time{ 500 };
	bool ignore_writes_below_block_processor_max_time{ true };
};

/** Configuration options for diagnostics information */
class diagnostics_config final
{
public:
	btcnew::error serialize_json (btcnew::jsonconfig &) const;
	btcnew::error deserialize_json (btcnew::jsonconfig &);
	btcnew::error serialize_toml (btcnew::tomlconfig &) const;
	btcnew::error deserialize_toml (btcnew::tomlconfig &);

	txn_tracking_config txn_tracking;
};
}

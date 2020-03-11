#include "btcnew/lib/errors.hpp"

std::string btcnew::error_common_messages::message (int ev) const
{
	switch (static_cast<btcnew::error_common> (ev))
	{
		case btcnew::error_common::generic:
			return "Unknown error";
		case btcnew::error_common::missing_account:
			return "Missing account";
		case btcnew::error_common::missing_balance:
			return "Missing balance";
		case btcnew::error_common::missing_link:
			return "Missing link, source or destination";
		case btcnew::error_common::missing_previous:
			return "Missing previous";
		case btcnew::error_common::missing_representative:
			return "Missing representative";
		case btcnew::error_common::missing_signature:
			return "Missing signature";
		case btcnew::error_common::missing_work:
			return "Missing work";
		case btcnew::error_common::exception:
			return "Exception thrown";
		case btcnew::error_common::account_exists:
			return "Account already exists";
		case btcnew::error_common::account_not_found:
			return "Account not found";
		case btcnew::error_common::account_not_found_wallet:
			return "Account not found in wallet";
		case btcnew::error_common::bad_account_number:
			return "Bad account number";
		case btcnew::error_common::bad_balance:
			return "Bad balance";
		case btcnew::error_common::bad_link:
			return "Bad link value";
		case btcnew::error_common::bad_previous:
			return "Bad previous hash";
		case btcnew::error_common::bad_representative_number:
			return "Bad representative";
		case btcnew::error_common::bad_source:
			return "Bad source";
		case btcnew::error_common::bad_signature:
			return "Bad signature";
		case btcnew::error_common::bad_private_key:
			return "Bad private key";
		case btcnew::error_common::bad_public_key:
			return "Bad public key";
		case btcnew::error_common::bad_seed:
			return "Bad seed";
		case btcnew::error_common::bad_threshold:
			return "Bad threshold number";
		case btcnew::error_common::bad_wallet_number:
			return "Bad wallet number";
		case btcnew::error_common::bad_work_format:
			return "Bad work";
		case btcnew::error_common::disabled_local_work_generation:
			return "Local work generation is disabled";
		case btcnew::error_common::disabled_work_generation:
			return "Work generation is disabled";
		case btcnew::error_common::failure_work_generation:
			return "Work generation cancellation or failure";
		case btcnew::error_common::insufficient_balance:
			return "Insufficient balance";
		case btcnew::error_common::invalid_amount:
			return "Invalid amount number";
		case btcnew::error_common::invalid_amount_big:
			return "Amount too big";
		case btcnew::error_common::invalid_count:
			return "Invalid count";
		case btcnew::error_common::invalid_ip_address:
			return "Invalid IP address";
		case btcnew::error_common::invalid_port:
			return "Invalid port";
		case btcnew::error_common::invalid_index:
			return "Invalid index";
		case btcnew::error_common::invalid_type_conversion:
			return "Invalid type conversion";
		case btcnew::error_common::invalid_work:
			return "Invalid work";
		case btcnew::error_common::numeric_conversion:
			return "Numeric conversion error";
		case btcnew::error_common::tracking_not_enabled:
			return "Database transaction tracking is not enabled in the config";
		case btcnew::error_common::wallet_lmdb_max_dbs:
			return "Failed to create wallet. Increase lmdb_max_dbs in node config";
		case btcnew::error_common::wallet_locked:
			return "Wallet is locked";
		case btcnew::error_common::wallet_not_found:
			return "Wallet not found";
	}

	return "Invalid error code";
}

std::string btcnew::error_blocks_messages::message (int ev) const
{
	switch (static_cast<btcnew::error_blocks> (ev))
	{
		case btcnew::error_blocks::generic:
			return "Unknown error";
		case btcnew::error_blocks::bad_hash_number:
			return "Bad hash number";
		case btcnew::error_blocks::invalid_block:
			return "Block is invalid";
		case btcnew::error_blocks::invalid_block_hash:
			return "Invalid block hash";
		case btcnew::error_blocks::invalid_type:
			return "Invalid block type";
		case btcnew::error_blocks::not_found:
			return "Block not found";
		case btcnew::error_blocks::work_low:
			return "Block work is less than threshold";
	}

	return "Invalid error code";
}

std::string btcnew::error_rpc_messages::message (int ev) const
{
	switch (static_cast<btcnew::error_rpc> (ev))
	{
		case btcnew::error_rpc::generic:
			return "Unknown error";
		case btcnew::error_rpc::bad_destination:
			return "Bad destination account";
		case btcnew::error_rpc::bad_difficulty_format:
			return "Bad difficulty";
		case btcnew::error_rpc::bad_key:
			return "Bad key";
		case btcnew::error_rpc::bad_link:
			return "Bad link number";
		case btcnew::error_rpc::bad_multiplier_format:
			return "Bad multiplier";
		case btcnew::error_rpc::bad_previous:
			return "Bad previous";
		case btcnew::error_rpc::bad_representative_number:
			return "Bad representative number";
		case btcnew::error_rpc::bad_source:
			return "Bad source";
		case btcnew::error_rpc::bad_timeout:
			return "Bad timeout number";
		case btcnew::error_rpc::block_create_balance_mismatch:
			return "Balance mismatch for previous block";
		case btcnew::error_rpc::block_create_key_required:
			return "Private key or local wallet and account required";
		case btcnew::error_rpc::block_create_public_key_mismatch:
			return "Incorrect key for given account";
		case btcnew::error_rpc::block_create_requirements_state:
			return "Previous, representative, final balance and link (source or destination) are required";
		case btcnew::error_rpc::block_create_requirements_open:
			return "Representative account and source hash required";
		case btcnew::error_rpc::block_create_requirements_receive:
			return "Previous hash and source hash required";
		case btcnew::error_rpc::block_create_requirements_change:
			return "Representative account and previous hash required";
		case btcnew::error_rpc::block_create_requirements_send:
			return "Destination account, previous hash, current balance and amount required";
		case btcnew::error_rpc::confirmation_height_not_processing:
			return "There are no blocks currently being processed for adding confirmation height";
		case btcnew::error_rpc::confirmation_not_found:
			return "Active confirmation not found";
		case btcnew::error_rpc::difficulty_limit:
			return "Difficulty above config limit or below publish threshold";
		case btcnew::error_rpc::disabled_bootstrap_lazy:
			return "Lazy bootstrap is disabled";
		case btcnew::error_rpc::disabled_bootstrap_legacy:
			return "Legacy bootstrap is disabled";
		case btcnew::error_rpc::invalid_balance:
			return "Invalid balance number";
		case btcnew::error_rpc::invalid_destinations:
			return "Invalid destinations number";
		case btcnew::error_rpc::invalid_epoch:
			return "Invalid epoch number";
		case btcnew::error_rpc::invalid_epoch_signer:
			return "Incorrect epoch signer";
		case btcnew::error_rpc::invalid_offset:
			return "Invalid offset";
		case btcnew::error_rpc::invalid_missing_type:
			return "Invalid or missing type argument";
		case btcnew::error_rpc::invalid_root:
			return "Invalid root hash";
		case btcnew::error_rpc::invalid_sources:
			return "Invalid sources number";
		case btcnew::error_rpc::invalid_subtype:
			return "Invalid block subtype";
		case btcnew::error_rpc::invalid_subtype_balance:
			return "Invalid block balance for given subtype";
		case btcnew::error_rpc::invalid_subtype_epoch_link:
			return "Invalid epoch link";
		case btcnew::error_rpc::invalid_subtype_previous:
			return "Invalid previous block for given subtype";
		case btcnew::error_rpc::invalid_timestamp:
			return "Invalid timestamp";
		case btcnew::error_rpc::payment_account_balance:
			return "Account has non-zero balance";
		case btcnew::error_rpc::payment_unable_create_account:
			return "Unable to create transaction account";
		case btcnew::error_rpc::rpc_control_disabled:
			return "RPC control is disabled";
		case btcnew::error_rpc::sign_hash_disabled:
			return "Signing by block hash is disabled";
		case btcnew::error_rpc::source_not_found:
			return "Source not found";
	}

	return "Invalid error code";
}

std::string btcnew::error_process_messages::message (int ev) const
{
	switch (static_cast<btcnew::error_process> (ev))
	{
		case btcnew::error_process::generic:
			return "Unknown error";
		case btcnew::error_process::bad_signature:
			return "Bad signature";
		case btcnew::error_process::old:
			return "Old block";
		case btcnew::error_process::negative_spend:
			return "Negative spend";
		case btcnew::error_process::fork:
			return "Fork";
		case btcnew::error_process::unreceivable:
			return "Unreceivable";
		case btcnew::error_process::gap_previous:
			return "Gap previous block";
		case btcnew::error_process::gap_source:
			return "Gap source block";
		case btcnew::error_process::opened_burn_account:
			return "Burning account";
		case btcnew::error_process::balance_mismatch:
			return "Balance and amount delta do not match";
		case btcnew::error_process::block_position:
			return "This block cannot follow the previous block";
		case btcnew::error_process::other:
			return "Error processing block";
	}

	return "Invalid error code";
}

std::string btcnew::error_config_messages::message (int ev) const
{
	switch (static_cast<btcnew::error_config> (ev))
	{
		case btcnew::error_config::generic:
			return "Unknown error";
		case btcnew::error_config::invalid_value:
			return "Invalid configuration value";
		case btcnew::error_config::missing_value:
			return "Missing value in configuration";
		case btcnew::error_config::rocksdb_enabled_but_not_supported:
			return "RocksDB has been enabled, but the node has not been built with RocksDB support. Set the CMake flag -DBTCNEW_ROCKSDB=ON";
	}

	return "Invalid error code";
}

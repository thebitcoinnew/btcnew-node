#include <btcnew/core_test/testutil.hpp>
#include <btcnew/crypto_lib/random_pool.hpp>
#include <btcnew/lib/config.hpp>
#include <btcnew/lib/numbers.hpp>
#include <btcnew/secure/blockstore.hpp>
#include <btcnew/secure/common.hpp>

#include <boost/endian/conversion.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <iostream>
#include <limits>
#include <queue>

#include <crypto/ed25519-donna/ed25519.h>

size_t constexpr btcnew::send_block::size;
size_t constexpr btcnew::receive_block::size;
size_t constexpr btcnew::open_block::size;
size_t constexpr btcnew::change_block::size;
size_t constexpr btcnew::state_block::size;

btcnew::btcnew_networks btcnew::network_constants::active_network = btcnew::btcnew_networks::ACTIVE_NETWORK;

namespace
{
char const * test_private_key_data = "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4";
char const * test_public_key_data = "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // btcnew_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo
char const * beta_public_key_data = "259A45BDF6418D51982223E41B40B0104F859ACEBBBB71D84FB2C390718EB1B2"; // btcnew_1betapyzeiefc8e46az65f1d164hipfexgxug9e6zep5k3rrxefkzhsuco6s
char const * live_public_key_data = "274AA339E4432EC4FBFE717E070A3D24133E3BA8E89D03FD2E9F6945582B83DE"; // btcnew_1btcnewyaisgrmxzwwdy1w75tb1m9rxtjt6x1hykx9ubaoe4q1yyg1dh69jb
char const * test_genesis_data = R"%%%({
	"type": "open",
	"source": "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
	"representative": "btcnew_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
	"account": "btcnew_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
	"work": "7b42a00ee91d5810",
	"signature": "ECDA914373A2F0CA1296475BAEE40500A7F0A7AD72A5A80C81D7FAB7F6C802B2CC7DB50F5DD0FB25B2EF11761FA7344A158DD5A700B21BD47DE5BD0F63153A02"
	})%%%";

char const * beta_genesis_data = R"%%%({
    "type": "open",
    "source": "259A45BDF6418D51982223E41B40B0104F859ACEBBBB71D84FB2C390718EB1B2",
    "representative": "btcnew_1betapyzeiefc8e46az65f1d164hipfexgxug9e6zep5k3rrxefkzhsuco6s",
    "account": "btcnew_1betapyzeiefc8e46az65f1d164hipfexgxug9e6zep5k3rrxefkzhsuco6s",
    "work": "0846c04fbf89e463",
    "signature": "B4A59108AB60FE729D88D3B2D56A6AEB73CF9EE87EE9C9222489A1231BB8B8FFC6F8B0331F5D62BBD554C95EB8F244326F1B4A0270110E22ABE25EBF8B5F0D0B"
	})%%%";

char const * live_genesis_data = R"%%%({
    "type": "open",
    "source": "274AA339E4432EC4FBFE717E070A3D24133E3BA8E89D03FD2E9F6945582B83DE",
    "representative": "btcnew_1btcnewyaisgrmxzwwdy1w75tb1m9rxtjt6x1hykx9ubaoe4q1yyg1dh69jb",
    "account": "btcnew_1btcnewyaisgrmxzwwdy1w75tb1m9rxtjt6x1hykx9ubaoe4q1yyg1dh69jb",
    "work": "72264d3182383172",
    "signature": "1D78E877E954919E2386EC7E43588DB882C37F0919C2473761F70E8795389C4837BEA46B1036962DE309AC2124566DAEC76C5ED4570704034D02D7FF31B7E908"
	})%%%";
}

btcnew::network_params::network_params () :
network_params (network_constants::active_network)
{
}

btcnew::network_params::network_params (btcnew::btcnew_networks network_a) :
network (network_a), protocol (network_a), ledger (network), voting (network), node (network), portmapping (network), bootstrap (network)
{
	unsigned constexpr kdf_full_work = 64 * 1024;
	unsigned constexpr kdf_test_work = 8;
	kdf_work = network.is_test_network () ? kdf_test_work : kdf_full_work;
	header_magic_number = network.is_test_network () ? std::array<uint8_t, 2>{ { 'R', 'A' } } : network.is_beta_network () ? std::array<uint8_t, 2>{ { 'N', 'B' } } : std::array<uint8_t, 2>{ { 'R', 'C' } };
}

btcnew::protocol_constants::protocol_constants (btcnew::btcnew_networks network_a)
{
}

btcnew::ledger_constants::ledger_constants (btcnew::network_constants & network_constants) :
ledger_constants (network_constants.network ())
{
}

btcnew::ledger_constants::ledger_constants (btcnew::btcnew_networks network_a) :
zero_key ("0"),
test_genesis_key (test_private_key_data),
btcnew_test_account (test_public_key_data),
btcnew_beta_account (beta_public_key_data),
btcnew_live_account (live_public_key_data),
btcnew_test_genesis (test_genesis_data),
btcnew_beta_genesis (beta_genesis_data),
btcnew_live_genesis (live_genesis_data),
genesis_account (network_a == btcnew::btcnew_networks::btcnew_test_network ? btcnew_test_account : network_a == btcnew::btcnew_networks::btcnew_beta_network ? btcnew_beta_account : btcnew_live_account),
genesis_block (network_a == btcnew::btcnew_networks::btcnew_test_network ? btcnew_test_genesis : network_a == btcnew::btcnew_networks::btcnew_beta_network ? btcnew_beta_genesis : btcnew_live_genesis),
genesis_amount (std::numeric_limits<btcnew::uint128_t>::max ()),
burn_account (0)
{
	btcnew::link epoch_link_v1;
	const char * epoch_message_v1 ("epoch v1 block");
	strncpy ((char *)epoch_link_v1.bytes.data (), epoch_message_v1, epoch_link_v1.bytes.size ());
	epochs.add (btcnew::epoch::epoch_1, genesis_account, epoch_link_v1);

	btcnew::link epoch_link_v2;
	auto btcnew_live_epoch_v2_signer = genesis_account;
	auto epoch_v2_signer (network_a == btcnew::btcnew_networks::btcnew_test_network ? btcnew_test_account : network_a == btcnew::btcnew_networks::btcnew_beta_network ? btcnew_beta_account : btcnew_live_epoch_v2_signer);
	const char * epoch_message_v2 ("epoch v2 block");
	strncpy ((char *)epoch_link_v2.bytes.data (), epoch_message_v2, epoch_link_v2.bytes.size ());
	epochs.add (btcnew::epoch::epoch_2, epoch_v2_signer, epoch_link_v2);
}

btcnew::random_constants::random_constants ()
{
	btcnew::random_pool::generate_block (not_an_account.bytes.data (), not_an_account.bytes.size ());
	btcnew::random_pool::generate_block (random_128.bytes.data (), random_128.bytes.size ());
}

btcnew::node_constants::node_constants (btcnew::network_constants & network_constants)
{
	period = network_constants.is_test_network () ? std::chrono::seconds (1) : std::chrono::seconds (60);
	half_period = network_constants.is_test_network () ? std::chrono::milliseconds (500) : std::chrono::milliseconds (30 * 1000);
	idle_timeout = network_constants.is_test_network () ? period * 15 : period * 2;
	cutoff = period * 5;
	syn_cookie_cutoff = std::chrono::seconds (5);
	backup_interval = std::chrono::minutes (5);
	search_pending_interval = network_constants.is_test_network () ? std::chrono::seconds (1) : std::chrono::seconds (5 * 60);
	peer_interval = search_pending_interval;
	unchecked_cleaning_interval = std::chrono::minutes (30);
	process_confirmed_interval = network_constants.is_test_network () ? std::chrono::milliseconds (50) : std::chrono::milliseconds (500);
	max_weight_samples = network_constants.is_live_network () ? 4032 : 864;
	weight_period = 5 * 60; // 5 minutes
}

btcnew::voting_constants::voting_constants (btcnew::network_constants & network_constants)
{
	max_cache = network_constants.is_test_network () ? 2 : 4 * 1024;
}

btcnew::portmapping_constants::portmapping_constants (btcnew::network_constants & network_constants)
{
	mapping_timeout = network_constants.is_test_network () ? 53 : 3593;
	check_timeout = network_constants.is_test_network () ? 17 : 53;
}

btcnew::bootstrap_constants::bootstrap_constants (btcnew::network_constants & network_constants)
{
	lazy_max_pull_blocks = network_constants.is_test_network () ? 2 : 512;
	lazy_min_pull_blocks = network_constants.is_test_network () ? 1 : 32;
	frontier_retry_limit = network_constants.is_test_network () ? 2 : 16;
	lazy_retry_limit = network_constants.is_test_network () ? 2 : frontier_retry_limit * 10;
	lazy_destinations_retry_limit = network_constants.is_test_network () ? 1 : frontier_retry_limit / 4;
}

/* Convenience constants for core_test which is always on the test network */
namespace
{
btcnew::ledger_constants test_constants (btcnew::btcnew_networks::btcnew_test_network);
}

btcnew::keypair const & btcnew::zero_key (test_constants.zero_key);
btcnew::keypair const & btcnew::test_genesis_key (test_constants.test_genesis_key);
btcnew::account const & btcnew::btcnew_test_account (test_constants.btcnew_test_account);
std::string const & btcnew::btcnew_test_genesis (test_constants.btcnew_test_genesis);
btcnew::account const & btcnew::genesis_account (test_constants.genesis_account);
std::string const & btcnew::genesis_block (test_constants.genesis_block);
btcnew::uint128_t const & btcnew::genesis_amount (test_constants.genesis_amount);
btcnew::account const & btcnew::burn_account (test_constants.burn_account);

// Create a new random keypair
btcnew::keypair::keypair ()
{
	random_pool::generate_block (prv.data.bytes.data (), prv.data.bytes.size ());
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a private key
btcnew::keypair::keypair (btcnew::raw_key && prv_a) :
prv (std::move (prv_a))
{
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a hex string of the private key
btcnew::keypair::keypair (std::string const & prv_a)
{
	auto error (prv.data.decode_hex (prv_a));
	(void)error;
	assert (!error);
	ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Serialize a block prefixed with an 8-bit typecode
void btcnew::serialize_block (btcnew::stream & stream_a, btcnew::block const & block_a)
{
	write (stream_a, block_a.type ());
	block_a.serialize (stream_a);
}

btcnew::account_info::account_info (btcnew::block_hash const & head_a, btcnew::account const & representative_a, btcnew::block_hash const & open_block_a, btcnew::amount const & balance_a, uint64_t modified_a, uint64_t block_count_a, btcnew::epoch epoch_a) :
head (head_a),
representative (representative_a),
open_block (open_block_a),
balance (balance_a),
modified (modified_a),
block_count (block_count_a),
epoch_m (epoch_a)
{
}

bool btcnew::account_info::deserialize (btcnew::stream & stream_a)
{
	auto error (false);
	try
	{
		btcnew::read (stream_a, head.bytes);
		btcnew::read (stream_a, representative.bytes);
		btcnew::read (stream_a, open_block.bytes);
		btcnew::read (stream_a, balance.bytes);
		btcnew::read (stream_a, modified);
		btcnew::read (stream_a, block_count);
		btcnew::read (stream_a, epoch_m);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

bool btcnew::account_info::operator== (btcnew::account_info const & other_a) const
{
	return head == other_a.head && representative == other_a.representative && open_block == other_a.open_block && balance == other_a.balance && modified == other_a.modified && block_count == other_a.block_count && epoch () == other_a.epoch ();
}

bool btcnew::account_info::operator!= (btcnew::account_info const & other_a) const
{
	return !(*this == other_a);
}

size_t btcnew::account_info::db_size () const
{
	assert (reinterpret_cast<const uint8_t *> (this) == reinterpret_cast<const uint8_t *> (&head));
	assert (reinterpret_cast<const uint8_t *> (&head) + sizeof (head) == reinterpret_cast<const uint8_t *> (&representative));
	assert (reinterpret_cast<const uint8_t *> (&representative) + sizeof (representative) == reinterpret_cast<const uint8_t *> (&open_block));
	assert (reinterpret_cast<const uint8_t *> (&open_block) + sizeof (open_block) == reinterpret_cast<const uint8_t *> (&balance));
	assert (reinterpret_cast<const uint8_t *> (&balance) + sizeof (balance) == reinterpret_cast<const uint8_t *> (&modified));
	assert (reinterpret_cast<const uint8_t *> (&modified) + sizeof (modified) == reinterpret_cast<const uint8_t *> (&block_count));
	assert (reinterpret_cast<const uint8_t *> (&block_count) + sizeof (block_count) == reinterpret_cast<const uint8_t *> (&epoch_m));
	return sizeof (head) + sizeof (representative) + sizeof (open_block) + sizeof (balance) + sizeof (modified) + sizeof (block_count) + sizeof (epoch_m);
}

btcnew::epoch btcnew::account_info::epoch () const
{
	return epoch_m;
}

size_t btcnew::block_counts::sum () const
{
	return send + receive + open + change + state;
}

btcnew::pending_info::pending_info (btcnew::account const & source_a, btcnew::amount const & amount_a, btcnew::epoch epoch_a) :
source (source_a),
amount (amount_a),
epoch (epoch_a)
{
}

bool btcnew::pending_info::deserialize (btcnew::stream & stream_a)
{
	auto error (false);
	try
	{
		btcnew::read (stream_a, source.bytes);
		btcnew::read (stream_a, amount.bytes);
		btcnew::read (stream_a, epoch);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

size_t btcnew::pending_info::db_size () const
{
	return sizeof (source) + sizeof (amount) + sizeof (epoch);
}

bool btcnew::pending_info::operator== (btcnew::pending_info const & other_a) const
{
	return source == other_a.source && amount == other_a.amount && epoch == other_a.epoch;
}

btcnew::pending_key::pending_key (btcnew::account const & account_a, btcnew::block_hash const & hash_a) :
account (account_a),
hash (hash_a)
{
}

bool btcnew::pending_key::deserialize (btcnew::stream & stream_a)
{
	auto error (false);
	try
	{
		btcnew::read (stream_a, account.bytes);
		btcnew::read (stream_a, hash.bytes);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

bool btcnew::pending_key::operator== (btcnew::pending_key const & other_a) const
{
	return account == other_a.account && hash == other_a.hash;
}

btcnew::account const & btcnew::pending_key::key () const
{
	return account;
}

btcnew::unchecked_info::unchecked_info (std::shared_ptr<btcnew::block> block_a, btcnew::account const & account_a, uint64_t modified_a, btcnew::signature_verification verified_a, bool confirmed_a) :
block (block_a),
account (account_a),
modified (modified_a),
verified (verified_a),
confirmed (confirmed_a)
{
}

void btcnew::unchecked_info::serialize (btcnew::stream & stream_a) const
{
	assert (block != nullptr);
	btcnew::serialize_block (stream_a, *block);
	btcnew::write (stream_a, account.bytes);
	btcnew::write (stream_a, modified);
	btcnew::write (stream_a, verified);
}

bool btcnew::unchecked_info::deserialize (btcnew::stream & stream_a)
{
	block = btcnew::deserialize_block (stream_a);
	bool error (block == nullptr);
	if (!error)
	{
		try
		{
			btcnew::read (stream_a, account.bytes);
			btcnew::read (stream_a, modified);
			btcnew::read (stream_a, verified);
		}
		catch (std::runtime_error const &)
		{
			error = true;
		}
	}
	return error;
}

btcnew::endpoint_key::endpoint_key (const std::array<uint8_t, 16> & address_a, uint16_t port_a) :
address (address_a), network_port (boost::endian::native_to_big (port_a))
{
}

const std::array<uint8_t, 16> & btcnew::endpoint_key::address_bytes () const
{
	return address;
}

uint16_t btcnew::endpoint_key::port () const
{
	return boost::endian::big_to_native (network_port);
}

btcnew::block_info::block_info (btcnew::account const & account_a, btcnew::amount const & balance_a) :
account (account_a),
balance (balance_a)
{
}

bool btcnew::vote::operator== (btcnew::vote const & other_a) const
{
	auto blocks_equal (true);
	if (blocks.size () != other_a.blocks.size ())
	{
		blocks_equal = false;
	}
	else
	{
		for (auto i (0); blocks_equal && i < blocks.size (); ++i)
		{
			auto block (blocks[i]);
			auto other_block (other_a.blocks[i]);
			if (block.which () != other_block.which ())
			{
				blocks_equal = false;
			}
			else if (block.which ())
			{
				if (boost::get<btcnew::block_hash> (block) != boost::get<btcnew::block_hash> (other_block))
				{
					blocks_equal = false;
				}
			}
			else
			{
				if (!(*boost::get<std::shared_ptr<btcnew::block>> (block) == *boost::get<std::shared_ptr<btcnew::block>> (other_block)))
				{
					blocks_equal = false;
				}
			}
		}
	}
	return sequence == other_a.sequence && blocks_equal && account == other_a.account && signature == other_a.signature;
}

bool btcnew::vote::operator!= (btcnew::vote const & other_a) const
{
	return !(*this == other_a);
}

void btcnew::vote::serialize_json (boost::property_tree::ptree & tree) const
{
	tree.put ("account", account.to_account ());
	tree.put ("signature", signature.number ());
	tree.put ("sequence", std::to_string (sequence));
	boost::property_tree::ptree blocks_tree;
	for (auto block : blocks)
	{
		boost::property_tree::ptree entry;
		if (block.which ())
		{
			entry.put ("", boost::get<btcnew::block_hash> (block).to_string ());
		}
		else
		{
			entry.put ("", boost::get<std::shared_ptr<btcnew::block>> (block)->hash ().to_string ());
		}
		blocks_tree.push_back (std::make_pair ("", entry));
	}
	tree.add_child ("blocks", blocks_tree);
}

std::string btcnew::vote::to_json () const
{
	std::stringstream stream;
	boost::property_tree::ptree tree;
	serialize_json (tree);
	boost::property_tree::write_json (stream, tree);
	return stream.str ();
}

btcnew::vote::vote (btcnew::vote const & other_a) :
sequence (other_a.sequence),
blocks (other_a.blocks),
account (other_a.account),
signature (other_a.signature)
{
}

btcnew::vote::vote (bool & error_a, btcnew::stream & stream_a, btcnew::block_uniquer * uniquer_a)
{
	error_a = deserialize (stream_a, uniquer_a);
}

btcnew::vote::vote (bool & error_a, btcnew::stream & stream_a, btcnew::block_type type_a, btcnew::block_uniquer * uniquer_a)
{
	try
	{
		btcnew::read (stream_a, account.bytes);
		btcnew::read (stream_a, signature.bytes);
		btcnew::read (stream_a, sequence);

		while (stream_a.in_avail () > 0)
		{
			if (type_a == btcnew::block_type::not_a_block)
			{
				btcnew::block_hash block_hash;
				btcnew::read (stream_a, block_hash);
				blocks.push_back (block_hash);
			}
			else
			{
				std::shared_ptr<btcnew::block> block (btcnew::deserialize_block (stream_a, type_a, uniquer_a));
				if (block == nullptr)
				{
					throw std::runtime_error ("Block is null");
				}
				blocks.push_back (block);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}

	if (blocks.empty ())
	{
		error_a = true;
	}
}

btcnew::vote::vote (btcnew::account const & account_a, btcnew::raw_key const & prv_a, uint64_t sequence_a, std::shared_ptr<btcnew::block> block_a) :
sequence (sequence_a),
blocks (1, block_a),
account (account_a),
signature (btcnew::sign_message (prv_a, account_a, hash ()))
{
}

btcnew::vote::vote (btcnew::account const & account_a, btcnew::raw_key const & prv_a, uint64_t sequence_a, std::vector<btcnew::block_hash> const & blocks_a) :
sequence (sequence_a),
account (account_a)
{
	assert (!blocks_a.empty ());
	assert (blocks_a.size () <= 12);
	blocks.reserve (blocks_a.size ());
	std::copy (blocks_a.cbegin (), blocks_a.cend (), std::back_inserter (blocks));
	signature = btcnew::sign_message (prv_a, account_a, hash ());
}

std::string btcnew::vote::hashes_string () const
{
	std::string result;
	for (auto hash : *this)
	{
		result += hash.to_string ();
		result += ", ";
	}
	return result;
}

const std::string btcnew::vote::hash_prefix = "vote ";

btcnew::block_hash btcnew::vote::hash () const
{
	btcnew::block_hash result;
	blake2b_state hash;
	blake2b_init (&hash, sizeof (result.bytes));
	if (blocks.size () > 1 || (!blocks.empty () && blocks.front ().which ()))
	{
		blake2b_update (&hash, hash_prefix.data (), hash_prefix.size ());
	}
	for (auto block_hash : *this)
	{
		blake2b_update (&hash, block_hash.bytes.data (), sizeof (block_hash.bytes));
	}
	union
	{
		uint64_t qword;
		std::array<uint8_t, 8> bytes;
	};
	qword = sequence;
	blake2b_update (&hash, bytes.data (), sizeof (bytes));
	blake2b_final (&hash, result.bytes.data (), sizeof (result.bytes));
	return result;
}

btcnew::block_hash btcnew::vote::full_hash () const
{
	btcnew::block_hash result;
	blake2b_state state;
	blake2b_init (&state, sizeof (result.bytes));
	blake2b_update (&state, hash ().bytes.data (), sizeof (hash ().bytes));
	blake2b_update (&state, account.bytes.data (), sizeof (account.bytes.data ()));
	blake2b_update (&state, signature.bytes.data (), sizeof (signature.bytes.data ()));
	blake2b_final (&state, result.bytes.data (), sizeof (result.bytes));
	return result;
}

void btcnew::vote::serialize (btcnew::stream & stream_a, btcnew::block_type type) const
{
	write (stream_a, account);
	write (stream_a, signature);
	write (stream_a, sequence);
	for (auto const & block : blocks)
	{
		if (block.which ())
		{
			assert (type == btcnew::block_type::not_a_block);
			write (stream_a, boost::get<btcnew::block_hash> (block));
		}
		else
		{
			if (type == btcnew::block_type::not_a_block)
			{
				write (stream_a, boost::get<std::shared_ptr<btcnew::block>> (block)->hash ());
			}
			else
			{
				boost::get<std::shared_ptr<btcnew::block>> (block)->serialize (stream_a);
			}
		}
	}
}

void btcnew::vote::serialize (btcnew::stream & stream_a) const
{
	write (stream_a, account);
	write (stream_a, signature);
	write (stream_a, sequence);
	for (auto const & block : blocks)
	{
		if (block.which ())
		{
			write (stream_a, btcnew::block_type::not_a_block);
			write (stream_a, boost::get<btcnew::block_hash> (block));
		}
		else
		{
			btcnew::serialize_block (stream_a, *boost::get<std::shared_ptr<btcnew::block>> (block));
		}
	}
}

bool btcnew::vote::deserialize (btcnew::stream & stream_a, btcnew::block_uniquer * uniquer_a)
{
	auto error (false);
	try
	{
		btcnew::read (stream_a, account);
		btcnew::read (stream_a, signature);
		btcnew::read (stream_a, sequence);

		btcnew::block_type type;

		while (true)
		{
			if (btcnew::try_read (stream_a, type))
			{
				// Reached the end of the stream
				break;
			}

			if (type == btcnew::block_type::not_a_block)
			{
				btcnew::block_hash block_hash;
				btcnew::read (stream_a, block_hash);
				blocks.push_back (block_hash);
			}
			else
			{
				std::shared_ptr<btcnew::block> block (btcnew::deserialize_block (stream_a, type, uniquer_a));
				if (block == nullptr)
				{
					throw std::runtime_error ("Block is empty");
				}

				blocks.push_back (block);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	if (blocks.empty ())
	{
		error = true;
	}

	return error;
}

bool btcnew::vote::validate () const
{
	return btcnew::validate_message (account, hash (), signature);
}

btcnew::block_hash btcnew::iterate_vote_blocks_as_hash::operator() (boost::variant<std::shared_ptr<btcnew::block>, btcnew::block_hash> const & item) const
{
	btcnew::block_hash result;
	if (item.which ())
	{
		result = boost::get<btcnew::block_hash> (item);
	}
	else
	{
		result = boost::get<std::shared_ptr<btcnew::block>> (item)->hash ();
	}
	return result;
}

boost::transform_iterator<btcnew::iterate_vote_blocks_as_hash, btcnew::vote_blocks_vec_iter> btcnew::vote::begin () const
{
	return boost::transform_iterator<btcnew::iterate_vote_blocks_as_hash, btcnew::vote_blocks_vec_iter> (blocks.begin (), btcnew::iterate_vote_blocks_as_hash ());
}

boost::transform_iterator<btcnew::iterate_vote_blocks_as_hash, btcnew::vote_blocks_vec_iter> btcnew::vote::end () const
{
	return boost::transform_iterator<btcnew::iterate_vote_blocks_as_hash, btcnew::vote_blocks_vec_iter> (blocks.end (), btcnew::iterate_vote_blocks_as_hash ());
}

btcnew::vote_uniquer::vote_uniquer (btcnew::block_uniquer & uniquer_a) :
uniquer (uniquer_a)
{
}

std::shared_ptr<btcnew::vote> btcnew::vote_uniquer::unique (std::shared_ptr<btcnew::vote> vote_a)
{
	auto result (vote_a);
	if (result != nullptr && !result->blocks.empty ())
	{
		if (!result->blocks.front ().which ())
		{
			result->blocks.front () = uniquer.unique (boost::get<std::shared_ptr<btcnew::block>> (result->blocks.front ()));
		}
		btcnew::block_hash key (vote_a->full_hash ());
		btcnew::lock_guard<std::mutex> lock (mutex);
		auto & existing (votes[key]);
		if (auto block_l = existing.lock ())
		{
			result = block_l;
		}
		else
		{
			existing = vote_a;
		}

		release_assert (std::numeric_limits<CryptoPP::word32>::max () > votes.size ());
		for (auto i (0); i < cleanup_count && !votes.empty (); ++i)
		{
			auto random_offset = btcnew::random_pool::generate_word32 (0, static_cast<CryptoPP::word32> (votes.size () - 1));

			auto existing (std::next (votes.begin (), random_offset));
			if (existing == votes.end ())
			{
				existing = votes.begin ();
			}
			if (existing != votes.end ())
			{
				if (auto block_l = existing->second.lock ())
				{
					// Still live
				}
				else
				{
					votes.erase (existing);
				}
			}
		}
	}
	return result;
}

size_t btcnew::vote_uniquer::size ()
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	return votes.size ();
}

namespace btcnew
{
std::unique_ptr<seq_con_info_component> collect_seq_con_info (vote_uniquer & vote_uniquer, const std::string & name)
{
	auto count = vote_uniquer.size ();
	auto sizeof_element = sizeof (vote_uniquer::value_type);
	auto composite = std::make_unique<seq_con_info_composite> (name);
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "votes", count, sizeof_element }));
	return composite;
}
}

btcnew::genesis::genesis ()
{
	static btcnew::network_params network_params;
	boost::property_tree::ptree tree;
	std::stringstream istream (network_params.ledger.genesis_block);
	boost::property_tree::read_json (istream, tree);
	open = btcnew::deserialize_block_json (tree);
	assert (open != nullptr);
}

btcnew::block_hash btcnew::genesis::hash () const
{
	return open->hash ();
}

btcnew::wallet_id btcnew::random_wallet_id ()
{
	btcnew::wallet_id wallet_id;
	btcnew::uint256_union dummy_secret;
	random_pool::generate_block (dummy_secret.bytes.data (), dummy_secret.bytes.size ());
	ed25519_publickey (dummy_secret.bytes.data (), wallet_id.bytes.data ());
	return wallet_id;
}

btcnew::unchecked_key::unchecked_key (btcnew::block_hash const & previous_a, btcnew::block_hash const & hash_a) :
previous (previous_a),
hash (hash_a)
{
}

bool btcnew::unchecked_key::deserialize (btcnew::stream & stream_a)
{
	auto error (false);
	try
	{
		btcnew::read (stream_a, previous.bytes);
		btcnew::read (stream_a, hash.bytes);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

bool btcnew::unchecked_key::operator== (btcnew::unchecked_key const & other_a) const
{
	return previous == other_a.previous && hash == other_a.hash;
}

btcnew::block_hash const & btcnew::unchecked_key::key () const
{
	return previous;
}
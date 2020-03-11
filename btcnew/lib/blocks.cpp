#include <btcnew/crypto_lib/random_pool.hpp>
#include <btcnew/lib/blocks.hpp>
#include <btcnew/lib/memory.hpp>
#include <btcnew/lib/numbers.hpp>
#include <btcnew/lib/utility.hpp>

#include <boost/endian/conversion.hpp>
#include <boost/pool/pool_alloc.hpp>

/** Compare blocks, first by type, then content. This is an optimization over dynamic_cast, which is very slow on some platforms. */
namespace
{
template <typename T>
bool blocks_equal (T const & first, btcnew::block const & second)
{
	static_assert (std::is_base_of<btcnew::block, T>::value, "Input parameter is not a block type");
	return (first.type () == second.type ()) && (static_cast<T const &> (second)) == first;
}

template <typename block>
std::shared_ptr<block> deserialize_block (btcnew::stream & stream_a)
{
	auto error (false);
	auto result = btcnew::make_shared<block> (error, stream_a);
	if (error)
	{
		result = nullptr;
	}

	return result;
}
}

void btcnew::block_memory_pool_purge ()
{
	btcnew::purge_singleton_pool_memory<btcnew::open_block> ();
	btcnew::purge_singleton_pool_memory<btcnew::state_block> ();
	btcnew::purge_singleton_pool_memory<btcnew::send_block> ();
	btcnew::purge_singleton_pool_memory<btcnew::change_block> ();
}

std::string btcnew::block::to_json () const
{
	std::string result;
	serialize_json (result);
	return result;
}

size_t btcnew::block::size (btcnew::block_type type_a)
{
	size_t result (0);
	switch (type_a)
	{
		case btcnew::block_type::invalid:
		case btcnew::block_type::not_a_block:
			assert (false);
			break;
		case btcnew::block_type::send:
			result = btcnew::send_block::size;
			break;
		case btcnew::block_type::receive:
			result = btcnew::receive_block::size;
			break;
		case btcnew::block_type::change:
			result = btcnew::change_block::size;
			break;
		case btcnew::block_type::open:
			result = btcnew::open_block::size;
			break;
		case btcnew::block_type::state:
			result = btcnew::state_block::size;
			break;
	}
	return result;
}

btcnew::block_hash btcnew::block::hash () const
{
	btcnew::block_hash result;
	blake2b_state hash_l;
	auto status (blake2b_init (&hash_l, sizeof (result.bytes)));
	assert (status == 0);
	hash (hash_l);
	status = blake2b_final (&hash_l, result.bytes.data (), sizeof (result.bytes));
	assert (status == 0);
	return result;
}

btcnew::block_hash btcnew::block::full_hash () const
{
	btcnew::block_hash result;
	blake2b_state state;
	blake2b_init (&state, sizeof (result.bytes));
	blake2b_update (&state, hash ().bytes.data (), sizeof (hash ()));
	auto signature (block_signature ());
	blake2b_update (&state, signature.bytes.data (), sizeof (signature));
	auto work (block_work ());
	blake2b_update (&state, &work, sizeof (work));
	blake2b_final (&state, result.bytes.data (), sizeof (result.bytes));
	return result;
}

btcnew::account const & btcnew::block::representative () const
{
	static btcnew::account rep{ 0 };
	return rep;
}

btcnew::block_hash const & btcnew::block::source () const
{
	static btcnew::block_hash source{ 0 };
	return source;
}

btcnew::link const & btcnew::block::link () const
{
	static btcnew::link link{ 0 };
	return link;
}

btcnew::account const & btcnew::block::account () const
{
	static btcnew::account account{ 0 };
	return account;
}

btcnew::qualified_root btcnew::block::qualified_root () const
{
	return btcnew::qualified_root (previous (), root ());
}

btcnew::amount const & btcnew::block::balance () const
{
	static btcnew::amount amount{ 0 };
	return amount;
}

void btcnew::send_block::visit (btcnew::block_visitor & visitor_a) const
{
	visitor_a.send_block (*this);
}

void btcnew::send_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t btcnew::send_block::block_work () const
{
	return work;
}

void btcnew::send_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

btcnew::send_hashables::send_hashables (btcnew::block_hash const & previous_a, btcnew::account const & destination_a, btcnew::amount const & balance_a) :
previous (previous_a),
destination (destination_a),
balance (balance_a)
{
}

btcnew::send_hashables::send_hashables (bool & error_a, btcnew::stream & stream_a)
{
	try
	{
		btcnew::read (stream_a, previous.bytes);
		btcnew::read (stream_a, destination.bytes);
		btcnew::read (stream_a, balance.bytes);
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

btcnew::send_hashables::send_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto destination_l (tree_a.get<std::string> ("destination"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = destination.decode_account (destination_l);
			if (!error_a)
			{
				error_a = balance.decode_hex (balance_l);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void btcnew::send_hashables::hash (blake2b_state & hash_a) const
{
	auto status (blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes)));
	assert (status == 0);
	status = blake2b_update (&hash_a, destination.bytes.data (), sizeof (destination.bytes));
	assert (status == 0);
	status = blake2b_update (&hash_a, balance.bytes.data (), sizeof (balance.bytes));
	assert (status == 0);
}

void btcnew::send_block::serialize (btcnew::stream & stream_a) const
{
	write (stream_a, hashables.previous.bytes);
	write (stream_a, hashables.destination.bytes);
	write (stream_a, hashables.balance.bytes);
	write (stream_a, signature.bytes);
	write (stream_a, work);
}

bool btcnew::send_block::deserialize (btcnew::stream & stream_a)
{
	auto error (false);
	try
	{
		read (stream_a, hashables.previous.bytes);
		read (stream_a, hashables.destination.bytes);
		read (stream_a, hashables.balance.bytes);
		read (stream_a, signature.bytes);
		read (stream_a, work);
	}
	catch (std::exception const &)
	{
		error = true;
	}

	return error;
}

void btcnew::send_block::serialize_json (std::string & string_a, bool single_line) const
{
	boost::property_tree::ptree tree;
	serialize_json (tree);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree, !single_line);
	string_a = ostream.str ();
}

void btcnew::send_block::serialize_json (boost::property_tree::ptree & tree) const
{
	tree.put ("type", "send");
	std::string previous;
	hashables.previous.encode_hex (previous);
	tree.put ("previous", previous);
	tree.put ("destination", hashables.destination.to_account ());
	std::string balance;
	hashables.balance.encode_hex (balance);
	tree.put ("balance", balance);
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", btcnew::to_string_hex (work));
	tree.put ("signature", signature_l);
}

bool btcnew::send_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "send");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto destination_l (tree_a.get<std::string> ("destination"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.destination.decode_account (destination_l);
			if (!error)
			{
				error = hashables.balance.decode_hex (balance_l);
				if (!error)
				{
					error = btcnew::from_string_hex (work_l, work);
					if (!error)
					{
						error = signature.decode_hex (signature_l);
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

btcnew::send_block::send_block (btcnew::block_hash const & previous_a, btcnew::account const & destination_a, btcnew::amount const & balance_a, btcnew::raw_key const & prv_a, btcnew::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, destination_a, balance_a),
signature (btcnew::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

btcnew::send_block::send_block (bool & error_a, btcnew::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		try
		{
			btcnew::read (stream_a, signature.bytes);
			btcnew::read (stream_a, work);
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

btcnew::send_block::send_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto signature_l (tree_a.get<std::string> ("signature"));
			auto work_l (tree_a.get<std::string> ("work"));
			error_a = signature.decode_hex (signature_l);
			if (!error_a)
			{
				error_a = btcnew::from_string_hex (work_l, work);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

bool btcnew::send_block::operator== (btcnew::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool btcnew::send_block::valid_predecessor (btcnew::block const & block_a) const
{
	bool result;
	switch (block_a.type ())
	{
		case btcnew::block_type::send:
		case btcnew::block_type::receive:
		case btcnew::block_type::open:
		case btcnew::block_type::change:
			result = true;
			break;
		default:
			result = false;
			break;
	}
	return result;
}

btcnew::block_type btcnew::send_block::type () const
{
	return btcnew::block_type::send;
}

bool btcnew::send_block::operator== (btcnew::send_block const & other_a) const
{
	auto result (hashables.destination == other_a.hashables.destination && hashables.previous == other_a.hashables.previous && hashables.balance == other_a.hashables.balance && work == other_a.work && signature == other_a.signature);
	return result;
}

btcnew::block_hash const & btcnew::send_block::previous () const
{
	return hashables.previous;
}

btcnew::root const & btcnew::send_block::root () const
{
	return hashables.previous;
}

btcnew::amount const & btcnew::send_block::balance () const
{
	return hashables.balance;
}

btcnew::signature const & btcnew::send_block::block_signature () const
{
	return signature;
}

void btcnew::send_block::signature_set (btcnew::signature const & signature_a)
{
	signature = signature_a;
}

btcnew::open_hashables::open_hashables (btcnew::block_hash const & source_a, btcnew::account const & representative_a, btcnew::account const & account_a) :
source (source_a),
representative (representative_a),
account (account_a)
{
}

btcnew::open_hashables::open_hashables (bool & error_a, btcnew::stream & stream_a)
{
	try
	{
		btcnew::read (stream_a, source.bytes);
		btcnew::read (stream_a, representative.bytes);
		btcnew::read (stream_a, account.bytes);
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

btcnew::open_hashables::open_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto source_l (tree_a.get<std::string> ("source"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto account_l (tree_a.get<std::string> ("account"));
		error_a = source.decode_hex (source_l);
		if (!error_a)
		{
			error_a = representative.decode_account (representative_l);
			if (!error_a)
			{
				error_a = account.decode_account (account_l);
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void btcnew::open_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, source.bytes.data (), sizeof (source.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
	blake2b_update (&hash_a, account.bytes.data (), sizeof (account.bytes));
}

btcnew::open_block::open_block (btcnew::block_hash const & source_a, btcnew::account const & representative_a, btcnew::account const & account_a, btcnew::raw_key const & prv_a, btcnew::public_key const & pub_a, uint64_t work_a) :
hashables (source_a, representative_a, account_a),
signature (btcnew::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
	assert (!representative_a.is_zero ());
}

btcnew::open_block::open_block (btcnew::block_hash const & source_a, btcnew::account const & representative_a, btcnew::account const & account_a, std::nullptr_t) :
hashables (source_a, representative_a, account_a),
work (0)
{
	signature.clear ();
}

btcnew::open_block::open_block (bool & error_a, btcnew::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		try
		{
			btcnew::read (stream_a, signature);
			btcnew::read (stream_a, work);
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

btcnew::open_block::open_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto work_l (tree_a.get<std::string> ("work"));
			auto signature_l (tree_a.get<std::string> ("signature"));
			error_a = btcnew::from_string_hex (work_l, work);
			if (!error_a)
			{
				error_a = signature.decode_hex (signature_l);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void btcnew::open_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t btcnew::open_block::block_work () const
{
	return work;
}

void btcnew::open_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

btcnew::block_hash const & btcnew::open_block::previous () const
{
	static btcnew::block_hash result{ 0 };
	return result;
}

btcnew::account const & btcnew::open_block::account () const
{
	return hashables.account;
}

void btcnew::open_block::serialize (btcnew::stream & stream_a) const
{
	write (stream_a, hashables.source);
	write (stream_a, hashables.representative);
	write (stream_a, hashables.account);
	write (stream_a, signature);
	write (stream_a, work);
}

bool btcnew::open_block::deserialize (btcnew::stream & stream_a)
{
	auto error (false);
	try
	{
		read (stream_a, hashables.source);
		read (stream_a, hashables.representative);
		read (stream_a, hashables.account);
		read (stream_a, signature);
		read (stream_a, work);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

void btcnew::open_block::serialize_json (std::string & string_a, bool single_line) const
{
	boost::property_tree::ptree tree;
	serialize_json (tree);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree, !single_line);
	string_a = ostream.str ();
}

void btcnew::open_block::serialize_json (boost::property_tree::ptree & tree) const
{
	tree.put ("type", "open");
	tree.put ("source", hashables.source.to_string ());
	tree.put ("representative", representative ().to_account ());
	tree.put ("account", hashables.account.to_account ());
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", btcnew::to_string_hex (work));
	tree.put ("signature", signature_l);
}

bool btcnew::open_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "open");
		auto source_l (tree_a.get<std::string> ("source"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto account_l (tree_a.get<std::string> ("account"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.source.decode_hex (source_l);
		if (!error)
		{
			error = hashables.representative.decode_hex (representative_l);
			if (!error)
			{
				error = hashables.account.decode_hex (account_l);
				if (!error)
				{
					error = btcnew::from_string_hex (work_l, work);
					if (!error)
					{
						error = signature.decode_hex (signature_l);
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void btcnew::open_block::visit (btcnew::block_visitor & visitor_a) const
{
	visitor_a.open_block (*this);
}

btcnew::block_type btcnew::open_block::type () const
{
	return btcnew::block_type::open;
}

bool btcnew::open_block::operator== (btcnew::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool btcnew::open_block::operator== (btcnew::open_block const & other_a) const
{
	return hashables.source == other_a.hashables.source && hashables.representative == other_a.hashables.representative && hashables.account == other_a.hashables.account && work == other_a.work && signature == other_a.signature;
}

bool btcnew::open_block::valid_predecessor (btcnew::block const & block_a) const
{
	return false;
}

btcnew::block_hash const & btcnew::open_block::source () const
{
	return hashables.source;
}

btcnew::root const & btcnew::open_block::root () const
{
	return hashables.account;
}

btcnew::account const & btcnew::open_block::representative () const
{
	return hashables.representative;
}

btcnew::signature const & btcnew::open_block::block_signature () const
{
	return signature;
}

void btcnew::open_block::signature_set (btcnew::signature const & signature_a)
{
	signature = signature_a;
}

btcnew::change_hashables::change_hashables (btcnew::block_hash const & previous_a, btcnew::account const & representative_a) :
previous (previous_a),
representative (representative_a)
{
}

btcnew::change_hashables::change_hashables (bool & error_a, btcnew::stream & stream_a)
{
	try
	{
		btcnew::read (stream_a, previous);
		btcnew::read (stream_a, representative);
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

btcnew::change_hashables::change_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = representative.decode_account (representative_l);
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void btcnew::change_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
}

btcnew::change_block::change_block (btcnew::block_hash const & previous_a, btcnew::account const & representative_a, btcnew::raw_key const & prv_a, btcnew::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, representative_a),
signature (btcnew::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

btcnew::change_block::change_block (bool & error_a, btcnew::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		try
		{
			btcnew::read (stream_a, signature);
			btcnew::read (stream_a, work);
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

btcnew::change_block::change_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto work_l (tree_a.get<std::string> ("work"));
			auto signature_l (tree_a.get<std::string> ("signature"));
			error_a = btcnew::from_string_hex (work_l, work);
			if (!error_a)
			{
				error_a = signature.decode_hex (signature_l);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void btcnew::change_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t btcnew::change_block::block_work () const
{
	return work;
}

void btcnew::change_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

btcnew::block_hash const & btcnew::change_block::previous () const
{
	return hashables.previous;
}

void btcnew::change_block::serialize (btcnew::stream & stream_a) const
{
	write (stream_a, hashables.previous);
	write (stream_a, hashables.representative);
	write (stream_a, signature);
	write (stream_a, work);
}

bool btcnew::change_block::deserialize (btcnew::stream & stream_a)
{
	auto error (false);
	try
	{
		read (stream_a, hashables.previous);
		read (stream_a, hashables.representative);
		read (stream_a, signature);
		read (stream_a, work);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

void btcnew::change_block::serialize_json (std::string & string_a, bool single_line) const
{
	boost::property_tree::ptree tree;
	serialize_json (tree);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree, !single_line);
	string_a = ostream.str ();
}

void btcnew::change_block::serialize_json (boost::property_tree::ptree & tree) const
{
	tree.put ("type", "change");
	tree.put ("previous", hashables.previous.to_string ());
	tree.put ("representative", representative ().to_account ());
	tree.put ("work", btcnew::to_string_hex (work));
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("signature", signature_l);
}

bool btcnew::change_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "change");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.representative.decode_hex (representative_l);
			if (!error)
			{
				error = btcnew::from_string_hex (work_l, work);
				if (!error)
				{
					error = signature.decode_hex (signature_l);
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void btcnew::change_block::visit (btcnew::block_visitor & visitor_a) const
{
	visitor_a.change_block (*this);
}

btcnew::block_type btcnew::change_block::type () const
{
	return btcnew::block_type::change;
}

bool btcnew::change_block::operator== (btcnew::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool btcnew::change_block::operator== (btcnew::change_block const & other_a) const
{
	return hashables.previous == other_a.hashables.previous && hashables.representative == other_a.hashables.representative && work == other_a.work && signature == other_a.signature;
}

bool btcnew::change_block::valid_predecessor (btcnew::block const & block_a) const
{
	bool result;
	switch (block_a.type ())
	{
		case btcnew::block_type::send:
		case btcnew::block_type::receive:
		case btcnew::block_type::open:
		case btcnew::block_type::change:
			result = true;
			break;
		default:
			result = false;
			break;
	}
	return result;
}

btcnew::root const & btcnew::change_block::root () const
{
	return hashables.previous;
}

btcnew::account const & btcnew::change_block::representative () const
{
	return hashables.representative;
}

btcnew::signature const & btcnew::change_block::block_signature () const
{
	return signature;
}

void btcnew::change_block::signature_set (btcnew::signature const & signature_a)
{
	signature = signature_a;
}

btcnew::state_hashables::state_hashables (btcnew::account const & account_a, btcnew::block_hash const & previous_a, btcnew::account const & representative_a, btcnew::amount const & balance_a, btcnew::link const & link_a) :
account (account_a),
previous (previous_a),
representative (representative_a),
balance (balance_a),
link (link_a)
{
}

btcnew::state_hashables::state_hashables (bool & error_a, btcnew::stream & stream_a)
{
	try
	{
		btcnew::read (stream_a, account);
		btcnew::read (stream_a, previous);
		btcnew::read (stream_a, representative);
		btcnew::read (stream_a, balance);
		btcnew::read (stream_a, link);
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

btcnew::state_hashables::state_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto account_l (tree_a.get<std::string> ("account"));
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		auto link_l (tree_a.get<std::string> ("link"));
		error_a = account.decode_account (account_l);
		if (!error_a)
		{
			error_a = previous.decode_hex (previous_l);
			if (!error_a)
			{
				error_a = representative.decode_account (representative_l);
				if (!error_a)
				{
					error_a = balance.decode_dec (balance_l);
					if (!error_a)
					{
						error_a = link.decode_account (link_l) && link.decode_hex (link_l);
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void btcnew::state_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, account.bytes.data (), sizeof (account.bytes));
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
	blake2b_update (&hash_a, balance.bytes.data (), sizeof (balance.bytes));
	blake2b_update (&hash_a, link.bytes.data (), sizeof (link.bytes));
}

btcnew::state_block::state_block (btcnew::account const & account_a, btcnew::block_hash const & previous_a, btcnew::account const & representative_a, btcnew::amount const & balance_a, btcnew::link const & link_a, btcnew::raw_key const & prv_a, btcnew::public_key const & pub_a, uint64_t work_a) :
hashables (account_a, previous_a, representative_a, balance_a, link_a),
signature (btcnew::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

btcnew::state_block::state_block (bool & error_a, btcnew::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		try
		{
			btcnew::read (stream_a, signature);
			btcnew::read (stream_a, work);
			boost::endian::big_to_native_inplace (work);
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

btcnew::state_block::state_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto type_l (tree_a.get<std::string> ("type"));
			auto signature_l (tree_a.get<std::string> ("signature"));
			auto work_l (tree_a.get<std::string> ("work"));
			error_a = type_l != "state";
			if (!error_a)
			{
				error_a = btcnew::from_string_hex (work_l, work);
				if (!error_a)
				{
					error_a = signature.decode_hex (signature_l);
				}
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void btcnew::state_block::hash (blake2b_state & hash_a) const
{
	btcnew::uint256_union preamble (static_cast<uint64_t> (btcnew::block_type::state));
	blake2b_update (&hash_a, preamble.bytes.data (), preamble.bytes.size ());
	hashables.hash (hash_a);
}

uint64_t btcnew::state_block::block_work () const
{
	return work;
}

void btcnew::state_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

btcnew::block_hash const & btcnew::state_block::previous () const
{
	return hashables.previous;
}

btcnew::account const & btcnew::state_block::account () const
{
	return hashables.account;
}

void btcnew::state_block::serialize (btcnew::stream & stream_a) const
{
	write (stream_a, hashables.account);
	write (stream_a, hashables.previous);
	write (stream_a, hashables.representative);
	write (stream_a, hashables.balance);
	write (stream_a, hashables.link);
	write (stream_a, signature);
	write (stream_a, boost::endian::native_to_big (work));
}

bool btcnew::state_block::deserialize (btcnew::stream & stream_a)
{
	auto error (false);
	try
	{
		read (stream_a, hashables.account);
		read (stream_a, hashables.previous);
		read (stream_a, hashables.representative);
		read (stream_a, hashables.balance);
		read (stream_a, hashables.link);
		read (stream_a, signature);
		read (stream_a, work);
		boost::endian::big_to_native_inplace (work);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

void btcnew::state_block::serialize_json (std::string & string_a, bool single_line) const
{
	boost::property_tree::ptree tree;
	serialize_json (tree);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree, !single_line);
	string_a = ostream.str ();
}

void btcnew::state_block::serialize_json (boost::property_tree::ptree & tree) const
{
	tree.put ("type", "state");
	tree.put ("account", hashables.account.to_account ());
	tree.put ("previous", hashables.previous.to_string ());
	tree.put ("representative", representative ().to_account ());
	tree.put ("balance", hashables.balance.to_string_dec ());
	tree.put ("link", hashables.link.to_string ());
	tree.put ("link_as_account", hashables.link.to_account ());
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("signature", signature_l);
	tree.put ("work", btcnew::to_string_hex (work));
}

bool btcnew::state_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "state");
		auto account_l (tree_a.get<std::string> ("account"));
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto representative_l (tree_a.get<std::string> ("representative"));
		auto balance_l (tree_a.get<std::string> ("balance"));
		auto link_l (tree_a.get<std::string> ("link"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.account.decode_account (account_l);
		if (!error)
		{
			error = hashables.previous.decode_hex (previous_l);
			if (!error)
			{
				error = hashables.representative.decode_account (representative_l);
				if (!error)
				{
					error = hashables.balance.decode_dec (balance_l);
					if (!error)
					{
						error = hashables.link.decode_account (link_l) && hashables.link.decode_hex (link_l);
						if (!error)
						{
							error = btcnew::from_string_hex (work_l, work);
							if (!error)
							{
								error = signature.decode_hex (signature_l);
							}
						}
					}
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

void btcnew::state_block::visit (btcnew::block_visitor & visitor_a) const
{
	visitor_a.state_block (*this);
}

btcnew::block_type btcnew::state_block::type () const
{
	return btcnew::block_type::state;
}

bool btcnew::state_block::operator== (btcnew::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool btcnew::state_block::operator== (btcnew::state_block const & other_a) const
{
	return hashables.account == other_a.hashables.account && hashables.previous == other_a.hashables.previous && hashables.representative == other_a.hashables.representative && hashables.balance == other_a.hashables.balance && hashables.link == other_a.hashables.link && signature == other_a.signature && work == other_a.work;
}

bool btcnew::state_block::valid_predecessor (btcnew::block const & block_a) const
{
	return true;
}

btcnew::root const & btcnew::state_block::root () const
{
	if (!hashables.previous.is_zero ())
	{
		return hashables.previous;
	}
	else
	{
		return hashables.account;
	}
}

btcnew::link const & btcnew::state_block::link () const
{
	return hashables.link;
}

btcnew::account const & btcnew::state_block::representative () const
{
	return hashables.representative;
}

btcnew::amount const & btcnew::state_block::balance () const
{
	return hashables.balance;
}

btcnew::signature const & btcnew::state_block::block_signature () const
{
	return signature;
}

void btcnew::state_block::signature_set (btcnew::signature const & signature_a)
{
	signature = signature_a;
}

std::shared_ptr<btcnew::block> btcnew::deserialize_block_json (boost::property_tree::ptree const & tree_a, btcnew::block_uniquer * uniquer_a)
{
	std::shared_ptr<btcnew::block> result;
	try
	{
		auto type (tree_a.get<std::string> ("type"));
		if (type == "receive")
		{
			bool error (false);
			std::unique_ptr<btcnew::receive_block> obj (new btcnew::receive_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
		else if (type == "send")
		{
			bool error (false);
			std::unique_ptr<btcnew::send_block> obj (new btcnew::send_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
		else if (type == "open")
		{
			bool error (false);
			std::unique_ptr<btcnew::open_block> obj (new btcnew::open_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
		else if (type == "change")
		{
			bool error (false);
			std::unique_ptr<btcnew::change_block> obj (new btcnew::change_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
		else if (type == "state")
		{
			bool error (false);
			std::unique_ptr<btcnew::state_block> obj (new btcnew::state_block (error, tree_a));
			if (!error)
			{
				result = std::move (obj);
			}
		}
	}
	catch (std::runtime_error const &)
	{
	}
	if (uniquer_a != nullptr)
	{
		result = uniquer_a->unique (result);
	}
	return result;
}

std::shared_ptr<btcnew::block> btcnew::deserialize_block (btcnew::stream & stream_a)
{
	btcnew::block_type type;
	auto error (try_read (stream_a, type));
	std::shared_ptr<btcnew::block> result;
	if (!error)
	{
		result = btcnew::deserialize_block (stream_a, type);
	}
	return result;
}

std::shared_ptr<btcnew::block> btcnew::deserialize_block (btcnew::stream & stream_a, btcnew::block_type type_a, btcnew::block_uniquer * uniquer_a)
{
	std::shared_ptr<btcnew::block> result;
	switch (type_a)
	{
		case btcnew::block_type::receive: {
			result = ::deserialize_block<btcnew::receive_block> (stream_a);
			break;
		}
		case btcnew::block_type::send: {
			result = ::deserialize_block<btcnew::send_block> (stream_a);
			break;
		}
		case btcnew::block_type::open: {
			result = ::deserialize_block<btcnew::open_block> (stream_a);
			break;
		}
		case btcnew::block_type::change: {
			result = ::deserialize_block<btcnew::change_block> (stream_a);
			break;
		}
		case btcnew::block_type::state: {
			result = ::deserialize_block<btcnew::state_block> (stream_a);
			break;
		}
		default:
			assert (false);
			break;
	}
	if (uniquer_a != nullptr)
	{
		result = uniquer_a->unique (result);
	}
	return result;
}

void btcnew::receive_block::visit (btcnew::block_visitor & visitor_a) const
{
	visitor_a.receive_block (*this);
}

bool btcnew::receive_block::operator== (btcnew::receive_block const & other_a) const
{
	auto result (hashables.previous == other_a.hashables.previous && hashables.source == other_a.hashables.source && work == other_a.work && signature == other_a.signature);
	return result;
}

void btcnew::receive_block::serialize (btcnew::stream & stream_a) const
{
	write (stream_a, hashables.previous.bytes);
	write (stream_a, hashables.source.bytes);
	write (stream_a, signature.bytes);
	write (stream_a, work);
}

bool btcnew::receive_block::deserialize (btcnew::stream & stream_a)
{
	auto error (false);
	try
	{
		read (stream_a, hashables.previous.bytes);
		read (stream_a, hashables.source.bytes);
		read (stream_a, signature.bytes);
		read (stream_a, work);
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}

	return error;
}

void btcnew::receive_block::serialize_json (std::string & string_a, bool single_line) const
{
	boost::property_tree::ptree tree;
	serialize_json (tree);
	std::stringstream ostream;
	boost::property_tree::write_json (ostream, tree, !single_line);
	string_a = ostream.str ();
}

void btcnew::receive_block::serialize_json (boost::property_tree::ptree & tree) const
{
	tree.put ("type", "receive");
	std::string previous;
	hashables.previous.encode_hex (previous);
	tree.put ("previous", previous);
	std::string source;
	hashables.source.encode_hex (source);
	tree.put ("source", source);
	std::string signature_l;
	signature.encode_hex (signature_l);
	tree.put ("work", btcnew::to_string_hex (work));
	tree.put ("signature", signature_l);
}

bool btcnew::receive_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
	auto error (false);
	try
	{
		assert (tree_a.get<std::string> ("type") == "receive");
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto source_l (tree_a.get<std::string> ("source"));
		auto work_l (tree_a.get<std::string> ("work"));
		auto signature_l (tree_a.get<std::string> ("signature"));
		error = hashables.previous.decode_hex (previous_l);
		if (!error)
		{
			error = hashables.source.decode_hex (source_l);
			if (!error)
			{
				error = btcnew::from_string_hex (work_l, work);
				if (!error)
				{
					error = signature.decode_hex (signature_l);
				}
			}
		}
	}
	catch (std::runtime_error const &)
	{
		error = true;
	}
	return error;
}

btcnew::receive_block::receive_block (btcnew::block_hash const & previous_a, btcnew::block_hash const & source_a, btcnew::raw_key const & prv_a, btcnew::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, source_a),
signature (btcnew::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

btcnew::receive_block::receive_block (bool & error_a, btcnew::stream & stream_a) :
hashables (error_a, stream_a)
{
	if (!error_a)
	{
		try
		{
			btcnew::read (stream_a, signature);
			btcnew::read (stream_a, work);
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

btcnew::receive_block::receive_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
	if (!error_a)
	{
		try
		{
			auto signature_l (tree_a.get<std::string> ("signature"));
			auto work_l (tree_a.get<std::string> ("work"));
			error_a = signature.decode_hex (signature_l);
			if (!error_a)
			{
				error_a = btcnew::from_string_hex (work_l, work);
			}
		}
		catch (std::runtime_error const &)
		{
			error_a = true;
		}
	}
}

void btcnew::receive_block::hash (blake2b_state & hash_a) const
{
	hashables.hash (hash_a);
}

uint64_t btcnew::receive_block::block_work () const
{
	return work;
}

void btcnew::receive_block::block_work_set (uint64_t work_a)
{
	work = work_a;
}

bool btcnew::receive_block::operator== (btcnew::block const & other_a) const
{
	return blocks_equal (*this, other_a);
}

bool btcnew::receive_block::valid_predecessor (btcnew::block const & block_a) const
{
	bool result;
	switch (block_a.type ())
	{
		case btcnew::block_type::send:
		case btcnew::block_type::receive:
		case btcnew::block_type::open:
		case btcnew::block_type::change:
			result = true;
			break;
		default:
			result = false;
			break;
	}
	return result;
}

btcnew::block_hash const & btcnew::receive_block::previous () const
{
	return hashables.previous;
}

btcnew::block_hash const & btcnew::receive_block::source () const
{
	return hashables.source;
}

btcnew::root const & btcnew::receive_block::root () const
{
	return hashables.previous;
}

btcnew::signature const & btcnew::receive_block::block_signature () const
{
	return signature;
}

void btcnew::receive_block::signature_set (btcnew::signature const & signature_a)
{
	signature = signature_a;
}

btcnew::block_type btcnew::receive_block::type () const
{
	return btcnew::block_type::receive;
}

btcnew::receive_hashables::receive_hashables (btcnew::block_hash const & previous_a, btcnew::block_hash const & source_a) :
previous (previous_a),
source (source_a)
{
}

btcnew::receive_hashables::receive_hashables (bool & error_a, btcnew::stream & stream_a)
{
	try
	{
		btcnew::read (stream_a, previous.bytes);
		btcnew::read (stream_a, source.bytes);
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

btcnew::receive_hashables::receive_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
	try
	{
		auto previous_l (tree_a.get<std::string> ("previous"));
		auto source_l (tree_a.get<std::string> ("source"));
		error_a = previous.decode_hex (previous_l);
		if (!error_a)
		{
			error_a = source.decode_hex (source_l);
		}
	}
	catch (std::runtime_error const &)
	{
		error_a = true;
	}
}

void btcnew::receive_hashables::hash (blake2b_state & hash_a) const
{
	blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
	blake2b_update (&hash_a, source.bytes.data (), sizeof (source.bytes));
}

std::shared_ptr<btcnew::block> btcnew::block_uniquer::unique (std::shared_ptr<btcnew::block> block_a)
{
	auto result (block_a);
	if (result != nullptr)
	{
		btcnew::uint256_union key (block_a->full_hash ());
		btcnew::lock_guard<std::mutex> lock (mutex);
		auto & existing (blocks[key]);
		if (auto block_l = existing.lock ())
		{
			result = block_l;
		}
		else
		{
			existing = block_a;
		}
		release_assert (std::numeric_limits<CryptoPP::word32>::max () > blocks.size ());
		for (auto i (0); i < cleanup_count && !blocks.empty (); ++i)
		{
			auto random_offset (btcnew::random_pool::generate_word32 (0, static_cast<CryptoPP::word32> (blocks.size () - 1)));
			auto existing (std::next (blocks.begin (), random_offset));
			if (existing == blocks.end ())
			{
				existing = blocks.begin ();
			}
			if (existing != blocks.end ())
			{
				if (auto block_l = existing->second.lock ())
				{
					// Still live
				}
				else
				{
					blocks.erase (existing);
				}
			}
		}
	}
	return result;
}

size_t btcnew::block_uniquer::size ()
{
	btcnew::lock_guard<std::mutex> lock (mutex);
	return blocks.size ();
}

namespace btcnew
{
std::unique_ptr<seq_con_info_component> collect_seq_con_info (block_uniquer & block_uniquer, const std::string & name)
{
	auto count = block_uniquer.size ();
	auto sizeof_element = sizeof (block_uniquer::value_type);
	auto composite = std::make_unique<seq_con_info_composite> (name);
	composite->add_component (std::make_unique<seq_con_info_leaf> (seq_con_info{ "blocks", count, sizeof_element }));
	return composite;
}
}

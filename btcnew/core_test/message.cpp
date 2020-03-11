#include <btcnew/node/common.hpp>

#include <gtest/gtest.h>

TEST (message, keepalive_serialization)
{
	btcnew::keepalive request1;
	std::vector<uint8_t> bytes;
	{
		btcnew::vectorstream stream (bytes);
		request1.serialize (stream);
	}
	auto error (false);
	btcnew::bufferstream stream (bytes.data (), bytes.size ());
	btcnew::message_header header (error, stream);
	ASSERT_FALSE (error);
	btcnew::keepalive request2 (error, stream, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (request1, request2);
}

TEST (message, keepalive_deserialize)
{
	btcnew::keepalive message1;
	message1.peers[0] = btcnew::endpoint (boost::asio::ip::address_v6::loopback (), 10000);
	std::vector<uint8_t> bytes;
	{
		btcnew::vectorstream stream (bytes);
		message1.serialize (stream);
	}
	btcnew::bufferstream stream (bytes.data (), bytes.size ());
	auto error (false);
	btcnew::message_header header (error, stream);
	ASSERT_FALSE (error);
	ASSERT_EQ (btcnew::message_type::keepalive, header.type);
	btcnew::keepalive message2 (error, stream, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (message1.peers, message2.peers);
}

TEST (message, publish_serialization)
{
	btcnew::network_params params;
	btcnew::publish publish (std::make_shared<btcnew::send_block> (0, 1, 2, btcnew::keypair ().prv, 4, 5));
	ASSERT_EQ (btcnew::block_type::send, publish.header.block_type ());
	std::vector<uint8_t> bytes;
	{
		btcnew::vectorstream stream (bytes);
		publish.header.serialize (stream);
	}
	ASSERT_EQ (8, bytes.size ());
	ASSERT_EQ (0x52, bytes[0]);
	ASSERT_EQ (0x41, bytes[1]);
	ASSERT_EQ (params.protocol.protocol_version, bytes[2]);
	ASSERT_EQ (params.protocol.protocol_version, bytes[3]);
	ASSERT_EQ (params.protocol.protocol_version_min, bytes[4]);
	ASSERT_EQ (static_cast<uint8_t> (btcnew::message_type::publish), bytes[5]);
	ASSERT_EQ (0x00, bytes[6]); // extensions
	ASSERT_EQ (static_cast<uint8_t> (btcnew::block_type::send), bytes[7]);
	btcnew::bufferstream stream (bytes.data (), bytes.size ());
	auto error (false);
	btcnew::message_header header (error, stream);
	ASSERT_FALSE (error);
	ASSERT_EQ (params.protocol.protocol_version_min, header.version_min);
	ASSERT_EQ (params.protocol.protocol_version, header.version_using);
	ASSERT_EQ (params.protocol.protocol_version, header.version_max);
	ASSERT_EQ (btcnew::message_type::publish, header.type);
}

TEST (message, confirm_ack_serialization)
{
	btcnew::keypair key1;
	auto vote (std::make_shared<btcnew::vote> (key1.pub, key1.prv, 0, std::make_shared<btcnew::send_block> (0, 1, 2, key1.prv, 4, 5)));
	btcnew::confirm_ack con1 (vote);
	std::vector<uint8_t> bytes;
	{
		btcnew::vectorstream stream1 (bytes);
		con1.serialize (stream1);
	}
	btcnew::bufferstream stream2 (bytes.data (), bytes.size ());
	bool error (false);
	btcnew::message_header header (error, stream2);
	btcnew::confirm_ack con2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (con1, con2);
	ASSERT_EQ (header.block_type (), btcnew::block_type::send);
}

TEST (message, confirm_ack_hash_serialization)
{
	std::vector<btcnew::block_hash> hashes;
	for (auto i (hashes.size ()); i < 12; i++)
	{
		btcnew::keypair key1;
		btcnew::block_hash previous;
		btcnew::random_pool::generate_block (previous.bytes.data (), previous.bytes.size ());
		btcnew::state_block block (key1.pub, previous, key1.pub, 2, 4, key1.prv, key1.pub, 5);
		hashes.push_back (block.hash ());
	}
	btcnew::keypair representative1;
	auto vote (std::make_shared<btcnew::vote> (representative1.pub, representative1.prv, 0, hashes));
	btcnew::confirm_ack con1 (vote);
	std::vector<uint8_t> bytes;
	{
		btcnew::vectorstream stream1 (bytes);
		con1.serialize (stream1);
	}
	btcnew::bufferstream stream2 (bytes.data (), bytes.size ());
	bool error (false);
	btcnew::message_header header (error, stream2);
	btcnew::confirm_ack con2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (con1, con2);
	std::vector<btcnew::block_hash> vote_blocks;
	for (auto block : con2.vote->blocks)
	{
		vote_blocks.push_back (boost::get<btcnew::block_hash> (block));
	}
	ASSERT_EQ (hashes, vote_blocks);
	// Check overflow with 12 hashes
	ASSERT_EQ (header.count_get (), hashes.size ());
	ASSERT_EQ (header.block_type (), btcnew::block_type::not_a_block);
}

TEST (message, confirm_req_serialization)
{
	btcnew::keypair key1;
	btcnew::keypair key2;
	auto block (std::make_shared<btcnew::send_block> (0, key2.pub, 200, btcnew::keypair ().prv, 2, 3));
	btcnew::confirm_req req (block);
	std::vector<uint8_t> bytes;
	{
		btcnew::vectorstream stream (bytes);
		req.serialize (stream);
	}
	auto error (false);
	btcnew::bufferstream stream2 (bytes.data (), bytes.size ());
	btcnew::message_header header (error, stream2);
	btcnew::confirm_req req2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (req, req2);
	ASSERT_EQ (*req.block, *req2.block);
}

TEST (message, confirm_req_hash_serialization)
{
	btcnew::keypair key1;
	btcnew::keypair key2;
	btcnew::send_block block (1, key2.pub, 200, btcnew::keypair ().prv, 2, 3);
	btcnew::confirm_req req (block.hash (), block.root ());
	std::vector<uint8_t> bytes;
	{
		btcnew::vectorstream stream (bytes);
		req.serialize (stream);
	}
	auto error (false);
	btcnew::bufferstream stream2 (bytes.data (), bytes.size ());
	btcnew::message_header header (error, stream2);
	btcnew::confirm_req req2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (req, req2);
	ASSERT_EQ (req.roots_hashes, req2.roots_hashes);
	ASSERT_EQ (header.block_type (), btcnew::block_type::not_a_block);
	ASSERT_EQ (header.count_get (), req.roots_hashes.size ());
}

TEST (message, confirm_req_hash_batch_serialization)
{
	btcnew::keypair key;
	btcnew::keypair representative;
	std::vector<std::pair<btcnew::block_hash, btcnew::root>> roots_hashes;
	btcnew::state_block open (key.pub, 0, representative.pub, 2, 4, key.prv, key.pub, 5);
	roots_hashes.push_back (std::make_pair (open.hash (), open.root ()));
	for (auto i (roots_hashes.size ()); i < 7; i++)
	{
		btcnew::keypair key1;
		btcnew::block_hash previous;
		btcnew::random_pool::generate_block (previous.bytes.data (), previous.bytes.size ());
		btcnew::state_block block (key1.pub, previous, representative.pub, 2, 4, key1.prv, key1.pub, 5);
		roots_hashes.push_back (std::make_pair (block.hash (), block.root ()));
	}
	roots_hashes.push_back (std::make_pair (open.hash (), open.root ()));
	btcnew::confirm_req req (roots_hashes);
	std::vector<uint8_t> bytes;
	{
		btcnew::vectorstream stream (bytes);
		req.serialize (stream);
	}
	auto error (false);
	btcnew::bufferstream stream2 (bytes.data (), bytes.size ());
	btcnew::message_header header (error, stream2);
	btcnew::confirm_req req2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (req, req2);
	ASSERT_EQ (req.roots_hashes, req2.roots_hashes);
	ASSERT_EQ (req.roots_hashes, roots_hashes);
	ASSERT_EQ (req2.roots_hashes, roots_hashes);
	ASSERT_EQ (header.block_type (), btcnew::block_type::not_a_block);
	ASSERT_EQ (header.count_get (), req.roots_hashes.size ());
}

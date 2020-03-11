#include <btcnew/node/testing.hpp>

#include <gtest/gtest.h>

namespace
{
class test_visitor : public btcnew::message_visitor
{
public:
	void keepalive (btcnew::keepalive const &) override
	{
		++keepalive_count;
	}
	void publish (btcnew::publish const &) override
	{
		++publish_count;
	}
	void confirm_req (btcnew::confirm_req const &) override
	{
		++confirm_req_count;
	}
	void confirm_ack (btcnew::confirm_ack const &) override
	{
		++confirm_ack_count;
	}
	void bulk_pull (btcnew::bulk_pull const &) override
	{
		++bulk_pull_count;
	}
	void bulk_pull_account (btcnew::bulk_pull_account const &) override
	{
		++bulk_pull_account_count;
	}
	void bulk_push (btcnew::bulk_push const &) override
	{
		++bulk_push_count;
	}
	void frontier_req (btcnew::frontier_req const &) override
	{
		++frontier_req_count;
	}
	void node_id_handshake (btcnew::node_id_handshake const &) override
	{
		++node_id_handshake_count;
	}
	uint64_t keepalive_count{ 0 };
	uint64_t publish_count{ 0 };
	uint64_t confirm_req_count{ 0 };
	uint64_t confirm_ack_count{ 0 };
	uint64_t bulk_pull_count{ 0 };
	uint64_t bulk_pull_account_count{ 0 };
	uint64_t bulk_push_count{ 0 };
	uint64_t frontier_req_count{ 0 };
	uint64_t node_id_handshake_count{ 0 };
};
}

TEST (message_parser, exact_confirm_ack_size)
{
	btcnew::system system (24000, 1);
	test_visitor visitor;
	btcnew::block_uniquer block_uniquer;
	btcnew::vote_uniquer vote_uniquer (block_uniquer);
	btcnew::message_parser parser (block_uniquer, vote_uniquer, visitor, system.work);
	auto block (std::make_shared<btcnew::send_block> (1, 1, 2, btcnew::keypair ().prv, 4, *system.work.generate (btcnew::root (1))));
	auto vote (std::make_shared<btcnew::vote> (0, btcnew::keypair ().prv, 0, std::move (block)));
	btcnew::confirm_ack message (vote);
	std::vector<uint8_t> bytes;
	{
		btcnew::vectorstream stream (bytes);
		message.serialize (stream);
	}
	ASSERT_EQ (0, visitor.confirm_ack_count);
	ASSERT_EQ (parser.status, btcnew::message_parser::parse_status::success);
	auto error (false);
	btcnew::bufferstream stream1 (bytes.data (), bytes.size ());
	btcnew::message_header header1 (error, stream1);
	ASSERT_FALSE (error);
	parser.deserialize_confirm_ack (stream1, header1);
	ASSERT_EQ (1, visitor.confirm_ack_count);
	ASSERT_EQ (parser.status, btcnew::message_parser::parse_status::success);
	bytes.push_back (0);
	btcnew::bufferstream stream2 (bytes.data (), bytes.size ());
	btcnew::message_header header2 (error, stream2);
	ASSERT_FALSE (error);
	parser.deserialize_confirm_ack (stream2, header2);
	ASSERT_EQ (1, visitor.confirm_ack_count);
	ASSERT_NE (parser.status, btcnew::message_parser::parse_status::success);
}

TEST (message_parser, exact_confirm_req_size)
{
	btcnew::system system (24000, 1);
	test_visitor visitor;
	btcnew::block_uniquer block_uniquer;
	btcnew::vote_uniquer vote_uniquer (block_uniquer);
	btcnew::message_parser parser (block_uniquer, vote_uniquer, visitor, system.work);
	auto block (std::make_shared<btcnew::send_block> (1, 1, 2, btcnew::keypair ().prv, 4, *system.work.generate (btcnew::root (1))));
	btcnew::confirm_req message (std::move (block));
	std::vector<uint8_t> bytes;
	{
		btcnew::vectorstream stream (bytes);
		message.serialize (stream);
	}
	ASSERT_EQ (0, visitor.confirm_req_count);
	ASSERT_EQ (parser.status, btcnew::message_parser::parse_status::success);
	auto error (false);
	btcnew::bufferstream stream1 (bytes.data (), bytes.size ());
	btcnew::message_header header1 (error, stream1);
	ASSERT_FALSE (error);
	parser.deserialize_confirm_req (stream1, header1);
	ASSERT_EQ (1, visitor.confirm_req_count);
	ASSERT_EQ (parser.status, btcnew::message_parser::parse_status::success);
	bytes.push_back (0);
	btcnew::bufferstream stream2 (bytes.data (), bytes.size ());
	btcnew::message_header header2 (error, stream2);
	ASSERT_FALSE (error);
	parser.deserialize_confirm_req (stream2, header2);
	ASSERT_EQ (1, visitor.confirm_req_count);
	ASSERT_NE (parser.status, btcnew::message_parser::parse_status::success);
}

TEST (message_parser, exact_confirm_req_hash_size)
{
	btcnew::system system (24000, 1);
	test_visitor visitor;
	btcnew::block_uniquer block_uniquer;
	btcnew::vote_uniquer vote_uniquer (block_uniquer);
	btcnew::message_parser parser (block_uniquer, vote_uniquer, visitor, system.work);
	btcnew::send_block block (1, 1, 2, btcnew::keypair ().prv, 4, *system.work.generate (btcnew::root (1)));
	btcnew::confirm_req message (block.hash (), block.root ());
	std::vector<uint8_t> bytes;
	{
		btcnew::vectorstream stream (bytes);
		message.serialize (stream);
	}
	ASSERT_EQ (0, visitor.confirm_req_count);
	ASSERT_EQ (parser.status, btcnew::message_parser::parse_status::success);
	auto error (false);
	btcnew::bufferstream stream1 (bytes.data (), bytes.size ());
	btcnew::message_header header1 (error, stream1);
	ASSERT_FALSE (error);
	parser.deserialize_confirm_req (stream1, header1);
	ASSERT_EQ (1, visitor.confirm_req_count);
	ASSERT_EQ (parser.status, btcnew::message_parser::parse_status::success);
	bytes.push_back (0);
	btcnew::bufferstream stream2 (bytes.data (), bytes.size ());
	btcnew::message_header header2 (error, stream2);
	ASSERT_FALSE (error);
	parser.deserialize_confirm_req (stream2, header2);
	ASSERT_EQ (1, visitor.confirm_req_count);
	ASSERT_NE (parser.status, btcnew::message_parser::parse_status::success);
}

TEST (message_parser, exact_publish_size)
{
	btcnew::system system (24000, 1);
	test_visitor visitor;
	btcnew::block_uniquer block_uniquer;
	btcnew::vote_uniquer vote_uniquer (block_uniquer);
	btcnew::message_parser parser (block_uniquer, vote_uniquer, visitor, system.work);
	auto block (std::make_shared<btcnew::send_block> (1, 1, 2, btcnew::keypair ().prv, 4, *system.work.generate (btcnew::root (1))));
	btcnew::publish message (std::move (block));
	std::vector<uint8_t> bytes;
	{
		btcnew::vectorstream stream (bytes);
		message.serialize (stream);
	}
	ASSERT_EQ (0, visitor.publish_count);
	ASSERT_EQ (parser.status, btcnew::message_parser::parse_status::success);
	auto error (false);
	btcnew::bufferstream stream1 (bytes.data (), bytes.size ());
	btcnew::message_header header1 (error, stream1);
	ASSERT_FALSE (error);
	parser.deserialize_publish (stream1, header1);
	ASSERT_EQ (1, visitor.publish_count);
	ASSERT_EQ (parser.status, btcnew::message_parser::parse_status::success);
	bytes.push_back (0);
	btcnew::bufferstream stream2 (bytes.data (), bytes.size ());
	btcnew::message_header header2 (error, stream2);
	ASSERT_FALSE (error);
	parser.deserialize_publish (stream2, header2);
	ASSERT_EQ (1, visitor.publish_count);
	ASSERT_NE (parser.status, btcnew::message_parser::parse_status::success);
}

TEST (message_parser, exact_keepalive_size)
{
	btcnew::system system (24000, 1);
	test_visitor visitor;
	btcnew::block_uniquer block_uniquer;
	btcnew::vote_uniquer vote_uniquer (block_uniquer);
	btcnew::message_parser parser (block_uniquer, vote_uniquer, visitor, system.work);
	btcnew::keepalive message;
	std::vector<uint8_t> bytes;
	{
		btcnew::vectorstream stream (bytes);
		message.serialize (stream);
	}
	ASSERT_EQ (0, visitor.keepalive_count);
	ASSERT_EQ (parser.status, btcnew::message_parser::parse_status::success);
	auto error (false);
	btcnew::bufferstream stream1 (bytes.data (), bytes.size ());
	btcnew::message_header header1 (error, stream1);
	ASSERT_FALSE (error);
	parser.deserialize_keepalive (stream1, header1);
	ASSERT_EQ (1, visitor.keepalive_count);
	ASSERT_EQ (parser.status, btcnew::message_parser::parse_status::success);
	bytes.push_back (0);
	btcnew::bufferstream stream2 (bytes.data (), bytes.size ());
	btcnew::message_header header2 (error, stream2);
	ASSERT_FALSE (error);
	parser.deserialize_keepalive (stream2, header2);
	ASSERT_EQ (1, visitor.keepalive_count);
	ASSERT_NE (parser.status, btcnew::message_parser::parse_status::success);
}

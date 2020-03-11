#include <btcnew/node/common.hpp>

#include <gtest/gtest.h>
namespace btcnew
{
void cleanup_test_directories_on_exit ();
void force_btcnew_test_network ();
}

int main (int argc, char ** argv)
{
	btcnew::force_btcnew_test_network ();
	btcnew::node_singleton_memory_pool_purge_guard memory_pool_cleanup_guard;
	testing::InitGoogleTest (&argc, argv);
	auto res = RUN_ALL_TESTS ();
	btcnew::cleanup_test_directories_on_exit ();
	return res;
}

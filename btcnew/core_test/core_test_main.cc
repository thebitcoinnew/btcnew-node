#include "gtest/gtest.h"

#include <btcnew/node/common.hpp>
#include <btcnew/node/logging.hpp>

namespace btcnew
{
void cleanup_test_directories_on_exit ();
void force_btcnew_test_network ();
}

GTEST_API_ int main (int argc, char ** argv)
{
	printf ("Running main() from core_test_main.cc\n");
	btcnew::force_btcnew_test_network ();
	btcnew::node_singleton_memory_pool_purge_guard memory_pool_cleanup_guard;
	// Setting up logging so that there aren't any piped to standard output.
	btcnew::logging logging;
	logging.init (btcnew::unique_path ());
	testing::InitGoogleTest (&argc, argv);
	auto res = RUN_ALL_TESTS ();
	btcnew::cleanup_test_directories_on_exit ();
	return res;
}

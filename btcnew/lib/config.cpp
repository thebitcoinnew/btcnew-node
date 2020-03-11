#include <btcnew/lib/config.hpp>

#include <valgrind/valgrind.h>

namespace btcnew
{
void force_btcnew_test_network ()
{
	btcnew::network_constants::set_active_network (btcnew::btcnew_networks::btcnew_test_network);
}

bool running_within_valgrind ()
{
	return (RUNNING_ON_VALGRIND > 0);
}
}

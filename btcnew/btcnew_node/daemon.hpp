#include <boost/filesystem.hpp>

namespace btcnew
{
class node_flags;
}
namespace btcnew_daemon
{
class daemon
{
public:
	void run (boost::filesystem::path const &, btcnew::node_flags const & flags);
};
}

#include <btcnew/lib/errors.hpp>
#include <btcnew/lib/utility.hpp>
#include <btcnew/node/cli.hpp>
#include <btcnew/rpc/rpc.hpp>
#include <btcnew/secure/working.hpp>

#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>

int main (int argc, char * const * argv)
{
	btcnew::set_umask ();
	try
	{
		boost::program_options::options_description description ("Command line options");
		description.add_options () ("help", "Print out options");
		btcnew::add_node_options (description);
		boost::program_options::variables_map vm;
		boost::program_options::store (boost::program_options::command_line_parser (argc, argv).options (description).allow_unregistered ().run (), vm);
		boost::program_options::notify (vm);
		int result (0);

		if (!vm.count ("data_path"))
		{
			std::string error_string;
			if (!btcnew::migrate_working_path (error_string))
			{
				throw std::runtime_error (error_string);
			}
		}

		auto ec = btcnew::handle_node_options (vm);
		if (ec == btcnew::error_cli::unknown_command && vm.count ("help") != 0)
		{
			std::cout << description << std::endl;
		}
		return result;
	}
	catch (std::exception const & e)
	{
		std::cerr << boost::str (boost::format ("Exception while initializing %1%") % e.what ());
	}
	catch (...)
	{
		std::cerr << boost::str (boost::format ("Unknown exception while initializing"));
	}
	return 1;
}

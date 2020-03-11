#include <btcnew/lib/errors.hpp>
#include <btcnew/lib/jsonconfig.hpp>
#include <btcnew/lib/utility.hpp>
#include <btcnew/btcnew_wallet/icon.hpp>
#include <btcnew/node/cli.hpp>
#include <btcnew/node/ipc.hpp>
#include <btcnew/rpc/rpc.hpp>
#include <btcnew/rpc/rpc_request_processor.hpp>
#include <btcnew/secure/working.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/program_options.hpp>

#include <csignal>

namespace
{
void logging_init (boost::filesystem::path const & application_path_a)
{
	static std::atomic_flag logging_already_added = ATOMIC_FLAG_INIT;
	if (!logging_already_added.test_and_set ())
	{
		boost::log::add_common_attributes ();
		auto path = application_path_a / "log";

		uintmax_t max_size{ 128 * 1024 * 1024 };
		uintmax_t rotation_size{ 4 * 1024 * 1024 };
		bool flush{ true };
		boost::log::add_file_log (boost::log::keywords::target = path, boost::log::keywords::file_name = path / "rpc_log_%Y-%m-%d_%H-%M-%S.%N.log", boost::log::keywords::rotation_size = rotation_size, boost::log::keywords::auto_flush = flush, boost::log::keywords::scan_method = boost::log::sinks::file::scan_method::scan_matching, boost::log::keywords::max_size = max_size, boost::log::keywords::format = "[%TimeStamp%]: %Message%");
	}
}

volatile sig_atomic_t sig_int_or_term = 0;

void run (boost::filesystem::path const & data_path, std::vector<std::string> const & config_overrides)
{
	boost::filesystem::create_directories (data_path);
	boost::system::error_code error_chmod;
	btcnew::set_secure_perm_directory (data_path, error_chmod);
	std::unique_ptr<btcnew::thread_runner> runner;

	btcnew::rpc_config rpc_config;
	auto error = btcnew::read_rpc_config_toml (data_path, rpc_config, config_overrides);
	if (!error)
	{
		logging_init (data_path);
		boost::asio::io_context io_ctx;
		try
		{
			btcnew::ipc_rpc_processor ipc_rpc_processor (io_ctx, rpc_config);
			auto rpc = btcnew::get_rpc (io_ctx, rpc_config, ipc_rpc_processor);
			rpc->start ();

			assert (!btcnew::signal_handler_impl);
			btcnew::signal_handler_impl = [&io_ctx] () {
				io_ctx.stop ();
				sig_int_or_term = 1;
			};

			std::signal (SIGINT, &btcnew::signal_handler);
			std::signal (SIGTERM, &btcnew::signal_handler);

			runner = std::make_unique<btcnew::thread_runner> (io_ctx, rpc_config.rpc_process.io_threads);
			runner->join ();

			if (sig_int_or_term == 1)
			{
				rpc->stop ();
			}
		}
		catch (const std::runtime_error & e)
		{
			std::cerr << "Error while running rpc (" << e.what () << ")\n";
		}
	}
	else
	{
		std::cerr << "Error deserializing config: " << error.get_message () << std::endl;
	}
}
}

int main (int argc, char * const * argv)
{
	btcnew::set_umask ();

	boost::program_options::options_description description ("Command line options");

	// clang-format off
	description.add_options ()
		("help", "Print out options")
		("config", boost::program_options::value<std::vector<std::string>>()->multitoken(), "Pass RPC configuration values. This takes precedence over any values in the configuration file. This option can be repeated multiple times.")
		("daemon", "Start RPC daemon")
		("data_path", boost::program_options::value<std::string> (), "Use the supplied path as the data directory")
		("network", boost::program_options::value<std::string> (), "Use the supplied network (live, beta or test)")
		("version", "Prints out version");
	// clang-format on

	boost::program_options::variables_map vm;
	try
	{
		boost::program_options::store (boost::program_options::parse_command_line (argc, argv, description), vm);
	}
	catch (boost::program_options::error const & err)
	{
		std::cerr << err.what () << std::endl;
		return 1;
	}
	boost::program_options::notify (vm);

	auto network (vm.find ("network"));
	if (network != vm.end ())
	{
		auto err (btcnew::network_constants::set_active_network (network->second.as<std::string> ()));
		if (err)
		{
			std::cerr << err.get_message () << std::endl;
			std::exit (1);
		}
	}

	auto data_path_it = vm.find ("data_path");
	if (data_path_it == vm.end ())
	{
		std::string error_string;
		if (!btcnew::migrate_working_path (error_string))
		{
			std::cerr << error_string << std::endl;

			return 1;
		}
	}

	boost::filesystem::path data_path ((data_path_it != vm.end ()) ? data_path_it->second.as<std::string> () : btcnew::working_path ());
	if (vm.count ("daemon") > 0)
	{
		std::vector<std::string> config_overrides;
		auto config (vm.find ("config"));
		if (config != vm.end ())
		{
			config_overrides = config->second.as<std::vector<std::string>> ();
		}
		run (data_path, config_overrides);
	}
	else if (vm.count ("version"))
	{
		std::cout << "Version " << BTCNEW_VERSION_STRING << "\n"
		          << "Build Info " << BUILD_INFO << std::endl;
	}
	else
	{
		std::cout << description << std::endl;
	}

	return 1;
}
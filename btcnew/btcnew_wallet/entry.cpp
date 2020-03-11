#include <btcnew/boost/process.hpp>
#include <btcnew/crypto_lib/random_pool.hpp>
#include <btcnew/lib/errors.hpp>
#include <btcnew/lib/rpcconfig.hpp>
#include <btcnew/lib/tomlconfig.hpp>
#include <btcnew/lib/utility.hpp>
#include <btcnew/lib/walletconfig.hpp>
#include <btcnew/btcnew_wallet/icon.hpp>
#include <btcnew/node/cli.hpp>
#include <btcnew/node/daemonconfig.hpp>
#include <btcnew/node/ipc.hpp>
#include <btcnew/node/json_handler.hpp>
#include <btcnew/node/node_rpc_config.hpp>
#include <btcnew/qt/qt.hpp>
#include <btcnew/rpc/rpc.hpp>
#include <btcnew/secure/working.hpp>

#include <boost/make_shared.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace
{
void show_error (std::string const & message_a)
{
	QMessageBox message (QMessageBox::Critical, "Error starting Bitcoin New", message_a.c_str ());
	message.setModal (true);
	message.show ();
	message.exec ();
}
void show_help (std::string const & message_a)
{
	QMessageBox message (QMessageBox::NoIcon, "Help", "see <a href=\"https://docs.btcnew.org/commands/command-line-interface/#launch-options\">launch options</a> ");
	message.setStyleSheet ("QLabel {min-width: 450px}");
	message.setDetailedText (message_a.c_str ());
	message.show ();
	message.exec ();
}

btcnew::error read_and_update_wallet_config (btcnew::wallet_config & config_a, boost::filesystem::path const & data_path_a)
{
	btcnew::tomlconfig wallet_config_toml;
	auto wallet_path (btcnew::get_qtwallet_toml_config_path (data_path_a));
	wallet_config_toml.read (btcnew::get_qtwallet_toml_config_path (data_path_a));
	config_a.serialize_toml (wallet_config_toml);

	// Write wallet config. If missing, the file is created and permissions are set.
	wallet_config_toml.write (wallet_path);
	return wallet_config_toml.get_error ();
}
}

int run_wallet (QApplication & application, int argc, char * const * argv, boost::filesystem::path const & data_path, std::vector<std::string> const & config_overrides, btcnew::node_flags const & flags)
{
	int result (0);
	btcnew_qt::eventloop_processor processor;
	boost::system::error_code error_chmod;
	boost::filesystem::create_directories (data_path);
	btcnew::set_secure_perm_directory (data_path, error_chmod);
	QPixmap pixmap (":/logo.png");
	QSplashScreen * splash = new QSplashScreen (pixmap);
	splash->show ();
	application.processEvents ();
	splash->showMessage (QSplashScreen::tr ("Remember - Back Up Your Wallet Seed"), Qt::AlignBottom | Qt::AlignHCenter, Qt::darkGray);
	application.processEvents ();

	btcnew::daemon_config config (data_path);
	btcnew::wallet_config wallet_config;

	auto error = btcnew::read_node_config_toml (data_path, config, config_overrides);
	if (!error)
	{
		error = read_and_update_wallet_config (wallet_config, data_path);
	}

#if !BTCNEW_ROCKSDB
	if (!error && config.node.rocksdb_config.enable)
	{
		error = btcnew::error_config::rocksdb_enabled_but_not_supported;
	}
#endif

	if (!error)
	{
		btcnew::set_use_memory_pools (config.node.use_memory_pools);

		config.node.logging.init (data_path);
		btcnew::logger_mt logger{ config.node.logging.min_time_between_log_output };

		boost::asio::io_context io_ctx;
		btcnew::thread_runner runner (io_ctx, config.node.io_threads);

		std::shared_ptr<btcnew::node> node;
		std::shared_ptr<btcnew_qt::wallet> gui;
		btcnew::set_application_icon (application);
		auto opencl (btcnew::opencl_work::create (config.opencl_enable, config.opencl, logger));
		btcnew::work_pool work (config.node.work_threads, config.node.pow_sleep_interval, opencl ? [&opencl] (btcnew::root const & root_a, uint64_t difficulty_a, std::atomic<int> &) {
			return opencl->generate_work (root_a, difficulty_a);
		}
		                                                                                       : std::function<boost::optional<uint64_t> (btcnew::root const &, uint64_t, std::atomic<int> &)> (nullptr));
		btcnew::alarm alarm (io_ctx);
		node = std::make_shared<btcnew::node> (io_ctx, data_path, alarm, config.node, work, flags);
		if (!node->init_error ())
		{
			auto wallet (node->wallets.open (wallet_config.wallet));
			if (wallet == nullptr)
			{
				auto existing (node->wallets.items.begin ());
				if (existing != node->wallets.items.end ())
				{
					wallet = existing->second;
					wallet_config.wallet = existing->first;
				}
				else
				{
					wallet = node->wallets.create (wallet_config.wallet);
				}
			}
			if (wallet_config.account.is_zero () || !wallet->exists (wallet_config.account))
			{
				auto transaction (wallet->wallets.tx_begin_write ());
				auto existing (wallet->store.begin (transaction));
				if (existing != wallet->store.end ())
				{
					wallet_config.account = existing->first;
				}
				else
				{
					wallet_config.account = wallet->deterministic_insert (transaction);
				}
			}
			assert (wallet->exists (wallet_config.account));
			read_and_update_wallet_config (wallet_config, data_path);
			node->start ();
			btcnew::ipc::ipc_server ipc (*node, config.rpc);

#if BOOST_PROCESS_SUPPORTED
			std::unique_ptr<boost::process::child> rpc_process;
			std::unique_ptr<boost::process::child> btcnew_pow_server_process;
#endif

			if (config.pow_server.enable)
			{
				if (!boost::filesystem::exists (config.pow_server.pow_server_path))
				{
					splash->hide ();
					show_error (std::string ("btcnew_pow_server is configured to start as a child process, however the file cannot be found at: ") + config.pow_server.pow_server_path);
					std::exit (1);
				}

#if BOOST_PROCESS_SUPPORTED
				auto network = node->network_params.network.get_current_network_as_string ();
				btcnew_pow_server_process = std::make_unique<boost::process::child> (config.pow_server.pow_server_path, "--config_path", data_path / "config-btcnew-pow-server.toml");
#else
				splash->hide ();
				show_error ("btcnew_pow_server is configured to start as a child process, but this is not supported on this system. Disable startup and start the server manually.");
				std::exit (1);
#endif
			}

			std::unique_ptr<btcnew::rpc> rpc;
			std::unique_ptr<btcnew::rpc_handler_interface> rpc_handler;
			if (config.rpc_enable)
			{
				if (!config.rpc.child_process.enable)
				{
					// Launch rpc in-process
					btcnew::rpc_config rpc_config;
					auto error = btcnew::read_rpc_config_toml (data_path, rpc_config);
					if (error)
					{
						show_error (error.get_message ());
					}
					rpc_handler = std::make_unique<btcnew::inprocess_rpc_handler> (*node, config.rpc);
					rpc = btcnew::get_rpc (io_ctx, rpc_config, *rpc_handler);
					rpc->start ();
				}
				else
				{
					// Spawn a child rpc process
					if (!boost::filesystem::exists (config.rpc.child_process.rpc_path))
					{
						throw std::runtime_error (std::string ("RPC is configured to spawn a new process however the file cannot be found at: ") + config.rpc.child_process.rpc_path);
					}

#if BOOST_PROCESS_SUPPORTED
					auto network = node->network_params.network.get_current_network_as_string ();
					rpc_process = std::make_unique<boost::process::child> (config.rpc.child_process.rpc_path, "--daemon", "--data_path", data_path, "--network", network);
#else
					show_error ("rpc_enable is set to true in the config. Set it to false and start the RPC server manually.");
#endif
				}
			}
			QObject::connect (&application, &QApplication::aboutToQuit, [&] () {
				ipc.stop ();
				node->stop ();
				if (rpc)
				{
					rpc->stop ();
				}
#if USE_BOOST_PROCESS
				if (rpc_process)
				{
					rpc_process->terminate ();
				}

				if (btcnew_pow_server_process)
				{
					btcnew_pow_server_process->terminate ();
				}
#endif
				runner.stop_event_processing ();
			});
			application.postEvent (&processor, new btcnew_qt::eventloop_event ([&] () {
				gui = std::make_shared<btcnew_qt::wallet> (application, processor, *node, wallet, wallet_config.account);
				splash->close ();
				gui->start ();
				gui->client_window->show ();
			}));
			result = application.exec ();
			runner.join ();
		}
		else
		{
			splash->hide ();
			show_error ("Error initializing node");
		}
		read_and_update_wallet_config (wallet_config, data_path);
	}
	else
	{
		splash->hide ();
		show_error ("Error deserializing config: " + error.get_message ());
	}
	return result;
}

int main (int argc, char * const * argv)
{
	btcnew::set_umask ();
	btcnew::node_singleton_memory_pool_purge_guard memory_pool_cleanup_guard;
	try
	{
		QApplication application (argc, const_cast<char **> (argv));
		boost::program_options::options_description description ("Command line options");
		// clang-format off
		description.add_options()
			("help", "Print out options")
			("config", boost::program_options::value<std::vector<std::string>>()->multitoken(), "Pass configuration values. This takes precedence over any values in the node configuration file. This option can be repeated multiple times.");
		btcnew::add_node_flag_options (description);
		btcnew::add_node_options (description);
		// clang-format on
		boost::program_options::variables_map vm;
		try
		{
			boost::program_options::store (boost::program_options::parse_command_line (argc, argv, description), vm);
		}
		catch (boost::program_options::error const & err)
		{
			show_error (err.what ());
			return 1;
		}
		boost::program_options::notify (vm);
		int result (0);
		auto network (vm.find ("network"));
		if (network != vm.end ())
		{
			auto err (btcnew::network_constants::set_active_network (network->second.as<std::string> ()));
			if (err)
			{
				show_error (err.get_message ());
				std::exit (1);
			}
		}

		if (!vm.count ("data_path"))
		{
			std::string error_string;
			if (!btcnew::migrate_working_path (error_string))
			{
				throw std::runtime_error (error_string);
			}
		}

		std::vector<std::string> config_overrides;
		if (vm.count ("config"))
		{
			config_overrides = vm["config"].as<std::vector<std::string>> ();
		}

		auto ec = btcnew::handle_node_options (vm);
		if (ec == btcnew::error_cli::unknown_command)
		{
			if (vm.count ("help") != 0)
			{
				std::ostringstream outstream;
				description.print (outstream);
				std::string helpstring = outstream.str ();
				show_help (helpstring);
				return 1;
			}
			else
			{
				try
				{
					boost::filesystem::path data_path;
					if (vm.count ("data_path"))
					{
						auto name (vm["data_path"].as<std::string> ());
						data_path = boost::filesystem::path (name);
					}
					else
					{
						data_path = btcnew::working_path ();
					}
					btcnew::node_flags flags;
					auto flags_ec = btcnew::update_flags (flags, vm);
					if (flags_ec)
					{
						throw std::runtime_error (flags_ec.message ());
					}
					auto config (vm.find ("config"));
					if (config != vm.end ())
					{
						flags.config_overrides = config->second.as<std::vector<std::string>> ();
					}
					result = run_wallet (application, argc, argv, data_path, config_overrides, flags);
				}
				catch (std::exception const & e)
				{
					show_error (boost::str (boost::format ("Exception while running wallet: %1%") % e.what ()));
				}
				catch (...)
				{
					show_error ("Unknown exception while running wallet");
				}
			}
		}
		return result;
	}
	catch (std::exception const & e)
	{
		show_error (boost::str (boost::format ("Exception while initializing %1%") % e.what ()));
	}
	catch (...)
	{
		show_error (boost::str (boost::format ("Unknown exception while initializing")));
	}
	return 1;
}

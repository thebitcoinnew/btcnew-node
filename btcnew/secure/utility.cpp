#include <btcnew/lib/config.hpp>
#include <btcnew/secure/utility.hpp>
#include <btcnew/secure/working.hpp>

static std::vector<boost::filesystem::path> all_unique_paths;

boost::filesystem::path btcnew::working_path (bool legacy)
{
	static btcnew::network_constants network_constants;
	auto result (btcnew::app_path ());
	switch (network_constants.network ())
	{
		case btcnew::btcnew_networks::btcnew_test_network:
			result /= "BitcoinNewTest";
			break;
		case btcnew::btcnew_networks::btcnew_beta_network:
			result /= "BitcoinNewBeta";
			break;
		case btcnew::btcnew_networks::btcnew_live_network:
			result /= "BitcoinNew";
			break;
	}
	return result;
}

bool btcnew::migrate_working_path (std::string & error_string)
{
	bool result (true);
	auto old_path (btcnew::working_path (true));
	auto new_path (btcnew::working_path ());

	if (old_path != new_path)
	{
		boost::system::error_code status_error;

		auto old_path_status (boost::filesystem::status (old_path, status_error));
		if (status_error == boost::system::errc::success && boost::filesystem::exists (old_path_status) && boost::filesystem::is_directory (old_path_status))
		{
			auto new_path_status (boost::filesystem::status (new_path, status_error));
			if (!boost::filesystem::exists (new_path_status))
			{
				boost::system::error_code rename_error;

				boost::filesystem::rename (old_path, new_path, rename_error);
				if (rename_error != boost::system::errc::success)
				{
					std::stringstream error_string_stream;

					error_string_stream << "Unable to migrate data from " << old_path << " to " << new_path;

					error_string = error_string_stream.str ();

					result = false;
				}
			}
		}
	}

	return result;
}

boost::filesystem::path btcnew::unique_path ()
{
	auto result (working_path () / boost::filesystem::unique_path ());
	all_unique_paths.push_back (result);
	return result;
}

void btcnew::remove_temporary_directories ()
{
	for (auto & path : all_unique_paths)
	{
		boost::system::error_code ec;
		boost::filesystem::remove_all (path, ec);
		if (ec)
		{
			std::cerr << "Could not remove temporary directory: " << ec.message () << std::endl;
		}

		// lmdb creates a -lock suffixed file for its MDB_NOSUBDIR databases
		auto lockfile = path;
		lockfile += "-lock";
		boost::filesystem::remove (lockfile, ec);
		if (ec)
		{
			std::cerr << "Could not remove temporary lock file: " << ec.message () << std::endl;
		}
	}
}

namespace btcnew
{
/** A wrapper for handling signals */
std::function<void ()> signal_handler_impl;
void signal_handler (int sig)
{
	if (signal_handler_impl != nullptr)
	{
		signal_handler_impl ();
	}
}
}

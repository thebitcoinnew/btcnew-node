#pragma once

#include <btcnew/lib/errors.hpp>
#include <btcnew/lib/numbers.hpp>

#include <crypto/cryptopp/osrng.h>

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <array>
#include <atomic>
#include <condition_variable>
#include <type_traits>

namespace btcnew
{
using bufferstream = boost::iostreams::stream_buffer<boost::iostreams::basic_array_source<uint8_t>>;
using vectorstream = boost::iostreams::stream_buffer<boost::iostreams::back_insert_device<std::vector<uint8_t>>>;
// OS-specific way of finding a path to a home directory.
boost::filesystem::path working_path (bool = false);
// Function to migrate working_path() from above from BtcNew to Bitcoin New
bool migrate_working_path (std::string &);
// Get a unique path within the home directory, used for testing.
// Any directories created at this location will be removed when a test finishes.
boost::filesystem::path unique_path ();
// Remove all unique tmp directories created by the process
void remove_temporary_directories ();
// Generic signal handler declarations
extern std::function<void ()> signal_handler_impl;
void signal_handler (int sig);
}

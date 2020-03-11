#pragma once

#include <boost/property_tree/ptree.hpp>

#include <functional>
#include <string>
#include <vector>

namespace btcnew
{
class rpc_config;
class rpc_handler_interface;
class logger_mt;

class rpc_handler : public std::enable_shared_from_this<btcnew::rpc_handler>
{
public:
	rpc_handler (btcnew::rpc_config const & rpc_config, std::string const & body_a, std::string const & request_id_a, std::function<void (std::string const &)> const & response_a, btcnew::rpc_handler_interface & rpc_handler_interface_a, btcnew::logger_mt & logger);
	void process_request ();

private:
	std::string body;
	std::string request_id;
	boost::property_tree::ptree request;
	std::function<void (std::string const &)> response;
	btcnew::rpc_config const & rpc_config;
	btcnew::rpc_handler_interface & rpc_handler_interface;
	btcnew::logger_mt & logger;
};
}

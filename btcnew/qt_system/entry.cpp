#include <btcnew/crypto_lib/random_pool.hpp>
#include <btcnew/lib/config.hpp>
#include <btcnew/node/common.hpp>
#include <btcnew/node/testing.hpp>
#include <btcnew/qt/qt.hpp>

#include <thread>

int main (int argc, char ** argv)
{
	btcnew::network_constants::set_active_network (btcnew::btcnew_networks::btcnew_test_network);
	btcnew::node_singleton_memory_pool_purge_guard memory_pool_cleanup_guard;
	QApplication application (argc, argv);
	QCoreApplication::setOrganizationName ("Nano");
	QCoreApplication::setOrganizationDomain ("btcnew.org");
	QCoreApplication::setApplicationName ("Nano Wallet");
	btcnew_qt::eventloop_processor processor;
	static uint16_t count (16);
	btcnew::system system (24000, count);
	btcnew::thread_runner runner (system.io_ctx, system.nodes[0]->config.io_threads);
	std::unique_ptr<QTabWidget> client_tabs (new QTabWidget);
	std::vector<std::unique_ptr<btcnew_qt::wallet>> guis;
	for (auto i (0); i < count; ++i)
	{
		auto wallet (system.nodes[i]->wallets.create (btcnew::random_wallet_id ()));
		btcnew::keypair key;
		wallet->insert_adhoc (key.prv);
		guis.push_back (std::unique_ptr<btcnew_qt::wallet> (new btcnew_qt::wallet (application, processor, *system.nodes[i], wallet, key.pub)));
		client_tabs->addTab (guis.back ()->client_window, boost::str (boost::format ("Wallet %1%") % i).c_str ());
	}
	client_tabs->show ();
	QObject::connect (&application, &QApplication::aboutToQuit, [&] () {
		system.stop ();
	});
	int result;
	try
	{
		result = application.exec ();
	}
	catch (...)
	{
		result = -1;
		assert (false);
	}
	runner.join ();
	return result;
}

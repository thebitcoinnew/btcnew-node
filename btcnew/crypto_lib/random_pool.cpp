#include <btcnew/crypto_lib/random_pool.hpp>

std::mutex btcnew::random_pool::mutex;
CryptoPP::AutoSeededRandomPool btcnew::random_pool::pool;

void btcnew::random_pool::generate_block (unsigned char * output, size_t size)
{
	std::lock_guard<std::mutex> guard (mutex);
	pool.GenerateBlock (output, size);
}

unsigned btcnew::random_pool::generate_word32 (unsigned min, unsigned max)
{
	std::lock_guard<std::mutex> guard (mutex);
	return pool.GenerateWord32 (min, max);
}

unsigned char btcnew::random_pool::generate_byte ()
{
	std::lock_guard<std::mutex> guard (mutex);
	return pool.GenerateByte ();
}

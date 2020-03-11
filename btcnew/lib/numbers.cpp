#include <btcnew/crypto/blake2/blake2.h>
#include <btcnew/crypto_lib/random_pool.hpp>
#include <btcnew/lib/numbers.hpp>
#include <btcnew/lib/utility.hpp>

#include <crypto/cryptopp/aes.h>
#include <crypto/cryptopp/modes.h>

#include <crypto/ed25519-donna/ed25519.h>

namespace
{
char const * account_lookup ("13456789abcdefghijkmnopqrstuwxyz");
char const * account_reverse ("~0~1234567~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~89:;<=>?@AB~CDEFGHIJK~LMNO~~~~~");
char account_encode (uint8_t value)
{
	assert (value < 32);
	auto result (account_lookup[value]);
	return result;
}
uint8_t account_decode (char value)
{
	assert (value >= '0');
	assert (value <= '~');
	auto result (account_reverse[value - 0x30]);
	if (result != '~')
	{
		result -= 0x30;
	}
	return result;
}
}

void btcnew::public_key::encode_account (std::string & destination_a) const
{
	assert (destination_a.empty ());
	destination_a.reserve (65);
	uint64_t check (0);
	blake2b_state hash;
	blake2b_init (&hash, 5);
	blake2b_update (&hash, bytes.data (), bytes.size ());
	blake2b_final (&hash, reinterpret_cast<uint8_t *> (&check), 5);
	btcnew::uint512_t number_l (number ());
	number_l <<= 40;
	number_l |= btcnew::uint512_t (check);
	for (auto i (0); i < 60; ++i)
	{
		uint8_t r (number_l & static_cast<uint8_t> (0x1f));
		number_l >>= 5;
		destination_a.push_back (account_encode (r));
	}
	destination_a.append ("_wenctb"); // btcnew_
	std::reverse (destination_a.begin (), destination_a.end ());
}

std::string btcnew::public_key::to_account () const
{
	std::string result;
	encode_account (result);
	return result;
}

std::string btcnew::public_key::to_node_id () const
{
	return to_account ().replace (0, 4, "node");
}

bool btcnew::public_key::decode_account (std::string const & source_a)
{
	auto error (source_a.size () < 5);
	if (!error)
	{
        auto btcnew_prefix (source_a[0] == 'b' && source_a[1] == 't' && source_a[2] == 'c' && source_a[3] == 'n' && source_a[4] == 'e' && source_a[5] == 'w' && source_a[6] == '_');
		error = (btcnew_prefix && source_a.size () != 67);
		if (!error)
		{
			if (btcnew_prefix)
			{
				auto i (source_a.begin () + 7);
				if (*i == '1' || *i == '3')
				{
					btcnew::uint512_t number_l;
					for (auto j (source_a.end ()); !error && i != j; ++i)
					{
						uint8_t character (*i);
						error = character < 0x30 || character >= 0x80;
						if (!error)
						{
							uint8_t byte (account_decode (character));
							error = byte == '~';
							if (!error)
							{
								number_l <<= 5;
								number_l += byte;
							}
						}
					}
					if (!error)
					{
						*this = (number_l >> 40).convert_to<btcnew::uint256_t> ();
						uint64_t check (number_l & static_cast<uint64_t> (0xffffffffff));
						uint64_t validation (0);
						blake2b_state hash;
						blake2b_init (&hash, 5);
						blake2b_update (&hash, bytes.data (), bytes.size ());
						blake2b_final (&hash, reinterpret_cast<uint8_t *> (&validation), 5);
						error = check != validation;
					}
				}
				else
				{
					error = true;
				}
			}
			else
			{
				error = true;
			}
		}
	}
	return error;
}

btcnew::uint256_union::uint256_union (btcnew::uint256_t const & number_a)
{
	bytes.fill (0);
	boost::multiprecision::export_bits (number_a, bytes.rbegin (), 8, false);
}

bool btcnew::uint256_union::operator== (btcnew::uint256_union const & other_a) const
{
	return bytes == other_a.bytes;
}

// Construct a uint256_union = AES_ENC_CTR (cleartext, key, iv)
void btcnew::uint256_union::encrypt (btcnew::raw_key const & cleartext, btcnew::raw_key const & key, uint128_union const & iv)
{
	CryptoPP::AES::Encryption alg (key.data.bytes.data (), sizeof (key.data.bytes));
	CryptoPP::CTR_Mode_ExternalCipher::Encryption enc (alg, iv.bytes.data ());
	enc.ProcessData (bytes.data (), cleartext.data.bytes.data (), sizeof (cleartext.data.bytes));
}

bool btcnew::uint256_union::is_zero () const
{
	return qwords[0] == 0 && qwords[1] == 0 && qwords[2] == 0 && qwords[3] == 0;
}

std::string btcnew::uint256_union::to_string () const
{
	std::string result;
	encode_hex (result);
	return result;
}

bool btcnew::uint256_union::operator< (btcnew::uint256_union const & other_a) const
{
	return std::memcmp (bytes.data (), other_a.bytes.data (), 32) < 0;
}

btcnew::uint256_union & btcnew::uint256_union::operator^= (btcnew::uint256_union const & other_a)
{
	auto j (other_a.qwords.begin ());
	for (auto i (qwords.begin ()), n (qwords.end ()); i != n; ++i, ++j)
	{
		*i ^= *j;
	}
	return *this;
}

btcnew::uint256_union btcnew::uint256_union::operator^ (btcnew::uint256_union const & other_a) const
{
	btcnew::uint256_union result;
	auto k (result.qwords.begin ());
	for (auto i (qwords.begin ()), j (other_a.qwords.begin ()), n (qwords.end ()); i != n; ++i, ++j, ++k)
	{
		*k = *i ^ *j;
	}
	return result;
}

btcnew::uint256_union::uint256_union (std::string const & hex_a)
{
	auto error (decode_hex (hex_a));

	release_assert (!error);
}

void btcnew::uint256_union::clear ()
{
	qwords.fill (0);
}

btcnew::uint256_t btcnew::uint256_union::number () const
{
	btcnew::uint256_t result;
	boost::multiprecision::import_bits (result, bytes.begin (), bytes.end ());
	return result;
}

void btcnew::uint256_union::encode_hex (std::string & text) const
{
	assert (text.empty ());
	std::stringstream stream;
	stream << std::hex << std::uppercase << std::noshowbase << std::setw (64) << std::setfill ('0');
	stream << number ();
	text = stream.str ();
}

bool btcnew::uint256_union::decode_hex (std::string const & text)
{
	auto error (false);
	if (!text.empty () && text.size () <= 64)
	{
		std::stringstream stream (text);
		stream << std::hex << std::noshowbase;
		btcnew::uint256_t number_l;
		try
		{
			stream >> number_l;
			*this = number_l;
			if (!stream.eof ())
			{
				error = true;
			}
		}
		catch (std::runtime_error &)
		{
			error = true;
		}
	}
	else
	{
		error = true;
	}
	return error;
}

void btcnew::uint256_union::encode_dec (std::string & text) const
{
	assert (text.empty ());
	std::stringstream stream;
	stream << std::dec << std::noshowbase;
	stream << number ();
	text = stream.str ();
}

bool btcnew::uint256_union::decode_dec (std::string const & text)
{
	auto error (text.size () > 78 || (text.size () > 1 && text.front () == '0') || (!text.empty () && text.front () == '-'));
	if (!error)
	{
		std::stringstream stream (text);
		stream << std::dec << std::noshowbase;
		btcnew::uint256_t number_l;
		try
		{
			stream >> number_l;
			*this = number_l;
			if (!stream.eof ())
			{
				error = true;
			}
		}
		catch (std::runtime_error &)
		{
			error = true;
		}
	}
	return error;
}

btcnew::uint256_union::uint256_union (uint64_t value0)
{
	*this = btcnew::uint256_t (value0);
}

bool btcnew::uint256_union::operator!= (btcnew::uint256_union const & other_a) const
{
	return !(*this == other_a);
}

bool btcnew::uint512_union::operator== (btcnew::uint512_union const & other_a) const
{
	return bytes == other_a.bytes;
}

btcnew::uint512_union::uint512_union (btcnew::uint256_union const & upper, btcnew::uint256_union const & lower)
{
	uint256s[0] = upper;
	uint256s[1] = lower;
}

btcnew::uint512_union::uint512_union (btcnew::uint512_t const & number_a)
{
	bytes.fill (0);
	boost::multiprecision::export_bits (number_a, bytes.rbegin (), 8, false);
}

bool btcnew::uint512_union::is_zero () const
{
	return qwords[0] == 0 && qwords[1] == 0 && qwords[2] == 0 && qwords[3] == 0
	&& qwords[4] == 0 && qwords[5] == 0 && qwords[6] == 0 && qwords[7] == 0;
}

void btcnew::uint512_union::clear ()
{
	bytes.fill (0);
}

btcnew::uint512_t btcnew::uint512_union::number () const
{
	btcnew::uint512_t result;
	boost::multiprecision::import_bits (result, bytes.begin (), bytes.end ());
	return result;
}

void btcnew::uint512_union::encode_hex (std::string & text) const
{
	assert (text.empty ());
	std::stringstream stream;
	stream << std::hex << std::uppercase << std::noshowbase << std::setw (128) << std::setfill ('0');
	stream << number ();
	text = stream.str ();
}

bool btcnew::uint512_union::decode_hex (std::string const & text)
{
	auto error (text.size () > 128);
	if (!error)
	{
		std::stringstream stream (text);
		stream << std::hex << std::noshowbase;
		btcnew::uint512_t number_l;
		try
		{
			stream >> number_l;
			*this = number_l;
			if (!stream.eof ())
			{
				error = true;
			}
		}
		catch (std::runtime_error &)
		{
			error = true;
		}
	}
	return error;
}

bool btcnew::uint512_union::operator!= (btcnew::uint512_union const & other_a) const
{
	return !(*this == other_a);
}

btcnew::uint512_union & btcnew::uint512_union::operator^= (btcnew::uint512_union const & other_a)
{
	uint256s[0] ^= other_a.uint256s[0];
	uint256s[1] ^= other_a.uint256s[1];
	return *this;
}

std::string btcnew::uint512_union::to_string () const
{
	std::string result;
	encode_hex (result);
	return result;
}

btcnew::raw_key::~raw_key ()
{
	data.clear ();
}

bool btcnew::raw_key::operator== (btcnew::raw_key const & other_a) const
{
	return data == other_a.data;
}

bool btcnew::raw_key::operator!= (btcnew::raw_key const & other_a) const
{
	return !(*this == other_a);
}

// This this = AES_DEC_CTR (ciphertext, key, iv)
void btcnew::raw_key::decrypt (btcnew::uint256_union const & ciphertext, btcnew::raw_key const & key_a, uint128_union const & iv)
{
	CryptoPP::AES::Encryption alg (key_a.data.bytes.data (), sizeof (key_a.data.bytes));
	CryptoPP::CTR_Mode_ExternalCipher::Decryption dec (alg, iv.bytes.data ());
	dec.ProcessData (data.bytes.data (), ciphertext.bytes.data (), sizeof (ciphertext.bytes));
}

btcnew::private_key const & btcnew::raw_key::as_private_key () const
{
	return reinterpret_cast<btcnew::private_key const &> (data);
}

btcnew::signature btcnew::sign_message (btcnew::raw_key const & private_key, btcnew::public_key const & public_key, btcnew::uint256_union const & message)
{
	btcnew::signature result;
	ed25519_sign (message.bytes.data (), sizeof (message.bytes), private_key.data.bytes.data (), public_key.bytes.data (), result.bytes.data ());
	return result;
}

btcnew::private_key btcnew::deterministic_key (btcnew::raw_key const & seed_a, uint32_t index_a)
{
	btcnew::private_key prv_key;
	blake2b_state hash;
	blake2b_init (&hash, prv_key.bytes.size ());
	blake2b_update (&hash, seed_a.data.bytes.data (), seed_a.data.bytes.size ());
	btcnew::uint256_union index (index_a);
	blake2b_update (&hash, reinterpret_cast<uint8_t *> (&index.dwords[7]), sizeof (uint32_t));
	blake2b_final (&hash, prv_key.bytes.data (), prv_key.bytes.size ());
	return prv_key;
}

btcnew::public_key btcnew::pub_key (btcnew::private_key const & privatekey_a)
{
	btcnew::public_key result;
	ed25519_publickey (privatekey_a.bytes.data (), result.bytes.data ());
	return result;
}

bool btcnew::validate_message (btcnew::public_key const & public_key, btcnew::uint256_union const & message, btcnew::signature const & signature)
{
	auto result (0 != ed25519_sign_open (message.bytes.data (), sizeof (message.bytes), public_key.bytes.data (), signature.bytes.data ()));
	return result;
}

bool btcnew::validate_message_batch (const unsigned char ** m, size_t * mlen, const unsigned char ** pk, const unsigned char ** RS, size_t num, int * valid)
{
	bool result (0 == ed25519_sign_open_batch (m, mlen, pk, RS, num, valid));
	return result;
}

btcnew::uint128_union::uint128_union (std::string const & string_a)
{
	auto error (decode_hex (string_a));

	release_assert (!error);
}

btcnew::uint128_union::uint128_union (uint64_t value_a)
{
	*this = btcnew::uint128_t (value_a);
}

btcnew::uint128_union::uint128_union (btcnew::uint128_t const & number_a)
{
	bytes.fill (0);
	boost::multiprecision::export_bits (number_a, bytes.rbegin (), 8, false);
}

bool btcnew::uint128_union::operator== (btcnew::uint128_union const & other_a) const
{
	return qwords[0] == other_a.qwords[0] && qwords[1] == other_a.qwords[1];
}

bool btcnew::uint128_union::operator!= (btcnew::uint128_union const & other_a) const
{
	return !(*this == other_a);
}

bool btcnew::uint128_union::operator< (btcnew::uint128_union const & other_a) const
{
	return std::memcmp (bytes.data (), other_a.bytes.data (), 16) < 0;
}

bool btcnew::uint128_union::operator> (btcnew::uint128_union const & other_a) const
{
	return std::memcmp (bytes.data (), other_a.bytes.data (), 16) > 0;
}

btcnew::uint128_t btcnew::uint128_union::number () const
{
	btcnew::uint128_t result;
	boost::multiprecision::import_bits (result, bytes.begin (), bytes.end ());
	return result;
}

void btcnew::uint128_union::encode_hex (std::string & text) const
{
	assert (text.empty ());
	std::stringstream stream;
	stream << std::hex << std::uppercase << std::noshowbase << std::setw (32) << std::setfill ('0');
	stream << number ();
	text = stream.str ();
}

bool btcnew::uint128_union::decode_hex (std::string const & text)
{
	auto error (text.size () > 32);
	if (!error)
	{
		std::stringstream stream (text);
		stream << std::hex << std::noshowbase;
		btcnew::uint128_t number_l;
		try
		{
			stream >> number_l;
			*this = number_l;
			if (!stream.eof ())
			{
				error = true;
			}
		}
		catch (std::runtime_error &)
		{
			error = true;
		}
	}
	return error;
}

void btcnew::uint128_union::encode_dec (std::string & text) const
{
	assert (text.empty ());
	std::stringstream stream;
	stream << std::dec << std::noshowbase;
	stream << number ();
	text = stream.str ();
}

bool btcnew::uint128_union::decode_dec (std::string const & text, bool decimal)
{
	auto error (text.size () > 39 || (text.size () > 1 && text.front () == '0' && !decimal) || (!text.empty () && text.front () == '-'));
	if (!error)
	{
		std::stringstream stream (text);
		stream << std::dec << std::noshowbase;
		boost::multiprecision::checked_uint128_t number_l;
		try
		{
			stream >> number_l;
			btcnew::uint128_t unchecked (number_l);
			*this = unchecked;
			if (!stream.eof ())
			{
				error = true;
			}
		}
		catch (std::runtime_error &)
		{
			error = true;
		}
	}
	return error;
}

bool btcnew::uint128_union::decode_dec (std::string const & text, btcnew::uint128_t scale)
{
	bool error (text.size () > 40 || (!text.empty () && text.front () == '-'));
	if (!error)
	{
		auto delimiter_position (text.find (".")); // Dot delimiter hardcoded until decision for supporting other locales
		if (delimiter_position == std::string::npos)
		{
			btcnew::uint128_union integer;
			error = integer.decode_dec (text);
			if (!error)
			{
				// Overflow check
				try
				{
					auto result (boost::multiprecision::checked_uint128_t (integer.number ()) * boost::multiprecision::checked_uint128_t (scale));
					error = (result > std::numeric_limits<btcnew::uint128_t>::max ());
					if (!error)
					{
						*this = btcnew::uint128_t (result);
					}
				}
				catch (std::overflow_error &)
				{
					error = true;
				}
			}
		}
		else
		{
			btcnew::uint128_union integer_part;
			std::string integer_text (text.substr (0, delimiter_position));
			error = (integer_text.empty () || integer_part.decode_dec (integer_text));
			if (!error)
			{
				// Overflow check
				try
				{
					error = ((boost::multiprecision::checked_uint128_t (integer_part.number ()) * boost::multiprecision::checked_uint128_t (scale)) > std::numeric_limits<btcnew::uint128_t>::max ());
				}
				catch (std::overflow_error &)
				{
					error = true;
				}
				if (!error)
				{
					btcnew::uint128_union decimal_part;
					std::string decimal_text (text.substr (delimiter_position + 1, text.length ()));
					error = (decimal_text.empty () || decimal_part.decode_dec (decimal_text, true));
					if (!error)
					{
						// Overflow check
						auto scale_length (scale.convert_to<std::string> ().length ());
						error = (scale_length <= decimal_text.length ());
						if (!error)
						{
							auto base10 = boost::multiprecision::cpp_int (10);
							release_assert ((scale_length - decimal_text.length () - 1) <= std::numeric_limits<unsigned>::max ());
							auto pow10 = boost::multiprecision::pow (base10, static_cast<unsigned> (scale_length - decimal_text.length () - 1));
							auto decimal_part_num = decimal_part.number ();
							auto integer_part_scaled = integer_part.number () * scale;
							auto decimal_part_mult_pow = decimal_part_num * pow10;
							auto result = integer_part_scaled + decimal_part_mult_pow;

							// Overflow check
							error = (result > std::numeric_limits<btcnew::uint128_t>::max ());
							if (!error)
							{
								*this = btcnew::uint128_t (result);
							}
						}
					}
				}
			}
		}
	}
	return error;
}

void format_frac (std::ostringstream & stream, btcnew::uint128_t value, btcnew::uint128_t scale, int precision)
{
	auto reduce = scale;
	auto rem = value;
	while (reduce > 1 && rem > 0 && precision > 0)
	{
		reduce /= 10;
		auto val = rem / reduce;
		rem -= val * reduce;
		stream << val;
		precision--;
	}
}

void format_dec (std::ostringstream & stream, btcnew::uint128_t value, char group_sep, const std::string & groupings)
{
	auto largestPow10 = btcnew::uint256_t (1);
	int dec_count = 1;
	while (1)
	{
		auto next = largestPow10 * 10;
		if (next > value)
		{
			break;
		}
		largestPow10 = next;
		dec_count++;
	}

	if (dec_count > 39)
	{
		// Impossible.
		return;
	}

	// This could be cached per-locale.
	bool emit_group[39];
	if (group_sep != 0)
	{
		int group_index = 0;
		int group_count = 0;
		for (int i = 0; i < dec_count; i++)
		{
			group_count++;
			if (group_count > groupings[group_index])
			{
				group_index = std::min (group_index + 1, (int)groupings.length () - 1);
				group_count = 1;
				emit_group[i] = true;
			}
			else
			{
				emit_group[i] = false;
			}
		}
	}

	auto reduce = btcnew::uint128_t (largestPow10);
	btcnew::uint128_t rem = value;
	while (reduce > 0)
	{
		auto val = rem / reduce;
		rem -= val * reduce;
		stream << val;
		dec_count--;
		if (group_sep != 0 && emit_group[dec_count] && reduce > 1)
		{
			stream << group_sep;
		}
		reduce /= 10;
	}
}

std::string format_balance (btcnew::uint128_t balance, btcnew::uint128_t scale, int precision, bool group_digits, char thousands_sep, char decimal_point, std::string & grouping)
{
	std::ostringstream stream;
	auto int_part = balance / scale;
	auto frac_part = balance % scale;
	auto prec_scale = scale;
	for (int i = 0; i < precision; i++)
	{
		prec_scale /= 10;
	}
	if (int_part == 0 && frac_part > 0 && frac_part / prec_scale == 0)
	{
		// Display e.g. "< 0.01" rather than 0.
		stream << "< ";
		if (precision > 0)
		{
			stream << "0";
			stream << decimal_point;
			for (int i = 0; i < precision - 1; i++)
			{
				stream << "0";
			}
		}
		stream << "1";
	}
	else
	{
		format_dec (stream, int_part, group_digits && grouping.length () > 0 ? thousands_sep : 0, grouping);
		if (precision > 0 && frac_part > 0)
		{
			stream << decimal_point;
			format_frac (stream, frac_part, scale, precision);
		}
	}
	return stream.str ();
}

std::string btcnew::uint128_union::format_balance (btcnew::uint128_t scale, int precision, bool group_digits)
{
	auto thousands_sep = std::use_facet<std::numpunct<char>> (std::locale ()).thousands_sep ();
	auto decimal_point = std::use_facet<std::numpunct<char>> (std::locale ()).decimal_point ();
	std::string grouping = "\3";
	return ::format_balance (number (), scale, precision, group_digits, thousands_sep, decimal_point, grouping);
}

std::string btcnew::uint128_union::format_balance (btcnew::uint128_t scale, int precision, bool group_digits, const std::locale & locale)
{
	auto thousands_sep = std::use_facet<std::moneypunct<char>> (locale).thousands_sep ();
	auto decimal_point = std::use_facet<std::moneypunct<char>> (locale).decimal_point ();
	std::string grouping = std::use_facet<std::moneypunct<char>> (locale).grouping ();
	return ::format_balance (number (), scale, precision, group_digits, thousands_sep, decimal_point, grouping);
}

void btcnew::uint128_union::clear ()
{
	qwords.fill (0);
}

bool btcnew::uint128_union::is_zero () const
{
	return qwords[0] == 0 && qwords[1] == 0;
}

std::string btcnew::uint128_union::to_string () const
{
	std::string result;
	encode_hex (result);
	return result;
}

std::string btcnew::uint128_union::to_string_dec () const
{
	std::string result;
	encode_dec (result);
	return result;
}

btcnew::hash_or_account::hash_or_account (uint64_t value_a) :
raw (value_a)
{
}

bool btcnew::hash_or_account::is_zero () const
{
	return raw.is_zero ();
}

void btcnew::hash_or_account::clear ()
{
	raw.clear ();
}

bool btcnew::hash_or_account::decode_hex (std::string const & text_a)
{
	return raw.decode_hex (text_a);
}

bool btcnew::hash_or_account::decode_account (std::string const & source_a)
{
	return account.decode_account (source_a);
}

std::string btcnew::hash_or_account::to_string () const
{
	return raw.to_string ();
}

std::string btcnew::hash_or_account::to_account () const
{
	return account.to_account ();
}

btcnew::hash_or_account::operator btcnew::block_hash const & () const
{
	return hash;
}

btcnew::hash_or_account::operator btcnew::account const & () const
{
	return account;
}

btcnew::hash_or_account::operator btcnew::uint256_union const & () const
{
	return raw;
}

btcnew::block_hash const & btcnew::root::previous () const
{
	return hash;
}

bool btcnew::hash_or_account::operator== (btcnew::hash_or_account const & hash_or_account_a) const
{
	return bytes == hash_or_account_a.bytes;
}

bool btcnew::hash_or_account::operator!= (btcnew::hash_or_account const & hash_or_account_a) const
{
	return !(*this == hash_or_account_a);
}

std::string btcnew::to_string_hex (uint64_t const value_a)
{
	std::stringstream stream;
	stream << std::hex << std::noshowbase << std::setw (16) << std::setfill ('0');
	stream << value_a;
	return stream.str ();
}

bool btcnew::from_string_hex (std::string const & value_a, uint64_t & target_a)
{
	auto error (value_a.empty ());
	if (!error)
	{
		error = value_a.size () > 16;
		if (!error)
		{
			std::stringstream stream (value_a);
			stream << std::hex << std::noshowbase;
			try
			{
				uint64_t number_l;
				stream >> number_l;
				target_a = number_l;
				if (!stream.eof ())
				{
					error = true;
				}
			}
			catch (std::runtime_error &)
			{
				error = true;
			}
		}
	}
	return error;
}

std::string btcnew::to_string (double const value_a, int const precision_a)
{
	std::stringstream stream;
	stream << std::setprecision (precision_a) << std::fixed;
	stream << value_a;
	return stream.str ();
}

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4146) // warning C4146: unary minus operator applied to unsigned type, result still unsigned
#endif

uint64_t btcnew::difficulty::from_multiplier (double const multiplier_a, uint64_t const base_difficulty_a)
{
	assert (multiplier_a > 0.);
	btcnew::uint128_t reverse_difficulty ((-base_difficulty_a) / multiplier_a);
	if (reverse_difficulty > std::numeric_limits<std::uint64_t>::max ())
	{
		return 0;
	}
	else if (reverse_difficulty != 0 || base_difficulty_a == 0 || multiplier_a < 1.)
	{
		return -(static_cast<uint64_t> (reverse_difficulty));
	}
	else
	{
		return std::numeric_limits<std::uint64_t>::max ();
	}
}

double btcnew::difficulty::to_multiplier (uint64_t const difficulty_a, uint64_t const base_difficulty_a)
{
	assert (difficulty_a > 0);
	return static_cast<double> (-base_difficulty_a) / (-difficulty_a);
}

#ifdef _WIN32
#pragma warning(pop)
#endif

btcnew::public_key::operator btcnew::link const & () const
{
	return reinterpret_cast<btcnew::link const &> (*this);
}

btcnew::public_key::operator btcnew::root const & () const
{
	return reinterpret_cast<btcnew::root const &> (*this);
}

btcnew::public_key::operator btcnew::hash_or_account const & () const
{
	return reinterpret_cast<btcnew::hash_or_account const &> (*this);
}

btcnew::block_hash::operator btcnew::link const & () const
{
	return reinterpret_cast<btcnew::link const &> (*this);
}

btcnew::block_hash::operator btcnew::root const & () const
{
	return reinterpret_cast<btcnew::root const &> (*this);
}

btcnew::block_hash::operator btcnew::hash_or_account const & () const
{
	return reinterpret_cast<btcnew::hash_or_account const &> (*this);
}

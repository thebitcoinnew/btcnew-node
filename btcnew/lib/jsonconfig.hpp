#pragma once

#include <btcnew/boost/asio.hpp>
#include <btcnew/lib/configbase.hpp>
#include <btcnew/lib/errors.hpp>
#include <btcnew/lib/utility.hpp>

#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <fstream>

namespace btcnew
{
/** Manages a node in a boost configuration tree. */
class jsonconfig : public btcnew::configbase
{
public:
	jsonconfig () :
	tree (tree_default)
	{
		error = std::make_shared<btcnew::error> ();
	}

	jsonconfig (boost::property_tree::ptree & tree_a, std::shared_ptr<btcnew::error> error_a = nullptr) :
	btcnew::configbase (error_a), tree (tree_a)
	{
		if (!error)
		{
			error = std::make_shared<btcnew::error> ();
		}
	}

	/**
	 * Reads a json object from the stream 
	 * @return btcnew::error&, including a descriptive error message if the config file is malformed.
	 */
	btcnew::error & read (boost::filesystem::path const & path_a)
	{
		std::fstream stream;
		open_or_create (stream, path_a.string ());
		if (!stream.fail ())
		{
			try
			{
				boost::property_tree::read_json (stream, tree);
			}
			catch (std::runtime_error const & ex)
			{
				auto pos (stream.tellg ());
				if (pos != std::streampos (0))
				{
					*error = ex;
				}
			}
			stream.close ();
		}
		return *error;
	}

	/**
	 * Reads a json object from the stream and if it was changed, write the object back to the stream.
	 * @return btcnew::error&, including a descriptive error message if the config file is malformed.
	 */
	template <typename T>
	btcnew::error & read_and_update (T & object, boost::filesystem::path const & path_a)
	{
		auto file_exists (boost::filesystem::exists (path_a));
		read (path_a);
		if (!*error)
		{
			std::fstream stream;
			auto updated (false);
			*error = object.deserialize_json (updated, *this);
			if (!*error && updated)
			{
				// Before updating the config file during an upgrade make a backup first
				if (file_exists)
				{
					create_backup_file (path_a);
				}
				stream.open (path_a.string (), std::ios_base::out | std::ios_base::trunc);
				try
				{
					boost::property_tree::write_json (stream, tree);
				}
				catch (std::runtime_error const & ex)
				{
					*error = ex;
				}
				stream.close ();
			}
		}
		return *error;
	}

	void write (boost::filesystem::path const & path_a)
	{
		std::fstream stream;
		open_or_create (stream, path_a.string ());
		write (stream);
	}

	void write (std::ostream & stream_a) const
	{
		boost::property_tree::write_json (stream_a, tree);
	}

	void read (std::istream & stream_a)
	{
		boost::property_tree::read_json (stream_a, tree);
	}

	/** Open configuration file, create if necessary */
	void open_or_create (std::fstream & stream_a, std::string const & path_a)
	{
		if (!boost::filesystem::exists (path_a))
		{
			// Create temp stream to first create the file
			std::ofstream stream (path_a);

			// Set permissions before opening otherwise Windows only has read permissions
			btcnew::set_secure_perm_file (path_a);
		}

		stream_a.open (path_a);
	}

	/** Takes a filepath, appends '_backup_<timestamp>' to the end (but before any extension) and saves that file in the same directory */
	void create_backup_file (boost::filesystem::path const & filepath_a)
	{
		auto extension = filepath_a.extension ();
		auto filename_without_extension = filepath_a.filename ().replace_extension ("");
		auto orig_filepath = filepath_a;
		auto & backup_path = orig_filepath.remove_filename ();
		auto backup_filename = filename_without_extension;
		backup_filename += "_backup_";
		backup_filename += std::to_string (std::chrono::system_clock::now ().time_since_epoch ().count ());
		backup_filename += extension;
		auto backup_filepath = backup_path / backup_filename;

		boost::filesystem::copy_file (filepath_a, backup_filepath);
	}

	/** Returns the boost property node managed by this instance */
	boost::property_tree::ptree const & get_tree ()
	{
		return tree;
	}

	/** Returns true if the property tree node is empty */
	bool empty () const
	{
		return tree.empty ();
	}

	boost::optional<jsonconfig> get_optional_child (std::string const & key_a)
	{
		boost::optional<jsonconfig> child_config;
		auto child = tree.get_child_optional (key_a);
		if (child)
		{
			return jsonconfig (child.get (), error);
		}
		return child_config;
	}

	jsonconfig get_required_child (std::string const & key_a)
	{
		auto child = tree.get_child_optional (key_a);
		if (!child)
		{
			*error = btcnew::error_config::missing_value;
			error->set_message ("Missing configuration node: " + key_a);
		}
		return child ? jsonconfig (child.get (), error) : *this;
	}

	jsonconfig & put_child (std::string const & key_a, btcnew::jsonconfig & conf_a)
	{
		tree.add_child (key_a, conf_a.get_tree ());
		return *this;
	}

	jsonconfig & replace_child (std::string const & key_a, btcnew::jsonconfig & conf_a)
	{
		tree.erase (key_a);
		put_child (key_a, conf_a);
		return *this;
	}

	/** Set value for the given key. Any existing value will be overwritten. */
	template <typename T>
	jsonconfig & put (std::string const & key, T const & value)
	{
		tree.put (key, value);
		return *this;
	}

	/** Push array element */
	template <typename T>
	jsonconfig & push (T const & value)
	{
		boost::property_tree::ptree entry;
		entry.put ("", value);
		tree.push_back (std::make_pair ("", entry));
		return *this;
	}

	/** Returns true if \p key_a is present */
	bool has_key (std::string const & key_a)
	{
		return tree.find (key_a) != tree.not_found ();
	}

	/** Erase the property of given key */
	jsonconfig & erase (std::string const & key_a)
	{
		tree.erase (key_a);
		return *this;
	}

	/** Iterate array entries */
	template <typename T>
	jsonconfig & array_entries (std::function<void (T)> callback)
	{
		for (auto & entry : tree)
		{
			callback (entry.second.get<T> (""));
		}
		return *this;
	}

	/** Get optional, using \p default_value if \p key is missing. */
	template <typename T>
	jsonconfig & get_optional (std::string const & key, T & target, T default_value)
	{
		get_config<T> (true, key, target, default_value);
		return *this;
	}

	/**
	 * Get optional value, using the current value of \p target as the default if \p key is missing.
	 * @return May return btcnew::error_config::invalid_value
	 */
	template <typename T>
	jsonconfig & get_optional (std::string const & key, T & target)
	{
		get_config<T> (true, key, target, target);
		return *this;
	}

	/** Return a boost::optional<T> for the given key */
	template <typename T>
	boost::optional<T> get_optional (std::string const & key)
	{
		boost::optional<T> res;
		if (has_key (key))
		{
			T target{};
			get_config<T> (true, key, target, target);
			res = target;
		}
		return res;
	}

	/** Get value, using the current value of \p target as the default if \p key is missing. */
	template <typename T>
	jsonconfig & get (std::string const & key, T & target)
	{
		get_config<T> (true, key, target, target);
		return *this;
	}

	/**
	 * Get value of optional key. Use default value of data type if missing.
	 */
	template <typename T>
	T get (std::string const & key)
	{
		T target{};
		get_config<T> (true, key, target, target);
		return target;
	}

	/**
	 * Get required value.
	 * @note May set btcnew::error_config::missing_value if \p key is missing, btcnew::error_config::invalid_value if value is invalid.
	 */
	template <typename T>
	jsonconfig & get_required (std::string const & key, T & target)
	{
		get_config<T> (false, key, target);
		return *this;
	}

protected:
	template <typename T, typename = std::enable_if_t<btcnew::is_lexical_castable<T>::value>>
	jsonconfig & get_config (bool optional, std::string key, T & target, T default_value = T ())
	{
		try
		{
			auto val (tree.get<std::string> (key));
			if (!boost::conversion::try_lexical_convert<T> (val, target))
			{
				conditionally_set_error<T> (btcnew::error_config::invalid_value, optional, key);
			}
		}
		catch (boost::property_tree::ptree_bad_path const &)
		{
			if (!optional)
			{
				conditionally_set_error<T> (btcnew::error_config::missing_value, optional, key);
			}
			else
			{
				target = default_value;
			}
		}
		catch (std::runtime_error & ex)
		{
			conditionally_set_error<T> (ex, optional, key);
		}
		return *this;
	}

	// boost's lexical cast doesn't handle (u)int8_t
	template <typename T, typename = std::enable_if_t<std::is_same<T, uint8_t>::value>>
	jsonconfig & get_config (bool optional, std::string key, uint8_t & target, uint8_t default_value = T ())
	{
		int64_t tmp;
		try
		{
			auto val (tree.get<std::string> (key));
			if (!boost::conversion::try_lexical_convert<int64_t> (val, tmp) || tmp < 0 || tmp > 255)
			{
				conditionally_set_error<T> (btcnew::error_config::invalid_value, optional, key);
			}
			else
			{
				target = static_cast<uint8_t> (tmp);
			}
		}
		catch (boost::property_tree::ptree_bad_path const &)
		{
			if (!optional)
			{
				conditionally_set_error<T> (btcnew::error_config::missing_value, optional, key);
			}
			else
			{
				target = default_value;
			}
		}
		catch (std::runtime_error & ex)
		{
			conditionally_set_error<T> (ex, optional, key);
		}
		return *this;
	}

	template <typename T, typename = std::enable_if_t<std::is_same<T, bool>::value>>
	jsonconfig & get_config (bool optional, std::string key, bool & target, bool default_value = false)
	{
		auto bool_conv = [this, &target, &key, optional] (std::string val) {
			if (val == "true")
			{
				target = true;
			}
			else if (val == "false")
			{
				target = false;
			}
			else if (!*error)
			{
				conditionally_set_error<T> (btcnew::error_config::invalid_value, optional, key);
			}
		};
		try
		{
			auto val (tree.get<std::string> (key));
			bool_conv (val);
		}
		catch (boost::property_tree::ptree_bad_path const &)
		{
			if (!optional)
			{
				conditionally_set_error<T> (btcnew::error_config::missing_value, optional, key);
			}
			else
			{
				target = default_value;
			}
		}
		catch (std::runtime_error & ex)
		{
			conditionally_set_error<T> (ex, optional, key);
		}
		return *this;
	}

	template <typename T, typename = std::enable_if_t<std::is_same<T, boost::asio::ip::address_v6>::value>>
	jsonconfig & get_config (bool optional, std::string key, boost::asio::ip::address_v6 & target, boost::asio::ip::address_v6 default_value = T ())
	{
		try
		{
			auto address_l (tree.get<std::string> (key));
			boost::system::error_code bec;
			target = boost::asio::ip::address_v6::from_string (address_l, bec);
			if (bec)
			{
				conditionally_set_error<T> (btcnew::error_config::invalid_value, optional, key);
			}
		}
		catch (boost::property_tree::ptree_bad_path const &)
		{
			if (!optional)
			{
				conditionally_set_error<T> (btcnew::error_config::missing_value, optional, key);
			}
			else
			{
				target = default_value;
			}
		}
		return *this;
	}

private:
	/** The property node being managed */
	boost::property_tree::ptree & tree;
	boost::property_tree::ptree tree_default;
};
}

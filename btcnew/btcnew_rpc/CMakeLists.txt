add_executable (btcnew_rpc
	entry.cpp)

target_link_libraries (btcnew_rpc
	rpc
	secure
	Boost::filesystem
	Boost::log_setup
	Boost::log
	Boost::program_options
	Boost::system
	Boost::thread
	Boost::boost)

target_compile_definitions(btcnew_rpc
	PUBLIC
		-DACTIVE_NETWORK=${ACTIVE_NETWORK}
	PRIVATE
		-DTAG_VERSION_STRING=${TAG_VERSION_STRING}
		-DGIT_COMMIT_HASH=${GIT_COMMIT_HASH})

if ((BTCNEW_GUI OR RAIBLOCKS_GUI) AND NOT APPLE)
	install(TARGETS btcnew_rpc
			RUNTIME DESTINATION .
	)
endif()

add_executable (btcnew_node
	daemon.cpp
	daemon.hpp
	entry.cpp)

target_link_libraries (btcnew_node
	node
	rpc
	secure
	argon2
	Boost::boost
	${PLATFORM_LIBS}
)

if (BTCNEW_STACKTRACE_BACKTRACE)
	target_link_libraries(btcnew_node backtrace)
endif ()

target_compile_definitions(btcnew_node
	PRIVATE
		-DTAG_VERSION_STRING=${TAG_VERSION_STRING}
		-DGIT_COMMIT_HASH=${GIT_COMMIT_HASH}
		-DBOOST_PROCESS_SUPPORTED=${BOOST_PROCESS_SUPPORTED})

set_target_properties (btcnew_node
	PROPERTIES
		COMPILE_FLAGS
			"-DQT_NO_KEYWORDS -DBOOST_ASIO_HAS_STD_ARRAY=1")

add_custom_command(TARGET btcnew_node
				   POST_BUILD
				   COMMAND btcnew_node --generate_config node > ${PROJECT_BINARY_DIR}/config-node.toml.sample
				   COMMAND btcnew_node --generate_config rpc > ${PROJECT_BINARY_DIR}/config-rpc.toml.sample)

if ((BTCNEW_GUI OR RAIBLOCKS_GUI) AND NOT APPLE)
	install(TARGETS btcnew_node
			RUNTIME DESTINATION .
	)
endif()

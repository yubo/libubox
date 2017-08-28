cmake_minimum_required(VERSION 2.6)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin/examples)
file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH})

if (BUILD_EXAMPLES)
	project(ubox-examples C)
	add_definitions(-O1 -Wall -Werror --std=gnu99 -g3)

	add_executable(jrpc_server jrpc_server.c)
	target_link_libraries(jrpc_server m ubox)

	add_executable(jrpc_client jrpc_client.c)
	target_link_libraries(jrpc_client m ubox)

	add_executable(blobmsg blobmsg.c)
	target_link_libraries(blobmsg ubox)

	add_executable(ustream ustream.c)
	target_link_libraries(ustream ubox)

	add_executable(runqueue runqueue.c)
	target_link_libraries(runqueue ubox m)

	add_executable(json_script json_script.c)
	target_link_libraries(json_script ubox)

	add_executable(timer timer.c)
	target_link_libraries(timer ubox)

	add_executable(format format.c)
	target_link_libraries(format ubox)
endif(BUILD_EXAMPLES)

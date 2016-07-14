cmake_minimum_required(VERSION 2.6)

if (BUILD_EXAMPLES)
	project(ubox-examples C)
	add_definitions(-O1 -Wall -Werror --std=gnu99 -g3)

	add_executable(jrpc_server-example jrpc_server-example.c)
	target_link_libraries(jrpc_server-example m ubox)

	add_executable(jrpc_client-example jrpc_client-example.c)
	target_link_libraries(jrpc_client-example m ubox)

	add_executable(blobmsg-example blobmsg-example.c)
	target_link_libraries(blobmsg-example ubox)

	add_executable(ustream-example ustream-example.c)
	target_link_libraries(ustream-example ubox)

	add_executable(runqueue-example runqueue-example.c)
	target_link_libraries(runqueue-example ubox m)

	add_executable(json_script-example json_script-example.c)
	target_link_libraries(json_script-example ubox)
endif(BUILD_EXAMPLES)

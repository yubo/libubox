cmake_minimum_required(VERSION 2.6)

set(SOURCES avl.c avl-cmp.c blob.c blobmsg.c uloop.c usock.c ustream.c ustream-fd.c vlist.c utils.c safe_list.c runqueue.c md5.c kvlist.c ulog.c base64.c json.c jsonrpc.c blobmsg_json.c printbuf.c json_script.c)

add_library(ubox SHARED ${SOURCES})
add_library(ubox-static STATIC ${SOURCES})
set_target_properties(ubox-static PROPERTIES OUTPUT_NAME ubox)
target_link_libraries(ubox rt m)

file(GLOB headers *.h)
install(FILES ${headers} DESTINATION include/libubox)
install(TARGETS ubox ubox-static
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)


add_executable(jshn jshn.c)
target_link_libraries(jshn ubox)
install(TARGETS jshn
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)

file(GLOB scripts sh/*.sh)
install(FILES ${scripts} DESTINATION share/libubox)

add_subdirectory(lua)

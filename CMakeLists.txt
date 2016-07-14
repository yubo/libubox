cmake_minimum_required(VERSION 2.6)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckFunctionExists)

PROJECT(ubox C)
ADD_DEFINITIONS(-O0 -Wall -Werror --std=gnu99 -g -Wmissing-declarations -I${CMAKE_CURRENT_SOURCE_DIR})

OPTION(BUILD_LUA "build Lua plugin" ON)
OPTION(BUILD_EXAMPLES "build examples" ON)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

SET(SOURCES avl.c avl-cmp.c blob.c blobmsg.c uloop.c usock.c ustream.c ustream-fd.c vlist.c utils.c safe_list.c runqueue.c md5.c kvlist.c ulog.c base64.c json.c jsonrpc.c blobmsg_json.c printbuf.c json_script.c)

ADD_LIBRARY(ubox SHARED ${SOURCES})
ADD_LIBRARY(ubox-static STATIC ${SOURCES})
SET_TARGET_PROPERTIES(ubox-static PROPERTIES OUTPUT_NAME ubox)

TARGET_LINK_LIBRARIES(ubox rt m)

FILE(GLOB headers *.h)
INSTALL(FILES ${headers}
	DESTINATION include/libubox
)
INSTALL(TARGETS ubox ubox-static
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)

ADD_SUBDIRECTORY(lua)
ADD_SUBDIRECTORY(examples)

ADD_EXECUTABLE(jshn jshn.c)
TARGET_LINK_LIBRARIES(jshn ubox)


INSTALL(TARGETS jshn
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)

FILE(GLOB scripts sh/*.sh)
INSTALL(FILES ${scripts}
	DESTINATION share/libubox
)


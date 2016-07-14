cmake_minimum_required(VERSION 2.6)

project(uloop C)

set(CMAKE_INSTALL_PREFIX /)


if(NOT LUA_CFLAGS)
	pkg_search_module(LUA lua5.1 lua-5.1)
endif()

add_definitions(-Os -Wall -Werror --std=gnu99 -g3 -I.. ${LUA_CFLAGS})
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

if(APPLE)
	set(CMAKE_SHARED_MODULE_CREATE_C_FLAGS "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} -undefined dynamic_lookup")
endif(APPLE)

if(NOT LUAPATH)
	execute_process(
		COMMAND  lua -e "for k in string.gmatch(package.cpath .. \";\", \"([^;]+)/..so;\") do if k:sub(1,1) == \"/\" then print(k) break end end"
		OUTPUT_VARIABLE LUAPATH
		RESULT_VARIABLE LUA_CHECK_RES
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	if(BUILD_LUA)
		if(NOT ${LUA_CHECK_RES} EQUAL 0 OR "${LUAPATH}" EQUAL "")
			MESSAGE(SEND_ERROR "Lua was not found on your system")
		endif()
	endif()
endif()

if(BUILD_LUA)
	add_library(uloop_lua MODULE uloop.c)
	set_target_properties(uloop_lua PROPERTIES
		OUTPUT_NAME uloop
		PREFIX ""
	)
	target_link_libraries(uloop_lua ubox)

	install(TARGETS uloop_lua
		LIBRARY DESTINATION ${LUAPATH}
	)
endif()

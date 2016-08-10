cmake_minimum_required(VERSION 2.6)
project(ubox C)

include(CheckLibraryExists)
include(CheckFunctionExists)
include(FindPkgConfig)

option(BUILD_LUA "build Lua plugin" ON)
option(BUILD_EXAMPLES "build examples" ON)
option(DEBUG "debug examples" ON)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

add_definitions(-Wall -Werror --std=gnu99 -Wmissing-declarations -I${CMAKE_CURRENT_SOURCE_DIR}/include)

if(DEBUG)
	add_definitions(-DDEBUG -g -O0)
	set(CMAKE_EXE_LINKER_FLAGS "-rdynamic")
else(DEBUG)
	add_definitions(-O3)
endif(DEBUG)

add_subdirectory(src)
add_subdirectory(examples)


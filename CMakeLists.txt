# Copyright David Stone 2018.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.26 FATAL_ERROR)

project(concurrent_queue LANGUAGES CXX)

enable_testing()

get_directory_property(has_parent PARENT_DIRECTORY)
if (NOT has_parent)
	set(CMAKE_CXX_STANDARD 23)
	set(CMAKE_CXX_STANDARD_REQUIRED On)
	set(CMAKE_CXX_EXTENSIONS Off)

	set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API "2182bf5c-ef0d-489a-91da-49dbc3090d2a")
	set(CMake_TEST_CXXModules_UUID "a246741c-d067-4019-a8fb-3d16b0c9d1d3")
	set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)

	add_subdirectory(dependencies/cmake_strict_defaults)
	add_subdirectory(dependencies/bounded)
	add_subdirectory(dependencies/Catch2 EXCLUDE_FROM_ALL)
	add_subdirectory(dependencies/numeric-traits)
	add_subdirectory(dependencies/operators)

	set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable testing benchmark.")
	add_subdirectory(dependencies/benchmark EXCLUDE_FROM_ALL)
	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		target_compile_options(benchmark PUBLIC
			"-Wno-global-constructors"
			"-Wno-shift-sign-overflow"
		)
	endif()
endif()	

add_library(concurrent_queue STATIC)

target_link_libraries(concurrent_queue
	PUBLIC
		strict_defaults_interface
		bounded
		containers
	PRIVATE
		strict_defaults
)

target_sources(concurrent_queue PUBLIC
	FILE_SET CXX_MODULES
	BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
	FILES
		source/queue.cpp
)

if(NOT WINDOWS)
	target_link_libraries(concurrent_queue INTERFACE
		pthread
	)
endif()

add_executable(concurrent_queue_test
	test/queue.cpp
)
target_link_libraries(concurrent_queue_test
	concurrent_queue
	strict_defaults
	Catch2::Catch2WithMain
)
add_test(concurrent_queue_test concurrent_queue_test)

add_executable(concurrent_queue_benchmark
	test/benchmark.cpp
)
target_link_libraries(concurrent_queue_benchmark
	benchmark
	concurrent_queue
	strict_defaults
)

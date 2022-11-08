# Copyright David Stone 2018.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(concurrent_queue LANGUAGES CXX)

enable_testing()

get_directory_property(has_parent PARENT_DIRECTORY)
if (NOT has_parent)
	set(CMAKE_CXX_STANDARD 23)
	set(CMAKE_CXX_STANDARD_REQUIRED On)
	set(CMAKE_CXX_EXTENSIONS Off)

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

add_library(concurrent_queue INTERFACE)

target_link_libraries(concurrent_queue INTERFACE
	strict_defaults_interface
	bounded
	containers
)

target_include_directories(concurrent_queue INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
)

if(NOT WINDOWS)
	target_link_libraries(concurrent_queue INTERFACE
		pthread
	)
endif()

add_executable(concurrent_queue_test
	source/concurrent/queue.cpp
)
target_link_libraries(concurrent_queue_test
	concurrent_queue
	strict_defaults
)
add_test(concurrent_queue_test concurrent_queue_test)

add_executable(concurrent_queue_benchmark
	source/concurrent/benchmark.cpp
)
target_link_libraries(concurrent_queue_benchmark
	benchmark
	concurrent_queue
	strict_defaults
)

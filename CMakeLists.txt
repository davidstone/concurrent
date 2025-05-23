# Copyright David Stone 2018.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

project(concurrent LANGUAGES CXX)

enable_testing()

get_directory_property(has_parent PARENT_DIRECTORY)
if (NOT has_parent)
	set(CMAKE_CXX_STANDARD 26)
	set(CMAKE_CXX_STANDARD_REQUIRED On)
	set(CMAKE_CXX_EXTENSIONS Off)

	add_subdirectory(dependencies/cmake_strict_defaults)
	add_subdirectory(dependencies/bounded)
	add_subdirectory(dependencies/Catch2 EXCLUDE_FROM_ALL)
	add_subdirectory(dependencies/numeric-traits)
	add_subdirectory(dependencies/operators)
	add_subdirectory(dependencies/std_module)

	set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable testing benchmark.")
	add_subdirectory(dependencies/benchmark EXCLUDE_FROM_ALL)
	get_property(benchmark_include_dirs TARGET benchmark PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
	set_property(TARGET benchmark PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${benchmark_include_dirs}")
	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		target_compile_options(benchmark PUBLIC
			"-Wno-global-constructors"
		)
	endif()
endif()	

add_library(concurrent STATIC)

target_link_libraries(concurrent
	PUBLIC
		bounded
		containers
		std_module
		strict_defaults_interface
	PRIVATE
		strict_defaults
)

target_sources(concurrent PUBLIC
	FILE_SET CXX_MODULES
	BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
	FILES
		source/concurrent.cpp
		source/locked_access.cpp
		source/queue.cpp
)

if(NOT WINDOWS)
	target_link_libraries(concurrent INTERFACE
		pthread
	)
endif()

add_executable(concurrent_test
	test/queue.cpp
)
target_link_libraries(concurrent_test
	Catch2::Catch2WithMain
	concurrent
	strict_defaults
)
add_test(concurrent_test concurrent_test)

add_executable(concurrent_queue_benchmark
	test/benchmark.cpp
)
target_link_libraries(concurrent_queue_benchmark
	benchmark
	concurrent
	strict_defaults
)

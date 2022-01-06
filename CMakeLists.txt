# Copyright David Stone 2018.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(concurrent_queue LANGUAGES CXX)

enable_testing()

get_directory_property(has_parent PARENT_DIRECTORY)
if (NOT has_parent)
	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_CXX_STANDARD_REQUIRED On)
	set(CMAKE_CXX_EXTENSIONS Off)

	add_subdirectory(dependencies/cmake_strict_defaults)
	find_package(Boost COMPONENTS chrono program_options system thread REQUIRED)
	add_library(boost INTERFACE IMPORTED)
	target_compile_definitions(boost INTERFACE BOOST_ALL_NO_LIB)
	set_property(TARGET boost PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
endif()

add_library(concurrent_queue INTERFACE)

target_link_libraries(concurrent_queue INTERFACE
	boost
	boost_chrono
	boost_program_options
	boost_system
	boost_thread
	strict_defaults_interface
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
)

add_test(concurrent_queue_test concurrent_queue_test)

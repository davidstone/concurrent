# Copyright David Stone 2018.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
project(concurrent_queue LANGUAGES CXX)

find_package(Boost COMPONENTS chrono program_options system thread REQUIRED)
add_library(boost INTERFACE IMPORTED)
target_compile_definitions(boost INTERFACE BOOST_ALL_NO_LIB)
set_property(TARGET boost PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR})
enable_testing()

add_library(concurrent_queue INTERFACE)

target_sources(concurrent_queue INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/include/boost/concurrent/queue.hpp
)

target_include_directories(concurrent_queue INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
)

target_link_libraries(concurrent_queue INTERFACE
	boost
	boost_chrono
	boost_program_options
	boost_system
	boost_thread
)

if(MSVC)
	target_compile_options(concurrent_queue INTERFACE
		"/std:c++latest"
	)
else()
	target_compile_options(concurrent_queue INTERFACE
		"-std=c++2a"
	)
endif()

add_executable(concurrent_queue_test
	source/queue.cpp
)

target_link_libraries(concurrent_queue_test
	concurrent_queue
)

function(set_sanitizers target)
	foreach(SANITIZER -fsanitize=undefined -fsanitize=address)
		target_compile_options(${target} PRIVATE
			$<$<CONFIG:Debug>:${SANITIZER}>
		)
		target_link_libraries(${target}
			$<$<CONFIG:Debug>:${SANITIZER}>
		)
	endforeach()
endfunction()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	set(COMPILE_OPTIONS
		$<$<CONFIG:Debug>:-O1;-march=native>
		$<$<CONFIG:Release>:-march=native>
		"-Weverything"
		"-Werror"
		"-Wno-c++98-compat"
		"-Wno-c++98-compat-pedantic"
		# -Wmissing-braces wants two sets of braces when using constructs such
		# as std::array
		"-Wno-missing-braces"
		"-Wno-newline-eof"
		"-Wno-padded"
		# -Wundefined-func-template has a bug that triggers on deduction guides
		"-Wno-undefined-func-template"
		# -Wunneeded-member-function has a bug that blocks constexpr libraries
		# from using it: https://llvm.org/bugs/show_bug.cgi?id=25084
		"-Wno-unneeded-member-function"
		# Has a bug that warns even for templates that are used
		"-Wno-unused-template"
		# -Wweak-vtables is incompatible with header-only libraries
		"-Wno-weak-vtables"
		# -Wzero-as-null-pointer-constant does not work with the `operator<=>`
		# emulation
		"-Wno-zero-as-null-pointer-constant"
	)
	target_compile_options(concurrent_queue_test PRIVATE ${COMPILE_OPTIONS})

	set_sanitizers(concurrent_queue_test)

elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(COMPILE_OPTIONS
		$<$<CONFIG:Debug>:-Og;-march=native>
		$<$<CONFIG:Release>:-O3;-march=native>
		"-Wall"
		"-Wextra"
		"-Wpedantic"
		"-Wabi"
		"-Wcast-align"
		"-Wcast-qual"
		# It seems impossible to initialize bit fields with this warning on
		#"-Wconversion"
		"-Wctor-dtor-privacy"
		"-Wdisabled-optimization"
		"-Wdouble-promotion"
		# -Weffc++ includes a warning if all data members are not explicitly
		# initialized in the initializer list. I intentionally do not do this in
		# many cases. This would be more useful as a collection of warnings
		# like -Wall instead of a single warning on its own.
		# "-Weffc++"
		# -Wfloat-equal warns for safe comparisons
		# "-Wfloat-equal"
		"-Wformat=2"
		"-Winit-self"
		"-Winvalid-pch"
		# -Wlogical-op warns for expressions that happen to be equal in a
		# template instantiation
		# "-Wlogical-op"
		"-Wmissing-declarations"
		# -Wmissing-format-attribute is not used because I do not use GNU
		# extensions. Same for -Wsuggest-attribute and several others.
		"-Wmissing-include-dirs"
		#"-Wnoexcept"
		"-Wno-non-template-friend"
		"-Wodr"
		"-Wold-style-cast"
		"-Woverloaded-virtual"
		"-Wno-padded"
		"-Wredundant-decls"
		"-Wshadow"
		"-Wsign-conversion"
		# -Wsign-promo triggers on code that is guaranteed safe due to the use
		# of the bounded::integer library. Working around the warning would lead
		# to either less efficient code or more obfuscated code.
		"-Wsign-promo"
		# -Wsuggest-final-methods and -Wsuggest-final-types is a linker warning,
		# so it is not possible to disable it for boost and other third-party
		# libraries by saying they are system headers.
		# "-Wsuggest-final-methods"
		# "-Wsuggest-final-types"
		"-Wstrict-null-sentinel"
		# -Wstrict-overflow=2 warns about comparing two pointers
		"-Wstrict-overflow=1"
		"-Wswitch-default"
		"-Wswitch-enum"
		"-Wtrampolines"
		"-Wundef"
		# -Wunsafe-loop-optimizations causes too many spurious warnings. It may
		# be useful to apply this one periodically and manually verify the
		# results. It generated this warning in my code when I looped over all
		# elements in a vector to apply a set of functions to them (using the
		# range-based for loop).  It is also warning for the constructor of a
		# const array of const std::string where there is no loop in user code.
		# "-Wunsafe-loop-optimizations"
		# -Wunused-but-set-parameter does not properly account for
		# `if constexpr` branches.
		"-Wno-unused-but-set-parameter"
		# -Wuseless-cast is incompatible with BOUNDED_INTEGER_CONDITIONAL
		# "-Wuseless-cast"
		"-Wvector-operation-performance"
		# -Wzero-as-null-pointer-constant does not work with the `operator<=>`
		# emulation
		# "-Wzero-as-null-pointer-constant"
		"-Werror"
	)
	target_compile_options(concurrent_queue_test PRIVATE ${COMPILE_OPTIONS})

	set_sanitizers(concurrent_queue_test)

elseif(MSVC)
	set(COMPILE_OPTIONS
		"/W4"
		"/WX"
	)
	target_compile_options(concurrent_queue_test PRIVATE ${COMPILE_OPTIONS})
endif()

add_test(concurrent_queue_test concurrent_queue_test)

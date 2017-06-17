# Copyright IHS Markit 2017.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

from program import prepend_dir, Program

source_directory = 'source'

include_directories = ['../include']

programs = [
	Program(
		'queue',
		include_directories = include_directories,
		sources = [
			'movable_condition_variable.cpp',
			'movable_mutex.cpp',
			'queue.cpp',
		],
		defines = [
			'BOOST_CHRONO_DONT_PROVIDE_HYBRID_ERROR_HANDLING',
			'BOOST_CHRONO_DONT_PROVIDES_DEPRECATED_IO_SINCE_V2_0_0',
			'BOOST_CHRONO_HEADER_ONLY',
			'BOOST_ERROR_CODE_HEADER_ONLY',
			'BOOST_SYSTEM_NO_DEPRECATED',
		],
		libraries = [
			'boost_program_options',
			'boost_thread',
		],
	),
]


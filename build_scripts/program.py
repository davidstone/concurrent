# Copyright David Stone 2015.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

def prepend_dir(directory, sources):
	"""Remove redundant specification of a directory for multiple sources"""
	return map(lambda source: directory + '/' + source, sources)

class Program:
	def __init__(self, name, sources, defines = [], libraries = [], include_directories = []):
		self.name = name
		self.sources = sources
		self.defines = defines
		self.libraries = libraries
		self.include_directories = include_directories

# Example of use:
#
# example_sources = prepend_dir('cool', ['alright.cpp', 'awesome.cpp'])
# example_sources += prepend_dir('yeah', ['no.cpp', 'maybe.cpp'])
# example_defines = ['NDEBUG']
# example_libraries = ['boost_filesystem']
# example_include_directories = []
#
# example = Program('example_program_name', example_sources, example_defines, example_libraries, example_include_directories)
#
# programs = [example]

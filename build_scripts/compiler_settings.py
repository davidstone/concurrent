# Copyright David Stone 2015.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

class Flags:
	def __init__(self, compiler):
		if compiler == 'gcc':
			from gcc.debug import debug
			from gcc.std import cxx_std
			from gcc.warnings import warnings
			from gcc.optimizations import optimize
		elif compiler == 'clang':
			from clang.debug import debug
			from clang.std import cxx_std
			from clang.warnings import warnings
			from clang.optimizations import optimize
		else:
			raise Exception('Invalid compiler ' + compiler)

		self.debug = debug
		self.cxx_std = cxx_std
		self.warnings = warnings
		self.optimize = optimize
		
def normalize_name(compiler):
	"""Normalize multiple names into one canonical name. Example: g++ -> gcc"""
	import re
	search = re.match('[a-z+]+', compiler.lower())
	if search == None:
		raise Exception('No matching compiler for ' + compiler)
	
	compiler = search.group(0)
	lookup = {}
	lookup['gcc'] = lookup['g++'] = 'gcc'
	lookup['clang'] = lookup['clang++'] = 'clang'
	return lookup[compiler]

class Compiler:
	def __init__(self, name, command, default):
		"""Get the compiler name and the command used to compile.
	
		This allows a user to specify that their compiler is named 'clang', and
		have SCons search in the normal directories for it, or the user can
		specify that their compiler is located at 'arbitrary/path/gcc' and SCons
		will assume that they are actually compiling with gcc. However, if the
		user has clang installed at 'path/g++', then the user must specify both
		the real name of the compiler and the path.
		"""
		
		import os

		if name == None:
			name = os.path.basename(command) if command != None else default

		if command == None:
			command = name
		
		assert name != None
		assert command != None

		self.name = normalize_name(name)
		self.command = command

class Settings:
	def __init__(self, compiler_name, compiler_command, default_compiler):
		self.compiler = Compiler(compiler_name, compiler_command, default_compiler)

		flags = Flags(self.compiler.name)
		self.cc = {
			'debug': flags.warnings + flags.debug.compile_flags + flags.optimize.compile_flags_debug,
			'release': flags.warnings + flags.debug.compile_flags_release + flags.optimize.compile_flags_release
		}
		self.cxx = {
			'debug': flags.cxx_std,
			'release': flags.cxx_std
		}
		self.link = {
			'debug': flags.warnings + flags.cxx_std + flags.debug.link_flags + flags.optimize.link_flags_debug,
			'release': flags.warnings + flags.cxx_std + flags.debug.link_flags_release + flags.optimize.link_flags_release
		}
		self.cpp = {
			'debug': [],
			'release': []
		}

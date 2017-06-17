# Copyright David Stone 2015.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

import os
import multiprocessing

from list_of_sources import source_directory, programs

SetOption('warn', 'no-duplicate-environment')

# Options to improve the default speed of SCons
SetOption('max_drift', 2)
SetOption('implicit_cache', 1)
SetOption('num_jobs', multiprocessing.cpu_count())

AddOption('--compiler', dest = 'compiler', type = 'string', action = 'store', help = 'Name of the compiler to use.')
AddOption('--compiler-command', dest = 'compiler_command', type = 'string', action = 'store', help = 'Command to launch the compiler.')

AddOption('--verbose', dest = 'verbose', action = "store_true", help = 'Print the full compiler output.')

Decider('MD5-timestamp')

default = DefaultEnvironment()

from compiler_settings import Settings
settings = Settings(GetOption('compiler'), GetOption('compiler_command'), default['CXX'])

# This replaces the wall of text caused by compiling with max warnings turned on
# into something a little more readable.
if not GetOption('verbose'):
	default['CXXCOMSTR'] = 'Compiling $TARGET'
	default['LINKCOMSTR'] = 'Linking $TARGET'

# This allows gcc and clang to autodetect whether they should use colors
default['ENV']['TERM'] = os.environ['TERM']

default.Replace(CXX = settings.compiler.command)

build_root = '../build/' + settings.compiler.name + '/'

def setup_environment(version):
	environment = default.Clone()
	environment.Append(CCFLAGS = settings.cc[version])
	environment.Append(CXXFLAGS = settings.cxx[version])
	environment.Append(LINKFLAGS = settings.link[version])
	environment.Append(CPPDEFINES = settings.cpp[version])
	return environment

debug = setup_environment('debug')
release = setup_environment('release')

def build_directory(version, defines):
	return build_root + version + '/' + '-'.join(map(str, defines)) + '/'

def generate_sources(sources, version, defines):
	return [build_directory(version, defines) + source for source in sources]

def create_program(program):
	env_name = { 'debug':debug, 'release':release }
	suffix = { 'debug':'-debug', 'release':'' }
	for version in ['debug', 'release']:
		targets = generate_sources(program.sources, version, program.defines)
		executable_name = program.name + suffix[version]
		env = env_name[version].Clone(LIBS = program.libraries, CPPDEFINES = program.defines)
		env.Append(CPPPATH = program.include_directories)
		env.VariantDir(build_directory(version, program.defines), '../' + source_directory, duplicate = 0)
		env.Program('../' + executable_name, targets)

for program in programs:
	create_program(program)

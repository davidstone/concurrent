# Purpose

The goal of this project is to simplify the use of SCons and various compilers. It is targeted at C++ developers. SCons Template assumes you are willing and able to use new tools, and as such, is willing to sacrifice compatability with older versions of Python, for example.

It currently provides support for gcc and clang.

# How to use

The easiest way to use this is probably to set up a Mercurial subrepo. Then you can have a top-level SConscript file that contains only `SConscript('scons-template/template.py')`.

Rename the file `rename_to_sources.py` to `sources.py` and edit to add in your sources (outlined below). hg subrepos have the unfortunate tendency to overwrite changes without prompting, so this allows you to make changes and freely do hg pulls and updates without losing your list of sources, but still provides a simple template for setting up new projects. For most projects, this is it.

You can specify which compiler to use by passing the flag --compiler=COMPILER, where COMPILER is gcc / g++ or clang / clang++. If you have your compiler installed in a non-standard path, you can do something like --compiler-command=~/compilers/gcc/g++. If you have both a non-standard path and a non-standard name, you have to specify both --compiler and --compiler-command.

## Editing sources.py

`sources.py` must have the following symbols available for import:

1. `source_directory`: The directory your sources are in.
2. `programs`: A list containing all of your programs (provided by class Progam)

Due to the flexibility granted to SCons by allowing you to use Python, you can generate these lists of sources however you want. Included with the program is a simple function, `prepend_dir`. If your directory structure looks like

	project_root/
	|-- build_scripts/
	`-- source/
	   |-- alright/
	   |   |-- light_bulb.cpp
	   |   |-- light_bulb.hpp
	   |   |-- sandwich.cpp
	   |   `-- sandwich.hpp
	   |-- cool/
	   |   |-- death_star.cpp
	   |   |-- death_star.hpp
	   |   |-- light_saber.cpp
	   |   `-- light_saber.hpp
	   |-- thing.cpp
	   `-- thing.hpp

Then you can generate these sources with something like

	alright_sources = prepend_dir('alright', ['light_bulb.cpp', 'sandwich.cpp'])
	cool_sources = prepend_dir('cool', ['death_star.cpp', 'light_saber.cpp'])
	program_sources = ['thing.cpp'] + alright_sources + cool_sources
	program_libraries = ['boost_filesystem']
	
	program = ('program', program_sources, program_libraries)
	
	base_sources = [program]

And as long as you don't need to tweak any compiler settings, you do not need to make any more modifications.

# Defaults / Assumptions

SCons Template assumes that you are compiling all sources with the same options. It does not have a built-in way to specify certain compiler flags for certain languages (although it could still possibly be used as the basis for something like that).

It also assumes that you want to build two versions of each program: debug and release.

The default compiler flags assumed by SCons Template is a very high warning level.

# Copyright David Stone 2015.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

class optimize:
	link_flags_debug = [
	]
	link_flags_release = [
		'-Ofast',
		'-march=native',
		'-fipa-pta',
		'-fnothrow-opt',
		'-funsafe-loop-optimizations',
		'-flto=4',
	]

	compile_flags_debug = [
		'-Og',
		'-march=native',
	]

	compile_flags_release = link_flags_release + [
	]

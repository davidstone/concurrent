# Copyright David Stone 2015.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# -Wno-c++98-compat is used because I do not care about being compatible with
# old versions of the standard. I use -Wno-c++98-compat-pedantic because I still
# do not care.
#
# -Wno-exit-time-destructors warns about any static variable that has a
# destructor. I use a few static const variables, but they do not depend on each
# other for their destruction (or any global variable), so my usage is safe.
#
# -Wfloat-equal warns for safe equality comparisons (in particular, comparison
# with a non-computed value of -1). An example in my code where I use this is
# that I have a vector of float. I go through this vector, and there are some
# elements I cannot evaluate yet what they should be, so I set them to -1.0f
# (since my problem only uses positive numbers, -1 is out of the domain). I
# later go through and update -1.0f values. It does not easily lend itself to a
# different method of operation.
#
# -Wmissing-braces is incompatible with the implementation of make_array. An
# implementation that doesn't run afoul of this warning ended up being very slow
# and memory intensive, so it seems that disabling the warning is the better
# option.
#
# -Wpadded is turned on occasionally to optimize the layout of classes, but it
# is not left on because not all classes have enough elements to remove padding
# at the end. In theory I could get some extra variables for 'free', but it's
# not worth the extra effort of maintaining that (if my class size changes,
# it's not easy to remove those previously free variables).
#
# -Wrange-loop-analysis is incompatible with const-correctness and iterators
# that return by value. Will have to improve the implementation of this warning.
#
# -Wswitch-enum isn't behavior that I want. I don't want to handle every switch
# statement explicitly. It would be useful if the language had some mechanism
# to activate this on specified switch statements (to ensure that future
# changes to the enum are handled everywhere that they need to be), but it's
# overkill for an "all-or-nothing" setting.
#
# -Wweak-vtables requires writing more code for no benefit other than slightly
# reduced compile times. They are not such a large issue for my project that it
# is worth more code.

warnings = [
	'-Weverything',
	'-Werror',
	'-Wno-c++98-compat',
	'-Wno-c++98-compat-pedantic',
	'-Wno-exit-time-destructors',
	'-Wno-float-equal',
	'-Wno-missing-braces',
	'-Wno-padded',
	'-Wno-range-loop-analysis',
	'-Wno-switch-enum',
	'-Wno-weak-vtables',
]

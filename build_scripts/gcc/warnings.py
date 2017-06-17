# Copyright David Stone 2015.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

# -Wabi is not needed because I'm not combining binaries from different
# compilers.
#
# -Waggregate-return is not something that I consider an error. For instance,
# it triggers when using a range-based for loop on a vector of classes. Return
# value optimization should take care of any negative effects of this.
#
# -Weffc++ includes a warning if all data members are not initialized in the
# initializer list. I intentionally do not do this in many cases, so the set of
# warnings is too cluttered to be useful. It's helpful to turn on every once in
# a while and scan for other warnings, though (such as non-virtual destructors
# of base classes). This would be more useful as a collection of warnings (like
# -Wall) instead of a single warning on its own.
#
# -Wfloat-equal warns for safe equality comparisons (in particular, comparison
# with a non-computed value of -1). An example in my code where I use this is
# that I have a vector of float. I go through this vector, and there are some
# elements I cannot evaluate yet what they should be, so I set them to -1.0f
# (since my problem only uses positive numbers, -1 is out of the domain). I
# later go through and update -1.0f values. It does not easily lend itself to a
# different method of operation.
#
# -Winline is absent because I don't use the inline keyword for optimization
# purposes, just to define functions inline in headers. I don't care if the
# optimizer actually inlines it. This warning also complains if it can't inline
# a function declared in a class body (such as an empty virtual destructor).
#
# -Wlogical-op warns for expressions that happen to be equal in a template
# instantiation
#
# -Wmissing-format-attribute is not used because I do not use gnu extensions.
# Same for -Wsuggest-attribute and several others.
#
# -Wnormalized=nfc is already the default option, and looks to be the best.
#
# -Wpadded is turned on occasionally to optimize the layout of classes, but it
# is not left on because not all classes have enough elements to remove padding
# at the end. In theory I could get some extra variables for 'free', but it's
# not worth the extra effort of maintaining that (if my class size changes,
# it's not easy to remove those previously free variables).
#
# -Wsign-promo triggers on code that is guaranteed safe due to the use of the
# bounded::integer library. Working around the warning would lead to either less
# efficient code or more obfuscated code.
#
# -Wstack-protector is not used because I do not use -fstack-protector.
#
# -Wstrict-aliasing=3 is turned on by -Wall and is the most accurate, but it
# looks like level 1 and 2 give more warnings. In theory a lower level is a
# 'stronger' warning, but it's at the cost of more false positives.
#
# -Wsuggest-final-methods and -Wsuggest-final-types is a linker warning, so it
# is not possible to disable it for boost and other third-party libraries by
# saying they are system headers.
#
# -Wswitch-enum isn't behavior that I want. I don't want to handle every switch
# statement explicitly. It would be useful if the language had some mechanism
# to activate this on specified switch statements (to ensure that future
# changes to the enum are handled everywhere that they need to be), but it's
# overkill for an "all-or-nothing" setting.
#
# -Wunsafe-loop-optimizations causes too many spurious warnings. It may be
# useful to apply this one periodically and manually verify the results. As an
# example, it generated this warning in my code when I looped over all elements
# in a vector to apply a set of functions to them (using the range-based for
# loop).  It is also warning for the constructor of a const array of const
# std::string (where there is no loop in user code).
#
# -Wuseless-cast is incompatible with BOUNDED_INTEGER_CONDITIONAL

warnings = [
	'-Wall',
	'-Wextra',
	'-Wpedantic',
#	'-Wabi',
	'-Wcast-align',
	'-Wcast-qual',
	'-Wconversion',
	'-Wctor-dtor-privacy',
	'-Wdisabled-optimization',
	'-Wdouble-promotion',
#	'-Weffc++',
#	'-Wfloat-equal',
	'-Wformat=2',
	'-Winit-self',
	'-Winvalid-pch',
#	'-Wlogical-op',
	'-Wmissing-declarations',
	'-Wmissing-include-dirs',
	'-Wnoexcept',
	'-Wodr',
	'-Wold-style-cast',
	'-Woverloaded-virtual',
#	'-Wpadded',
	'-Wredundant-decls',
	'-Wshadow',
	'-Wsign-conversion',
#	'-Wsign-promo',
#	'-Wsuggest-final-methods',
#	'-Wsuggest-final-types',
	'-Wstrict-null-sentinel',
#	'-Wstrict-overflow=5',
	'-Wswitch-default',
#	'-Wswitch-enum',
	'-Wtrampolines',
	'-Wundef',
#	'-Wunsafe-loop-optimizations',
#	'-Wuseless-cast',
	'-Wvector-operation-performance',
#	'-Wzero-as-null-pointer-constant',
	'-Werror',
]

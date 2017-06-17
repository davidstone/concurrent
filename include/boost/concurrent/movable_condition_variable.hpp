// Copyright IHS Markit Ltd 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <boost/thread/condition_variable.hpp>

// This is identical to boost::condition_variable, except that it is move
// constructible.
//
// See movable_mutex.hpp

namespace boost {
namespace concurrent {

// TODO: Use this implementation?
struct movable_condition_variable : boost::condition_variable {
	movable_condition_variable() = default;
	using boost::condition_variable::condition_variable;
	movable_condition_variable(movable_condition_variable &&) noexcept {}
};

}	// namespace concurrent
}	// namespace boost

// Copyright IHS Markit Ltd 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <boost/thread/mutex.hpp>

// This is identical to boost::mutex, except that it is move constructible.

namespace boost {
namespace concurrent {

// We publicly inherit from boost::mutex to make it easy to work with
// boost::condition_variable. If your lock type is not
// boost::unique_lock<boost::mutex>, you have to use
// boost::condition_variable_any, which is less efficient. The conversion
// operator means you can use boost::unique_lock<boost::mutex>(mutex), where
// mutex is of type concurrent::movable_mutex.
struct movable_mutex : boost::mutex {
	movable_mutex() = default;
	using boost::mutex::mutex;
	// This does not lock the mutex or try to ensure thread safety. A move
	// constructor can only be called in one of two situations:
	//
	// 1) The object is a temporary. In this case, it is obvious that no one
	// else can be accessing it concurrently.
	//
	// 2) Someone explicitly called std::move. In this case, the user is saying
	// that is it safe to treat this class as if it were temporary.
	//
	// This allows the use of a concurrent types inside of a type that is
	// returned by a factory function.
	//
	// Conceptually, there is no state in a mutex that we need to move from the
	// source into the target.
	movable_mutex(movable_mutex &&) noexcept {}
};

}	// namespace concurrent
}	// namespace boost

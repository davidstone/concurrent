// Copyright IHS Markit Ltd 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// This contains a thread-safe queue that can be used in a multi-producer,
// multi-consumer context. It is optimized for a single consumer, as the entire
// queue is drained whenever you request more data.

module;

#include <operators/forward.hpp>

export module concurrent.queue;

import concurrent.queue_stack_impl;

import containers;
import tv;
import std_module;

namespace concurrent {

// basic_unbounded_queue is limited only by the available memory on the system
export template<typename Container, typename Mutex = std::mutex>
struct basic_unbounded_queue : private queue_stack_impl<Container, Mutex, basic_unbounded_queue<Container, Mutex>> {
private:
	using base = queue_stack_impl<Container, Mutex, basic_unbounded_queue<Container, Mutex>>;
public:
	using typename base::container_type;
	using typename base::value_type;

	basic_unbounded_queue() = default;

	using base::append;
	using base::non_blocking_append;
	using base::emplace;
	using base::non_blocking_emplace;
	using base::push;
	using base::non_blocking_push;

	using base::pop_all;
	using base::try_pop_all;
	using base::pop_one;
	using base::try_pop_one;

	using base::clear;

	using base::reserve;
	using base::size;
	
private:
	friend base;

	auto handle_add(Container &, std::unique_lock<Mutex> &) -> void {
	}
	auto handle_add(Container &, std::stop_token const &, std::unique_lock<Mutex> &) -> bool {
		return true;
	}
	auto handle_non_blocking_add(Container &, std::unique_lock<Mutex> &) -> bool {
		return true;
	}
	auto handle_remove_all(containers::range_size_t<Container>) -> void {
	}
	auto handle_remove_one(containers::range_size_t<Container>) -> void {
	}
};

export template<typename T, typename Mutex = std::mutex>
using unbounded_queue = basic_unbounded_queue<std::vector<T>, std::mutex>;



// blocking_queue has a max_size. If the queue contains at least max_size()
// elements when attempting to add data, the call will block until the size is
// less than max_size().
export template<typename Container, typename Mutex = std::mutex>
struct basic_blocking_queue : private queue_stack_impl<Container, Mutex, basic_blocking_queue<Container, Mutex>> {
private:
	using base = queue_stack_impl<Container, Mutex, basic_blocking_queue<Container, Mutex>>;
public:
	using typename base::container_type;
	using typename base::value_type;

	explicit basic_blocking_queue(containers::range_size_t<Container> const max_size_):
		m_max_size(max_size_)
	{
	}

	auto max_size() const {
		return m_max_size;
	}

	using base::append;
	using base::non_blocking_append;
	using base::emplace;
	using base::stoppable_emplace;
	using base::non_blocking_emplace;
	using base::push;
	using base::non_blocking_push;

	using base::pop_all;
	using base::try_pop_all;
	using base::pop_one;
	using base::try_pop_one;

	using base::clear;

	using base::reserve;
	using base::size;

private:
	friend base;

	auto handle_add(Container & queue, std::unique_lock<Mutex> & lock) -> void {
		m_notify_removal.wait(
			lock,
			[&]{ return containers::size(queue) < m_max_size; }
		);
	}
	auto handle_add(Container & queue, std::stop_token token, std::unique_lock<Mutex> & lock) -> bool {
		return m_notify_removal.wait(
			lock,
			std::move(token),
			[&]{ return containers::size(queue) < m_max_size; }
		);
	}
	auto handle_non_blocking_add(Container & queue, std::unique_lock<Mutex> &) -> bool {
		return containers::size(queue) < m_max_size;
	}
	auto handle_remove_all(containers::range_size_t<Container> const previous_size) -> void {
		if (previous_size >= max_size()) {
			m_notify_removal.notify_all();
		}
	}
	auto handle_remove_one(containers::range_size_t<Container> const previous_size) -> void {
		if (previous_size >= max_size()) {
			m_notify_removal.notify_one();
		}
	}

	containers::range_size_t<Container> m_max_size;
	std::condition_variable_any m_notify_removal;
};

export template<typename T, typename Mutex = std::mutex>
using blocking_queue = basic_blocking_queue<std::vector<T>, Mutex>;

} // namespace concurrent

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

import containers;
import tv;
import std_module;

namespace concurrent {

template<typename Container>
concept pop_frontable = requires(Container & container) { containers::pop_front(container); };

template<typename Container, typename Mutex, typename Derived>
struct basic_queue_impl {
	using container_type = Container;
	using value_type = containers::range_value_t<Container>;

	basic_queue_impl() = default;

	// If you know you will be adding multiple elements into the queue, prefer
	// to use the range-based append member function, as it will take a single
	// lock for the entire insert, rather than acquiring a lock per element.
	// This will also optimize memory usage, as the underlying container can
	// reserve all the space it needs.
	auto append(containers::range auto && input) -> void {
		generic_add(
			[&]{ containers::append(m_container, OPERATORS_FORWARD(input)); }
		);
	}

	auto non_blocking_append(containers::range auto && input) -> bool {
		return generic_non_blocking_add(
			[&]{ containers::append(m_container, OPERATORS_FORWARD(input)); }
		);
	}

	auto emplace(auto && ... args) -> void {
		generic_add(
			[&]{ containers::emplace_back(m_container, OPERATORS_FORWARD(args)...); }
		);
	}
	auto push(value_type && value) -> void {
		emplace(std::move(value));
	}
	auto push(value_type const & value) -> void {
		emplace(value);
	}

	auto non_blocking_emplace(auto && ... args) -> bool {
		return generic_non_blocking_add(
			[&]{ containers::emplace_back(m_container, OPERATORS_FORWARD(args)...); }
		);
	}
	auto non_blocking_push(value_type && value) -> bool {
		return non_blocking_emplace(std::move(value));
	}
	auto non_blocking_push(value_type const & value) -> bool {
		return non_blocking_emplace(value);
	}


	// Returns all messages in the queue. This strategy minimizes contention by
	// giving each worker thread the largest chunk of work possible. The
	// tradeoff for this approach is that the queue does not attempt to be fair,
	// so it is possible in a multi-consumer case to have one thread with a lot
	// of work, and all other threads with no work.
	//
	// For many real-world workloads, however, this leads to better throughput
	// than solutions which rely on returning to the queue for each element.
	//
	// All pop_all overloads accept a parameter of the same type as the
	// container. This argument is used to reuse capacity. This is especially
	// helpful for the common case where the container is a std::vector.

	// This overload never returns an empty container. If the queue is empty,
	// this will block
	auto pop_all(Container storage = Container{}) -> Container {
		return generic_pop_all(wait_for_data(), std::move(storage));
	}
	// This overload can return an empty container if a stop was requested. If
	// the queue is empty, this will block.
	auto pop_all(std::stop_token token, Container storage = Container{}) -> Container {
		return generic_pop_all(wait_for_data(std::move(token)), std::move(storage));
	}

	// The versions with a timeout will block until there is data available,
	// unless the timeout is reached, in which case they return an empty
	// container.
	template<typename Clock, typename Duration>
	auto pop_all(std::chrono::time_point<Clock, Duration> const timeout, Container storage = Container{}) -> Container {
		return generic_pop_all(wait_for_data(timeout), std::move(storage));
	}
	template<typename Clock, typename Duration>
	auto pop_all(std::stop_token token, std::chrono::time_point<Clock, Duration> const timeout, Container storage = Container{}) -> Container {
		return generic_pop_all(wait_for_data(std::move(token), timeout), std::move(storage));
	}
	template<typename Rep, typename Period>
	auto pop_all(std::chrono::duration<Rep, Period> const timeout, Container storage = Container{}) -> Container {
		return generic_pop_all(wait_for_data(timeout), std::move(storage));
	}
	template<typename Rep, typename Period>
	auto pop_all(std::stop_token token, std::chrono::duration<Rep, Period> const timeout, Container storage = Container{}) -> Container {
		return generic_pop_all(wait_for_data(std::move(token), timeout), std::move(storage));
	}

	// Does not wait for data (can return an empty container)
	auto try_pop_all(Container storage = Container{}) -> Container {
		return generic_pop_all(lock_type(m_mutex), std::move(storage));
	}



	// If the queue is empty, this will block.
	auto pop_one() -> value_type {
		return generic_pop_one(wait_for_data());
	}
	auto pop_one(std::stop_token token) -> tv::optional<value_type> {
		auto lock = wait_for_data(std::move(token));
		if (containers::is_empty(m_container)) {
			return tv::none;
		}
		return generic_pop_one(std::move(lock));
	}

	// The versions with a timeout will return as soon as there is data
	// available, unless the timeout is reached, in which case they return
	// tv::none
	template<typename Clock, typename Duration>
	auto pop_one(std::chrono::time_point<Clock, Duration> const timeout) -> tv::optional<value_type> {
		auto lock = wait_for_data(timeout);
		if (containers::is_empty(m_container)) {
			return tv::none;
		}
		return generic_pop_one(std::move(lock));
	}
	template<typename Clock, typename Duration>
	auto pop_one(std::stop_token token, std::chrono::time_point<Clock, Duration> const timeout) -> tv::optional<value_type> {
		auto lock = wait_for_data(std::move(token), timeout);
		if (containers::is_empty(m_container)) {
			return tv::none;
		}
		return generic_pop_one(std::move(lock));
	}
	template<typename Rep, typename Period>
	auto pop_one(std::chrono::duration<Rep, Period> const timeout) -> tv::optional<value_type> {
		auto lock = wait_for_data(timeout);
		if (containers::is_empty(m_container)) {
			return tv::none;
		}
		return generic_pop_one(std::move(lock));
	}
	template<typename Rep, typename Period>
	auto pop_one(std::stop_token token, std::chrono::duration<Rep, Period> const timeout) -> tv::optional<value_type> {
		auto lock = wait_for_data(std::move(token), timeout);
		if (containers::is_empty(m_container)) {
			return tv::none;
		}
		return generic_pop_one(std::move(lock));
	}

	auto try_pop_one() -> tv::optional<value_type> {
		auto lock = lock_type(m_mutex);
		if (containers::is_empty(m_container)) {
			return tv::none;
		}
		return generic_pop_one(std::move(lock));
	}


	auto clear() -> void {
		auto const lock = lock_type(m_mutex);
		auto const previous_size = containers::size(m_container);
		containers::clear(m_container);
		derived().handle_remove_all(previous_size);
	}


	auto reserve(auto const new_capacity) -> void {
		auto lock = lock_type(m_mutex);
		m_container.reserve(new_capacity);
	}
	auto size() const {
		auto lock = lock_type(m_mutex);
		return containers::size(m_container);
	}

private:
	using lock_type = std::unique_lock<Mutex>;

	auto is_not_empty() const {
		return [&]{ return !containers::is_empty(m_container); };
	}

	auto wait_for_data(std::stop_token token) -> lock_type {
		auto lock = lock_type(m_mutex);
		m_notify_addition.wait(lock, std::move(token), is_not_empty());
		return lock;
	}
	auto wait_for_data() -> lock_type {
		auto lock = lock_type(m_mutex);
		m_notify_addition.wait(lock, is_not_empty());
		return lock;
	}
	template<typename Clock, typename Duration>
	auto wait_for_data(std::chrono::time_point<Clock, Duration> const timeout) -> lock_type {
		auto lock = lock_type(m_mutex);
		m_notify_addition.wait_until(lock, timeout, is_not_empty());
		return lock;
	}
	template<typename Clock, typename Duration>
	auto wait_for_data(std::stop_token token, std::chrono::time_point<Clock, Duration> const timeout) -> lock_type {
		auto lock = lock_type(m_mutex);
		m_notify_addition.wait_until(lock, std::move(token), timeout, is_not_empty());
		return lock;
	}
	template<typename Rep, typename Period>
	auto wait_for_data(std::chrono::duration<Rep, Period> const timeout) -> lock_type {
		auto lock = lock_type(m_mutex);
		m_notify_addition.wait_for(lock, timeout, is_not_empty());
		return lock;
	}
	template<typename Rep, typename Period>
	auto wait_for_data(std::stop_token token, std::chrono::duration<Rep, Period> const timeout) -> lock_type {
		auto lock = lock_type(m_mutex);
		m_notify_addition.wait_for(lock, std::move(token), timeout, is_not_empty());
		return lock;
	}


	auto derived() -> Derived & {
		return static_cast<Derived &>(*this);
	}


	auto generic_add(auto && add) -> void {
		generic_add_impl(lock_type(m_mutex), add);
	}

	auto generic_non_blocking_add(auto && add) -> bool {
		auto lock = lock_type(m_mutex, std::try_to_lock);
		if (!lock.owns_lock()) {
			return false;
		}
		generic_add_impl(std::move(lock), add);
		return true;
	}

	auto generic_add_impl(lock_type lock, auto && add) -> void {
		derived().handle_add(m_container, lock);
		auto const was_empty = containers::is_empty(m_container);
		add();
		lock.unlock();
		// It is safe to notify outside of the lock here.
		//
		// With some code, it is dangerous to notify outside of the lock. The
		// orderings that create the deadlock cannot occur in this code.
		//
		// These are the four main units of code:
		//
		// 1) Check in generic_pop_all if the container is empty
		// 2) Wait on the condition_variable if the container was empty in 1
		// A) Add a value in generic_add
		// B) Signal if the container was empty before A
		//
		// 1 is obviously ordered before 2 in any execution, and A is ordered
		// before B.
		//
		// For it to be dangerous here, the following ordering must be possible:
		// 1, A, B, 2. This is not a possible ordering because A cannot happen
		// between 1 and 2. The state transition from 1 to 2 is atomic, and A
		// must respect that because A holds a lock.
		//
		// The possible orderings are
		//
		// 1, 2, A, B: We receive the signal properly because the wait is
		// ordered before the signal.
		//
		// A, 1, 2, B: We do not wait because the container is no longer empty
		// after A. B sends a signal that no one receives, but that is OK
		// because no one needs to receive it.
		//
		// A, 1, B, 2: Same explanation as A, 1, 2, B.
		//
		// A, B, 1, 2: Same as A, 1, 2, B. 2 never waits because 1 does not
		// find an empty container.
		if (was_empty) {
			if constexpr (pop_frontable<Container>) {
				m_notify_addition.notify_all();
			} else {
				m_notify_addition.notify_one();
			}
		}
	}


	// lock must be in the locked state
	auto generic_pop_all(lock_type lock, Container storage) -> Container {
		using std::swap;
		swap(m_container, storage);
		derived().handle_remove_all(containers::size(storage));
		lock.unlock();
		return storage;
	}

	// lock must be in the locked state
	auto generic_pop_one(lock_type lock) -> value_type {
		auto const previous_size = containers::size(m_container);
		auto result = std::move(containers::front(m_container));
		containers::pop_front(m_container);
		derived().handle_remove_one(previous_size);
		lock.unlock();
		return result;
	}

	Container m_container;
	mutable Mutex m_mutex;
	std::condition_variable_any m_notify_addition;
};

// basic_unbounded_queue is limited only by the available memory on the system
export template<typename Container, typename Mutex = std::mutex>
struct basic_unbounded_queue : private basic_queue_impl<Container, Mutex, basic_unbounded_queue<Container, Mutex>> {
private:
	using base = basic_queue_impl<Container, Mutex, basic_unbounded_queue<Container, Mutex>>;
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
struct basic_blocking_queue : private basic_queue_impl<Container, Mutex, basic_blocking_queue<Container, Mutex>> {
private:
	using base = basic_queue_impl<Container, Mutex, basic_blocking_queue<Container, Mutex>>;
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
	auto handle_add(Container & queue, std::stop_token token, std::unique_lock<Mutex> & lock) -> void {
		m_notify_removal.wait(
			lock,
			std::move(token),
			[&]{ return containers::size(queue) < m_max_size; }
		);
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

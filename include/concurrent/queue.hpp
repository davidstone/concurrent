// Copyright IHS Markit Ltd 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// This contains a thread-safe queue that can be used in a multi-producer,
// multi-consumer context. It is optimized for a single consumer, as the entire
// queue is drained whenever you request more data.

#pragma once

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/lock_types.hpp>

#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace concurrent {
namespace detail {

template<typename Container, typename = void>
constexpr bool supports_pop_front = false;

template<typename Container>
constexpr bool supports_pop_front<Container, std::void_t<decltype(std::declval<Container &>().pop_front())>> = true;

inline constexpr auto size = [](auto && range) {
	using std::size;
	return size(range);
};

inline constexpr auto empty = [](auto && range) {
	using std::empty;
	return empty(range);
};

template<typename Mutex>
using condition_variable = std::conditional_t<std::is_same_v<Mutex, boost::mutex>, boost::condition_variable, boost::condition_variable_any>;

template<typename Container, typename Mutex, typename Derived>
struct basic_queue_impl {
	using container_type = Container;
	using value_type = typename Container::value_type;

	basic_queue_impl() = default;

	// If you know you will be adding multiple elements into the queue, prefer
	// to use the range-based append member function, as it will take a single
	// lock for the entire insert, rather than acquiring a lock per element.
	// This will also optimize memory usage, as the underlying container can
	// reserve all the space it needs.
	template<typename InputIterator, typename Sentinel>
	auto append(InputIterator const first, Sentinel const last) -> void {
		constexpr auto adding_several = std::true_type{};
		generic_add(adding_several, [&]{ m_container.insert(m_container.end(), first, last); });
	}

	template<typename InputIterator, typename Sentinel>
	bool non_blocking_append(InputIterator const first, Sentinel const last) {
		constexpr auto adding_several = std::true_type{};
		return generic_non_blocking_add(adding_several, [&]{ m_container.insert(m_container.end(), first, last); });
	}

	template<typename... Args>
	auto emplace(Args && ... args) -> void {
		constexpr auto adding_several = std::false_type{};
		generic_add(adding_several, [&]{ m_container.emplace_back(std::forward<Args>(args)...); });
	}
	auto push(value_type && value) -> void {
		emplace(std::move(value));
	}
	auto push(value_type const & value) -> void {
		emplace(value);
	}

	template<typename... Args>
	bool non_blocking_emplace(Args && ... args) {
		constexpr auto adding_several = std::false_type{};
		return generic_non_blocking_add(adding_several, [&]{ m_container.emplace_back(std::forward<Args>(args)...); });
	}
	bool non_blocking_push(value_type && value) {
		return non_blocking_emplace(std::move(value));
	}
	bool non_blocking_push(value_type const & value) {
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
	//
	// This overload never returns an empty container. If the queue is empty,
	// this will block
	Container pop_all(Container storage = Container{}) {
		return generic_pop_all(wait_for_data(), std::move(storage));
	}

	// The two versions with a timeout will block until there is data available,
	// unless the timeout is reached, in which case they return an empty
	// container.
	template<typename Clock, typename Duration>
	Container pop_all(boost::chrono::time_point<Clock, Duration> const timeout, Container storage = Container{}) {
		return generic_pop_all(wait_for_data(timeout), std::move(storage));
	}
	template<typename Rep, typename Period>
	Container pop_all(boost::chrono::duration<Rep, Period> const timeout, Container storage = Container{}) {
		return generic_pop_all(wait_for_data(timeout), std::move(storage));
	}

	// Does not wait for data (can return an empty container)
	container_type try_pop_all(container_type storage = container_type{}) {
		return generic_pop_all(lock_type(m_mutex), std::move(storage));
	}



	// If the queue is empty, this will block.
	value_type pop_one() {
		return generic_pop_one(wait_for_data());
	}

	// The two versions with a timeout will return as soon as there is data
	// available, unless the timeout is reached, in which case they return
	// boost::none
	template<typename Clock, typename Duration>
	std::optional<value_type> pop_one(boost::chrono::time_point<Clock, Duration> const timeout) {
		auto lock = wait_for_data(timeout);
		if (empty(m_container)) {
			return std::nullopt;
		}
		return generic_pop_one(std::move(lock));
	}
	template<typename Rep, typename Period>
	std::optional<value_type> pop_one(boost::chrono::duration<Rep, Period> const timeout) {
		auto lock = wait_for_data(timeout);
		if (empty(m_container)) {
			return std::nullopt;
		}
		return generic_pop_one(std::move(lock));
	}

	std::optional<value_type> try_pop_one() {
		auto lock = lock_type(m_mutex);
		if (empty(m_container)) {
			return std::nullopt;
		}
		return generic_pop_one(std::move(lock));
	}


	void clear() {
		auto const lock = lock_type(m_mutex);
		auto const previous_size = detail::size(m_container);
		m_container.clear();
		derived().handle_remove_all(previous_size);
	}


	template<typename Capacity>
	void reserve(Capacity const new_capacity) {
		auto lock = lock_type(m_mutex);
		m_container.reserve(new_capacity);
	}
	auto size() const {
		auto lock = lock_type(m_mutex);
		return detail::size(m_container);
	}

private:
	using lock_type = boost::unique_lock<Mutex>;

	auto is_not_empty() const {
		return [&]{ return !empty(m_container); };
	}

	auto wait_for_data() {
		auto lock = lock_type(m_mutex);
		m_notify_addition.wait(lock, is_not_empty());
		return lock;
	}
	template<typename Clock, typename Duration>
	auto wait_for_data(boost::chrono::time_point<Clock, Duration> const timeout) {
		auto lock = lock_type(m_mutex);
		m_notify_addition.wait_until(lock, timeout, is_not_empty());
		return lock;
	}
	template<typename Rep, typename Period>
	auto wait_for_data(boost::chrono::duration<Rep, Period> const timeout) {
		auto lock = lock_type(m_mutex);
		m_notify_addition.wait_for(lock, timeout, is_not_empty());
		return lock;
	}


	Derived & derived() {
		return static_cast<Derived &>(*this);
	}


	template<typename Bool, typename Function>
	auto generic_add(Bool const adding_several, Function && add) -> void {
		generic_add_impl(adding_several, lock_type(m_mutex), add);
	}

	template<typename Bool, typename Function>
	bool generic_non_blocking_add(Bool const adding_several, Function && add) {
		auto lock = lock_type(m_mutex, boost::try_to_lock);
		if (!lock.owns_lock()) {
			return false;
		}
		generic_add_impl(adding_several, std::move(lock), add);
		return true;
	}

	template<typename Bool, typename Function>
	auto generic_add_impl(Bool const adding_several, lock_type lock, Function && add) -> void {
		derived().handle_add(m_container, lock);
		auto const was_empty = empty(m_container);
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
		// A, B, 1, 2: Same as A, 1, 2, B. 2 never waits because 1 does not
		// find an empty container.
		if (was_empty) {
			if constexpr (detail::supports_pop_front<Container> && adding_several) {
				m_notify_addition.notify_all();
			} else {
				m_notify_addition.notify_one();
			}
		}
	}


	// lock must be in the locked state
	container_type generic_pop_all(lock_type lock, container_type storage) {
		using std::swap;
		swap(m_container, storage);
		derived().handle_remove_all(detail::size(storage));
		lock.unlock();
		return storage;
	}

	// lock must be in the locked state
	value_type generic_pop_one(lock_type lock) {
		auto const previous_size = detail::size(m_container);
		auto result = std::move(m_container.front());
		m_container.pop_front();
		derived().handle_remove_one(previous_size);
		lock.unlock();
		return result;
	}

	container_type m_container;
	mutable Mutex m_mutex;
	detail::condition_variable<Mutex> m_notify_addition;
};

}	// namespace detail

// basic_unbounded_queue is limited only by the available memory on the system
template<typename Container, typename Mutex = boost::mutex>
struct basic_unbounded_queue : private detail::basic_queue_impl<Container, Mutex, basic_unbounded_queue<Container, Mutex>> {
private:
	using base = detail::basic_queue_impl<Container, Mutex, basic_unbounded_queue<Container, Mutex>>;
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

	void handle_add(Container &, boost::unique_lock<Mutex> &) {
	}
	void handle_remove_all(typename Container::size_type) {
	}
	void handle_remove_one(typename Container::size_type) {
	}
};

template<typename T, typename Mutex = boost::mutex>
using unbounded_queue = basic_unbounded_queue<std::vector<T>, boost::mutex>;



// blocking_queue has a max_size. If the queue contains at least max_size()
// elements when attempting to add data, the call will block until the size is
// less than max_size().
template<typename Container, typename Mutex = boost::mutex>
struct basic_blocking_queue : private detail::basic_queue_impl<Container, Mutex, basic_blocking_queue<Container, Mutex>> {
private:
	using base = detail::basic_queue_impl<Container, Mutex, basic_blocking_queue<Container, Mutex>>;
public:
	using typename base::container_type;
	using typename base::value_type;

	explicit basic_blocking_queue(typename Container::size_type const max_size_):
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

	void handle_add(Container & queue, boost::unique_lock<Mutex> & lock) {
		m_notify_removal.wait(lock, [&]{ return detail::size(queue) < m_max_size; });
	}
	void handle_remove_all(typename Container::size_type const previous_size) {
		if (previous_size >= max_size()) {
			m_notify_removal.notify_all();
		}
	}
	void handle_remove_one(typename Container::size_type const previous_size) {
		if (previous_size >= max_size()) {
			m_notify_removal.notify_one();
		}
	}

	typename Container::size_type m_max_size;
	detail::condition_variable<Mutex> m_notify_removal;
};

template<typename T, typename Mutex = boost::mutex>
using blocking_queue = basic_blocking_queue<std::vector<T>, Mutex>;

} // namespace concurrent

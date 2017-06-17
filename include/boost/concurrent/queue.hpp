// Copyright IHS Markit Ltd 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// This contains a thread-safe queue that can be used in a multi-producer,
// multi-consumer context. It is optimized for a single consumer, as the entire
// queue is drained whenever you request more data.

#pragma once

#include <boost/concurrent/movable_condition_variable.hpp>
#include <boost/concurrent/movable_mutex.hpp>

#include <boost/optional.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/lock_types.hpp>

#include <type_traits>
#include <utility>
#include <vector>

namespace boost {
namespace concurrent {
namespace detail {

template<typename...>
using void_t = void;

template<typename Container, typename = void>
struct supports_pop_front : std::false_type {};

template<typename Container>
struct supports_pop_front<Container, void_t<decltype(std::declval<Container &>().pop_front())>> : std::true_type {};


template<typename Container, typename Derived>
struct basic_queue_impl {
	using container_type = Container;
	using value_type = typename container_type::value_type;

	basic_queue_impl() = default;

	// Not thread safe: See movable_mutex.hpp
	basic_queue_impl(basic_queue_impl &&) = default;

	basic_queue_impl(basic_queue_impl const &) = delete;
	basic_queue_impl & operator=(basic_queue_impl const &) = delete;
	basic_queue_impl & operator=(basic_queue_impl &&) = delete;


	// If you know you will be adding multiple elements into the queue, prefer
	// to use the range-based append member function, as it will take a single
	// lock for the entire insert, rather than acquiring a lock per element.
	// This will also optimize memory usage, as the underlying container can
	// reserve all the space it needs.
	template<typename InputIterator, typename Sentinel>
	decltype(auto) append(InputIterator const first, Sentinel const last) {
		constexpr auto adding_several = std::true_type{};
		return generic_add(adding_several, [&]{ m_container.insert(m_container.end(), first, last); });
	}

	template<typename... Args>
	decltype(auto) emplace(Args && ... args) {
		constexpr auto adding_several = std::false_type{};
		return generic_add(adding_several, [&]{ m_container.emplace_back(std::forward<Args>(args)...); });
	}
	decltype(auto) push(value_type && value) {
		return emplace(std::move(value));
	}
	decltype(auto) push(value_type const & value) {
		return emplace(value);
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
	container_type pop_all(container_type storage = container_type{}) {
		return generic_pop_all(wait_for_data(), std::move(storage));
	}

	// The two versions with a timeout will block until there is data available,
	// unless the timeout is reached, in which case they return an empty
	// container.
	template<typename Clock, typename Duration>
	container_type pop_all(boost::chrono::time_point<Clock, Duration> const timeout, container_type storage = container_type{}) {
		return generic_pop_all(wait_for_data(timeout), std::move(storage));
	}
	template<typename Rep, typename Period>
	container_type pop_all(boost::chrono::duration<Rep, Period> const timeout, container_type storage = container_type{}) {
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
	boost::optional<value_type> pop_one(boost::chrono::time_point<Clock, Duration> const timeout) {
		auto lock = wait_for_data(timeout);
		if (m_container.empty()) {
			return boost::none;
		}
		return generic_pop_one(std::move(lock));
	}
	template<typename Rep, typename Period>
	boost::optional<value_type> pop_one(boost::chrono::duration<Rep, Period> const timeout) {
		auto lock = wait_for_data(timeout);
		if (m_container.empty()) {
			return boost::none;
		}
		return generic_pop_one(std::move(lock));
	}

	boost::optional<value_type> try_pop_one() {
		auto lock = lock_type(m_mutex);
		if (m_container.empty()) {
			return boost::none;
		}
		return generic_pop_one(std::move(lock));
	}


	void clear() {
		auto const lock = lock_type(m_mutex);
		auto const previous_size = m_container.size();
		m_container.clear();
		derived().handle_remove_all(previous_size);
	}


	template<typename Size>
	void reserve(Size const size) {
		auto lock = lock_type(m_mutex);
		m_container.reserve(size);
	}
	auto size() const {
		auto lock = lock_type(m_mutex);
		return m_container.size();
	}

private:
	using lock_type = boost::unique_lock<boost::mutex>;

	auto is_not_empty() const {
		return [=]{ return !m_container.empty(); };
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



	// Until we can get something like this proposal
	// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0146r1.html
	// we require multiple levels of template indirection with empty, wrap_void,
	// and unwrap_forward

	enum class empty {
	};

	// TODO: implement this with if constexpr
	template<typename Function, typename std::enable_if<!std::is_void<decltype(std::declval<Function>()())>{}, empty>::type = empty{}>
	static decltype(auto) wrap_void(Function function) {
		return function();
	}
	template<typename Function, typename std::enable_if<std::is_void<decltype(std::declval<Function>()())>{}, empty>::type = empty{}>
	static decltype(auto) wrap_void(Function function) {
		function();
		return empty{};
	}
	
	template<typename>
	static void unwrap_forward(empty) {
	}
	template<typename T, typename Arg>
	static decltype(auto) unwrap_forward(Arg && value) {
		// std::forward always returns a reference. We want to return a value if
		// handle_add returns by value. This means we need to static cast to the
		// correct type so that the decltype(auto) return type picks it up.
		// However, if we do not forward and the function returns by value, we
		// will always copy. If there were implicit moves from local rvalue
		// references, we wouldn't need to do anything here and we could just
		// return result.
		//
		// This change to the standard is proposed in
		// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0527r0.html
		return static_cast<T>(std::forward<T>(value));
	}

	template<typename Bool, typename Function>
	decltype(auto) generic_add(Bool const adding_several, Function && add) {
		auto lock = lock_type(m_mutex);
		decltype(auto) result = wrap_void([&]() -> decltype(auto) { return derived().handle_add(m_container, lock); });
		auto const was_empty = m_container.empty();
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
			if (detail::supports_pop_front<Container>{} && adding_several) {
				m_notify_addition.notify_all();
			} else {
				m_notify_addition.notify_one();
			}
		}
		using add_result_type = decltype(std::declval<Derived &>().handle_add(
				std::declval<Container &>(),
				std::declval<lock_type &>()
		));
		return unwrap_forward<add_result_type>(result);
	}


	// lock must be in the locked state
	container_type generic_pop_all(lock_type lock, container_type storage) {
		using std::swap;
		swap(m_container, storage);
		derived().handle_remove_all(storage.size());
		lock.unlock();
		return storage;
	}

	// lock must be in the locked state
	value_type generic_pop_one(lock_type lock) {
		auto const previous_size = m_container.size();
		auto result = std::move(m_container.front());
		m_container.pop_front();
		derived().handle_remove_one(previous_size);
		lock.unlock();
		return result;
	}

	container_type m_container;
	movable_mutex m_mutex;
	movable_condition_variable m_notify_addition;
};

}	// namespace detail

// basic_unbounded_queue is limited only by the available memory on the system
template<typename Container>
struct basic_unbounded_queue : private detail::basic_queue_impl<Container, basic_unbounded_queue<Container>> {
private:
	using base = detail::basic_queue_impl<Container, basic_unbounded_queue<Container>>;
public:
	using typename base::container_type;
	using typename base::value_type;

	basic_unbounded_queue() = default;

	using base::append;
	using base::emplace;
	using base::push;

	using base::pop_all;
	using base::try_pop_all;
	using base::pop_one;
	using base::try_pop_one;

	using base::clear;

	using base::reserve;
	using base::size;
	
private:
	friend base;

	void handle_add(container_type &, boost::unique_lock<boost::mutex> &) {
	}
	void handle_remove_all(typename container_type::size_type) {
	}
	void handle_remove_one(typename container_type::size_type) {
	}
};

template<typename T, typename Allocator = std::allocator<T>>
using unbounded_queue = basic_unbounded_queue<std::vector<T, Allocator>>;



// blocking_queue has a max_size. If the queue contains at least max_size()
// elements when attempting to add data, the call will block until the size is
// less than max_size().
template<typename Container>
struct basic_blocking_queue : private detail::basic_queue_impl<Container, basic_blocking_queue<Container>> {
private:
	using base = detail::basic_queue_impl<Container, basic_blocking_queue<Container>>;
public:
	using typename base::container_type;
	using typename base::value_type;

	explicit basic_blocking_queue(typename container_type::size_type const max_size_):
		m_max_size(max_size_)
	{
	}

	auto max_size() const {
		return m_max_size;
	}

	using base::append;
	using base::emplace;
	using base::push;

	using base::pop_all;
	using base::try_pop_all;
	using base::pop_one;
	using base::try_pop_one;

	using base::clear;

	using base::reserve;
	using base::size;

private:
	friend base;

	void handle_add(container_type & queue, boost::unique_lock<boost::mutex> & lock) {
		m_notify_removal.wait(lock, [&]{ return queue.size() < m_max_size; });
	}
	void handle_remove_all(typename container_type::size_type const previous_size) {
		if (previous_size >= max_size()) {
			m_notify_removal.notify_all();
		}
	}
	void handle_remove_one(typename container_type::size_type const previous_size) {
		if (previous_size >= max_size()) {
			m_notify_removal.notify_one();
		}
	}

	typename container_type::size_type m_max_size;
	movable_condition_variable m_notify_removal;
};

template<typename T, typename Allocator = std::allocator<T>>
using blocking_queue = basic_blocking_queue<std::vector<T, Allocator>>;



// dropping_queue has a max_size. If the queue contains at least max_size()
// elements after data is added, the queue will instead be emptied.
template<typename Container>
struct basic_dropping_queue : private detail::basic_queue_impl<Container, basic_dropping_queue<Container>> {
private:
	using base = detail::basic_queue_impl<Container, basic_dropping_queue<Container>>;
public:
	using typename base::container_type;
	using typename base::value_type;

	explicit basic_dropping_queue(typename container_type::size_type const max_size_):
		m_max_size(max_size_)
	{
	}

	auto max_size() const {
		return m_max_size;
	}

	using base::append;
	using base::emplace;
	using base::push;

	using base::pop_all;
	using base::try_pop_all;
	using base::pop_one;
	using base::try_pop_one;

	using base::clear;

	using base::reserve;
	using base::size;

private:
	friend base;

	auto handle_add(container_type & queue, boost::unique_lock<boost::mutex> &) {
		if (queue.size() < max_size())
		{
			return static_cast<typename container_type::size_type>(0);
		}
		auto const skipped = queue.size();
		queue.clear();
		return skipped;
	}
	void handle_remove_all(typename container_type::size_type) {
	}
	void handle_remove_one(typename container_type::size_type) {
	}

	typename container_type::size_type m_max_size;
};

template<typename T, typename Allocator = std::allocator<T>>
using dropping_queue = basic_dropping_queue<std::vector<T, Allocator>>;

}	// namespace concurrent
}	// namespace boost

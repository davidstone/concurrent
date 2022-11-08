// Copyright IHS Markit Ltd 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <concurrent/queue.hpp>

#include <bounded/scope_guard.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <utility>

#if defined NDEBUG
	#define CONCURRENT_NDEBUG_WAS_DEFINED NDEBUG
	#undef NDEBUG
#endif

#include <cassert>

// Like assert, but always evaluated
#define CONCURRENT_TEST assert

#if defined CONCURRENT_NDEBUG_WAS_DEFINED
	#undef CONCURRENT_NDEBUG_WAS_DEFINED
#endif

namespace {

void test_int() {
	auto queue = concurrent::unbounded_queue<int>{};
	queue.emplace(0);
	queue.push(7);
	auto const first_values = queue.pop_all();
	CONCURRENT_TEST(size(first_values) == 2);
	CONCURRENT_TEST(first_values[0] == 0);
	CONCURRENT_TEST(first_values[1] == 7);
	queue.push(4);
	auto const second_values = queue.pop_all();
	CONCURRENT_TEST(size(second_values) == 1);
}

// Tests ranges and conversions
void test_string() {
	auto queue = concurrent::unbounded_queue<std::string>{};
	queue.emplace("Reese");
	queue.push("Finch");
	char const * array[] = {
		"Carter", "Fusco"
	};
	queue.append(array);
	auto const values = queue.pop_all();
	auto const expected = std::array<char const *, 4>{
		"Reese", "Finch", "Carter", "Fusco"
	};
	CONCURRENT_TEST(std::equal(values.begin(), values.end(), expected.begin(), expected.end()));
}


#ifndef _MSC_VER

struct copy_move_counter {
	copy_move_counter() noexcept {
		++s_default_constructed;
	}
	copy_move_counter(copy_move_counter const &) noexcept {
		++s_copy_constructed;
	}
	copy_move_counter(copy_move_counter &&) noexcept {
		++s_move_constructed;
	}
	// The assignment operators must exist, because they theoretically can get
	// called on some code paths, but in reality they cannot because we only
	// insert at the end of the vector.
	copy_move_counter & operator=(copy_move_counter const &) noexcept {
		++s_copy_assigned;
		return *this;
	}
	copy_move_counter & operator=(copy_move_counter &&) noexcept {
		++s_move_assigned;
		return *this;
	}
	
	static std::size_t default_constructed() noexcept {
		return s_default_constructed;
	}
	static std::size_t copy_constructed() noexcept {
		return s_copy_constructed;
	}
	static std::size_t move_constructed() noexcept {
		return s_move_constructed;
	}
	static std::size_t copy_assigned() noexcept {
		return s_copy_assigned;
	}
	static std::size_t move_assigned() noexcept {
		return s_move_assigned;
	}
private:
	static inline std::size_t s_default_constructed = 0;
	static inline std::size_t s_copy_constructed = 0;
	static inline std::size_t s_move_constructed = 0;
	static inline std::size_t s_copy_assigned = 0;
	static inline std::size_t s_move_assigned = 0;
};

void test_copy_move() {
	// Some of these tests will fail if the standard library implementation
	// makes unnecessary copies / moves.
	auto queue = concurrent::unbounded_queue<copy_move_counter>{};
	auto expected_default_constructed = static_cast<std::size_t>(0);
	auto expected_copy_constructed = static_cast<std::size_t>(0);
	auto expected_move_constructed = static_cast<std::size_t>(0);
	auto check_all = [&]{
		CONCURRENT_TEST(copy_move_counter::default_constructed() == expected_default_constructed);
		CONCURRENT_TEST(copy_move_counter::copy_constructed() == expected_copy_constructed);
		CONCURRENT_TEST(copy_move_counter::move_constructed() == expected_move_constructed);
		CONCURRENT_TEST(copy_move_counter::copy_assigned() == 0);
		CONCURRENT_TEST(copy_move_counter::move_assigned() == 0);
	};

	check_all();

	queue.emplace();
	++expected_default_constructed;
	check_all();
	
	queue.pop_all();
	check_all();
	
	auto array = std::array<copy_move_counter, 3>{};
	expected_default_constructed += size(array);
	check_all();

	queue.append(array);
	expected_copy_constructed += size(array);
	check_all();
	
	queue.pop_all();
	check_all();

	queue.append(std::move(array));
	expected_move_constructed += size(array);
	check_all();
}

#endif

auto now() {
	return std::chrono::steady_clock::now();
}
auto const duration = std::chrono::milliseconds(100);

void test_timeout() {
	auto queue = concurrent::unbounded_queue<int>{};
	auto const before_time_point = now();
	auto const values_time_point = queue.pop_all(before_time_point + duration);
	auto const after_time_point = now();
	auto const values_duration = queue.pop_all(duration);
	auto const after_duration = now();

	CONCURRENT_TEST(after_time_point - before_time_point >= duration);
	CONCURRENT_TEST(after_duration - after_time_point >= duration);

	CONCURRENT_TEST(empty(values_time_point));
	CONCURRENT_TEST(empty(values_duration));
	
	queue.push(0);
	auto const should_be_fast = queue.pop_all(std::chrono::hours(24 * 365));
	CONCURRENT_TEST(size(should_be_fast) == 1);
	CONCURRENT_TEST(should_be_fast[0] == 0);
	
	auto const immediate = queue.try_pop_all();
	CONCURRENT_TEST(empty(immediate));
}



void test_blocking() {
	auto queue = concurrent::unbounded_queue<int>{};
	auto const value = 6;
	auto const time_to_wake_up = now() + duration;
	auto thread = std::jthread([&]{
		std::this_thread::sleep_until(time_to_wake_up);
		queue.emplace(value);
	});
	auto const result = queue.pop_all();
	CONCURRENT_TEST(now() >= time_to_wake_up);
	CONCURRENT_TEST(size(result) == 1);
	CONCURRENT_TEST(result.front() == value);
}

} // namespace

auto main() -> int {
	test_int();
	test_string();

#ifndef _MSC_VER
	test_copy_move();
#endif
	test_timeout();
	test_blocking();
}

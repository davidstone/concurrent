// Copyright IHS Markit Ltd 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <std_module/prelude.hpp>
#include <catch2/catch_test_macros.hpp>

import concurrent_queue;
import containers;
import std_module;

namespace {

TEST_CASE("int", "concurrent_queue") {
	auto queue = concurrent::unbounded_queue<int>{};
	queue.emplace(0);
	queue.push(7);
	auto const first_values = queue.pop_all();
	CHECK(size(first_values) == 2);
	CHECK(first_values[0] == 0);
	CHECK(first_values[1] == 7);
	queue.push(4);
	auto const second_values = queue.pop_all();
	CHECK(size(second_values) == 1);
}

// Tests ranges and conversions
TEST_CASE("string", "concurrent_queue") {
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
	CHECK(containers::equal(values, expected));
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
	auto operator=(copy_move_counter const &) noexcept -> copy_move_counter & {
		++s_copy_assigned;
		return *this;
	}
	auto operator=(copy_move_counter &&) noexcept -> copy_move_counter & {
		++s_move_assigned;
		return *this;
	}
	
	static auto default_constructed() noexcept -> std::size_t {
		return s_default_constructed;
	}
	static auto copy_constructed() noexcept -> std::size_t {
		return s_copy_constructed;
	}
	static auto move_constructed() noexcept -> std::size_t {
		return s_move_constructed;
	}
	static auto copy_assigned() noexcept -> std::size_t {
		return s_copy_assigned;
	}
	static auto move_assigned() noexcept -> std::size_t {
		return s_move_assigned;
	}
private:
	static inline std::size_t s_default_constructed = 0;
	static inline std::size_t s_copy_constructed = 0;
	static inline std::size_t s_move_constructed = 0;
	static inline std::size_t s_copy_assigned = 0;
	static inline std::size_t s_move_assigned = 0;
};

TEST_CASE("copy move", "concurrent_queue") {
	// Some of these tests will fail if the standard library implementation
	// makes unnecessary copies / moves.
	auto queue = concurrent::unbounded_queue<copy_move_counter>{};
	auto expected_default_constructed = static_cast<std::size_t>(0);
	auto expected_copy_constructed = static_cast<std::size_t>(0);
	auto expected_move_constructed = static_cast<std::size_t>(0);
	auto check_all = [&]{
		CHECK(copy_move_counter::default_constructed() == expected_default_constructed);
		CHECK(copy_move_counter::copy_constructed() == expected_copy_constructed);
		CHECK(copy_move_counter::move_constructed() == expected_move_constructed);
		CHECK(copy_move_counter::copy_assigned() == 0);
		CHECK(copy_move_counter::move_assigned() == 0);
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
constexpr auto duration = std::chrono::milliseconds(100);

TEST_CASE("timeout", "concurrent_queue") {
	auto queue = concurrent::unbounded_queue<int>{};
	auto const before_time_point = now();
	auto const values_time_point = queue.pop_all(before_time_point + duration);
	auto const after_time_point = now();
	auto const values_duration = queue.pop_all(duration);
	auto const after_duration = now();

	CHECK(after_time_point - before_time_point >= duration);
	CHECK(after_duration - after_time_point >= duration);

	CHECK(empty(values_time_point));
	CHECK(empty(values_duration));
	
	queue.push(0);
	auto const should_be_fast = queue.pop_all(std::chrono::hours(24 * 365));
	CHECK(size(should_be_fast) == 1);
	CHECK(should_be_fast[0] == 0);
	
	auto const immediate = queue.try_pop_all();
	CHECK(empty(immediate));
}


TEST_CASE("blocking", "concurrent_queue") {
	auto queue = concurrent::unbounded_queue<int>{};
	auto const value = 6;
	auto const time_to_wake_up = now() + duration;
	auto thread = std::jthread([&]{
		std::this_thread::sleep_until(time_to_wake_up);
		queue.emplace(value);
	});
	auto const result = queue.pop_all();
	CHECK(now() >= time_to_wake_up);
	CHECK(size(result) == 1);
	CHECK(result.front() == value);
}

} // namespace

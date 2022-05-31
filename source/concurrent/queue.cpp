// Copyright IHS Markit Ltd 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <concurrent/queue.hpp>

#include <bounded/scope_guard.hpp>

#include <containers/stable_vector.hpp>
#include <containers/vector.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

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

	{
		auto v = std::vector<copy_move_counter>();
		auto a = std::array<copy_move_counter, 3>();
		expected_default_constructed += size(a);
		containers::detail::assign_to_empty_or_append(
			v,
			std::move(a),
			containers::detail::exponential_reserve,
			[&] { return containers::end(v); },
			containers::detail::append_fallback
		);
		expected_move_constructed += size(a);
		CONCURRENT_TEST(copy_move_counter::default_constructed() == 3);
		CONCURRENT_TEST(copy_move_counter::copy_constructed() == 0);
		CONCURRENT_TEST(copy_move_counter::move_constructed() == 3);
		CONCURRENT_TEST(copy_move_counter::copy_assigned() == 0);
		CONCURRENT_TEST(copy_move_counter::move_assigned() == 0);
	}
	
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

constexpr auto reserved_size = 6'000'000'000;
template<typename T>
using Container = containers::stable_vector<T, reserved_size>;

template<typename C>
void reserve([[maybe_unused]] C & c) {
#if 0
	c.reserve(bounded::assume_in_range<containers::range_size_t<C>>(reserved_size));
#endif
}

struct spin_mutex {
	auto try_lock() -> bool {
		return !m_flag.test_and_set(std::memory_order_acquire);
	}
	auto lock() -> void {
		while (!try_lock()) {
			while (m_flag.test(std::memory_order_relaxed)) {
			}
		}
	}
	auto unlock() -> void {
		m_flag.clear(std::memory_order_release);
	}
private:
	std::atomic_flag m_flag = ATOMIC_FLAG_INIT;
};

void test_ordering(std::size_t number_of_readers, std::size_t number_of_writers, std::size_t bulk_size) {
	std::atomic<std::uint64_t> largest_read(0);
	std::atomic<std::uint64_t> items_read(0);
	std::atomic<std::uint64_t> number_of_writes(0);
	
	using value_type = int;
	auto const bulk_data_source = [=]{
		auto result = std::vector<value_type>(bulk_size);
			std::iota(result.begin(), result.end(), 0);
			return result;
		}();
	value_type const * const bulk_data_begin = bulk_data_source.data();
	value_type const * const bulk_data_end = bulk_data_source.data() + size(bulk_data_source);
	
	auto create_threads = [](auto const count, auto const function) {
		auto threads = std::vector<std::jthread>{};
		threads.reserve(count);
		for (std::size_t n = 0; n != count; ++n) {
			threads.emplace_back(function);
		}
		return threads;
	};
	
	auto update_atomic = [](auto & atomic, auto & local) { return bounded::scope_guard([&]{ atomic += local; }); };
	
	using namespace bounded::literal;
	auto queue = concurrent::basic_unbounded_queue<Container<value_type>, spin_mutex>();
	reserve(queue);
		
	auto const start = now();

	{
		// The reader thread should only see units of bulk_data, never a partial
		// update.
		auto const reader_threads = create_threads(number_of_readers, [&](std::stop_token token) {
			auto data = Container<value_type>();
			reserve(data);

			auto local_largest_read = std::uint64_t(0);
			auto const update_largest_read = bounded::scope_guard([&]{
				auto temp = largest_read.load();
				while (temp < local_largest_read and !largest_read.compare_exchange_weak(temp, local_largest_read)) {
				}
			});

			auto local_items_read = std::uint64_t(0);
			auto const update_items_read = update_atomic(items_read, local_items_read);

			// If a thread is waiting on a condition_variable and that
			// condition_variable has been notified that its condition is now
			// true and the thread has been interrupted, the interruption is
			// processed rather than the condition_variable unblocking normally
			// regardless of the order in which the notify and the interruption
			// occur. That means that this program would eventually (typically
			// after a few thousand iterations) fail because we read fewer items
			// than we wrote by not getting in one final read. To prevent that,
			// we get one last read.
			auto process_data = [&] {
				auto const count = size(data);
				local_largest_read = std::max(local_largest_read, static_cast<std::size_t>(count));
				local_items_read += count;
				CONCURRENT_TEST(count % bulk_size == 0_bi);
				for (auto it = begin(data); it != end(data); it += bounded::assume_in_range(bulk_size, 0_bi, bounded::constant<reserved_size>)) {
					CONCURRENT_TEST(containers::equal(bulk_data_begin, bulk_data_end, it));
				}
			};
			while (!token.stop_requested()) {
				data = queue.pop_all(token, std::move(data));
				process_data();
				containers::clear(data);
			}
			data = queue.try_pop_all(std::move(data));
			process_data();
		});

		auto const writer_threads = create_threads(number_of_writers, [&](std::stop_token token) {
			auto local_number_of_writes = std::uint64_t(0);
			auto const update_count_of_writes = update_atomic(number_of_writes, local_number_of_writes);
			while (!token.stop_requested()) {
				queue.append(bulk_data_source);
				++local_number_of_writes;
				std::this_thread::yield();
			}
		});
	
		std::this_thread::sleep_until(start + std::chrono::milliseconds(1000));
	}

	auto const end = now();
	
	CONCURRENT_TEST(items_read == number_of_writes * bulk_size);

	auto const time_taken = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
	std::cout << static_cast<double>(items_read) / static_cast<double>(time_taken) << " million messages / second\n";
	std::cout << largest_read.load() << " peak elements on queue\n";
}

struct parsed_args {
	std::size_t readers;
	std::size_t writers;
	std::size_t batch_size;
};

auto parse_args(int const argc, char const * const * argv) {
	if (argc == 1) {
		return parsed_args{1, 1, 2000};
	}
	if (argc != 4) {
		throw std::runtime_error("Usage is queue readers writers batch-size");
	}
	auto const readers = std::stoull(argv[1]);
	auto const writers = std::stoull(argv[2]);
	auto const batch_size = std::stoull(argv[3]);
	if (readers == 0) {
		throw std::runtime_error("Must have at least one reader thread");
	}
	if (writers == 0) {
		throw std::runtime_error("Must have at least one writer thread");
	}
	return parsed_args{readers, writers, batch_size};
}

} // namespace

int main(int const argc, char const * const * argv) {
	auto const args = parse_args(argc, argv);
	test_int();
	test_string();

#ifndef _MSC_VER
	test_copy_move();
#endif
	test_timeout();
	test_blocking();
	
	test_ordering(args.readers, args.writers, args.batch_size);
}

// Copyright IHS Markit Ltd 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <std_module/prelude.hpp>

#if defined NDEBUG
	#define CONCURRENT_NDEBUG_WAS_DEFINED NDEBUG
	#undef NDEBUG
#endif

#include <cassert>

import bounded;
import concurrent_queue;
import containers;
import std_module;

// Like assert, but always evaluated
#define CONCURRENT_TEST assert

#if defined CONCURRENT_NDEBUG_WAS_DEFINED
	#undef CONCURRENT_NDEBUG_WAS_DEFINED
#endif

namespace {

using namespace bounded::literal;

using thread_count = bounded::integer<1, 1'000'000>;

auto now() {
	return std::chrono::steady_clock::now();
}

constexpr auto reserved_size = 600'000'000_bi;
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
		return !m_flag.test_and_set(std::memory_order::acquire);
	}
	auto lock() -> void {
		while (!try_lock()) {
			while (m_flag.test(std::memory_order::relaxed)) {
			}
		}
	}
	auto unlock() -> void {
		m_flag.clear(std::memory_order::release);
	}
private:
	std::atomic_flag m_flag;
};

void test_ordering(thread_count const number_of_readers, thread_count const number_of_writers, std::size_t bulk_size) {
	std::atomic<std::uint64_t> largest_read(0);
	std::atomic<std::uint64_t> items_read(0);
	std::atomic<std::uint64_t> number_of_writes(0);
	
	using value_type = int;
	auto const bulk_data_source = containers::vector<value_type>(containers::integer_range(bulk_size));
	value_type const * const bulk_data_begin = containers::data(bulk_data_source);
	value_type const * const bulk_data_end = bulk_data_begin + containers::size(bulk_data_source);
	
	auto create_threads = [](thread_count const count, auto const function) {
		return containers::dynamic_array<std::jthread>(containers::generate_n(
			count,
			[&] { return bounded::no_lazy_construction(std::jthread(function)); }
		));
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
				local_items_read += static_cast<std::uint64_t>(count);
				CONCURRENT_TEST(static_cast<std::size_t>(count) % bulk_size == 0);
				for (auto it = begin(data); it != end(data); it += bounded::assume_in_range(bulk_size, 0_bi, reserved_size)) {
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
	thread_count readers;
	thread_count writers;
	std::size_t batch_size;
};

auto parse_args(int const argc, char const * const * argv) {
	if (argc == 1) {
		return parsed_args{1_bi, 1_bi, 2000};
	}
	if (argc != 4) {
		throw std::runtime_error("Usage is queue readers writers batch-size");
	}
	auto const readers = bounded::check_in_range<thread_count>(std::stoull(argv[1]));
	auto const writers = bounded::check_in_range<thread_count>(std::stoull(argv[2]));
	auto const batch_size = std::stoull(argv[3]);
	return parsed_args{readers, writers, batch_size};
}

} // namespace

auto main(int const argc, char const * const * argv) -> int {
	auto const args = parse_args(argc, argv);
	test_ordering(args.readers, args.writers, args.batch_size);
}

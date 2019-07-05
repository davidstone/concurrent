// Copyright IHS Markit Ltd 2017.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/concurrent/queue.hpp>

#include <boost/program_options.hpp>
#include <boost/thread/scoped_thread.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <numeric>
#include <string>

namespace {

void test_int() {
	auto queue = boost::concurrent::unbounded_queue<int>{};
	queue.emplace(0);
	queue.push(7);
	auto const first_values = queue.pop_all();
	assert(size(first_values) == 2);
	assert(first_values[0] == 0);
	assert(first_values[1] == 7);
	queue.push(4);
	auto const second_values = queue.pop_all();
	assert(size(second_values) == 1);
}

// Tests ranges and conversions
void test_string() {
	auto queue = boost::concurrent::unbounded_queue<std::string>{};
	queue.emplace("Reese");
	queue.push("Finch");
	char const * array[] = {
		"Carter", "Fusco"
	};
	queue.append(std::begin(array), std::end(array));
	auto const values = queue.pop_all();
	auto const expected = std::array<char const *, 4>{
		"Reese", "Finch", "Carter", "Fusco"
	};
	assert(std::equal(values.begin(), values.end(), expected.begin(), expected.end()));
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
	auto queue = boost::concurrent::unbounded_queue<copy_move_counter>{};
	auto expected_default_constructed = static_cast<std::size_t>(0);
	auto expected_copy_constructed = static_cast<std::size_t>(0);
	auto expected_move_constructed = static_cast<std::size_t>(0);
	auto check_all = [&]{
		assert(copy_move_counter::default_constructed() == expected_default_constructed);
		assert(copy_move_counter::copy_constructed() == expected_copy_constructed);
		assert(copy_move_counter::move_constructed() == expected_move_constructed);
		assert(copy_move_counter::copy_assigned() == 0);
		assert(copy_move_counter::move_assigned() == 0);
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
	
	queue.append(array.begin(), array.end());
	expected_copy_constructed += size(array);
	check_all();
	
	queue.pop_all();
	check_all();
	
	queue.append(std::make_move_iterator(array.begin()), std::make_move_iterator(array.end()));
	expected_move_constructed += size(array);
	check_all();
}

#endif

auto now() {
	return boost::chrono::steady_clock::now();
}
auto const duration = boost::chrono::milliseconds(100);

void test_timeout() {
	auto queue = boost::concurrent::unbounded_queue<int>{};
	auto const before_time_point = now();
	auto const values_time_point = queue.pop_all(before_time_point + duration);
	auto const after_time_point = now();
	auto const values_duration = queue.pop_all(duration);
	auto const after_duration = now();

	assert(after_time_point - before_time_point >= duration);
	assert(after_duration - after_time_point >= duration);

	assert(empty(values_time_point));
	assert(empty(values_duration));
	
	queue.push(0);
	auto const should_be_fast = queue.pop_all(boost::chrono::hours(24 * 365));
	assert(size(should_be_fast) == 1);
	assert(should_be_fast[0] == 0);
	
	auto const immediate = queue.try_pop_all();
	assert(empty(immediate));
}



using thread_t = boost::scoped_thread<boost::interrupt_and_join_if_joinable>;

void test_blocking() {
	auto queue = boost::concurrent::unbounded_queue<int>{};
	auto const value = 6;
	auto const time_to_wake_up = now() + duration;
	auto thread = thread_t([&]{
		boost::this_thread::sleep_until(time_to_wake_up);
		queue.emplace(value);
	});
	auto const result = queue.pop_all();
	assert(now() >= time_to_wake_up);
	assert(size(result) == 1);
	assert(result.front() == value);
}

template<typename Function>
struct scope_guard {
	constexpr explicit scope_guard(Function function_) noexcept(std::is_nothrow_move_constructible<Function>{}):
		function(std::move(function_)),
		is_active(true)
	{
	}
	
	constexpr scope_guard(scope_guard && other) noexcept(std::is_nothrow_move_constructible<Function>{}):
		function(	std::move(other.function)),
		is_active(std::exchange(other.is_active, false))
	{
	}

	~scope_guard() {
		if (is_active) {
			std::move(function)();
		}
	}
	
	constexpr void dismiss() noexcept {
		is_active = false;
	}

private:
	Function function;
	bool is_active;
};

template<typename Function>
constexpr auto make_scope_guard(Function function) noexcept(std::is_nothrow_move_constructible<Function>{}) {
	return scope_guard<Function>(std::move(function));
}


struct Reader {
	std::size_t threads;
	std::size_t cost_per_item;
	std::size_t cost_per_batch;
};

void test_ordering(Reader const reader, std::size_t number_of_writers, std::size_t bulk_size) {
	std::atomic<std::uint64_t> number_of_reads(0);
	std::atomic<std::uint64_t> items_read(0);
	std::atomic<std::uint64_t> number_of_writes(0);
	
	auto const start = now();

	{
		auto const individual_data = 42;
		auto const bulk_data = [=]{
			auto result = std::vector<int>(bulk_size);
			std::iota(result.begin(), result.end(), 0);
			return result;
		}();
	
		auto queue = boost::concurrent::unbounded_queue<int>{};
		
		auto create_threads = [](auto const count, auto const function) {
			auto threads = std::vector<thread_t>{};
			threads.reserve(count);
			for (std::size_t n = 0; n != count; ++n) {
				threads.emplace_back(function);
			}
			return threads;
		};
		
		// `cost` is volatile to ensure that we generate a read from memory
		auto wait = [](std::size_t const volatile cost) {
			for (std::size_t n = 0; n != cost; ++n) {
			}
		};
		
		auto update_atomic = [](auto & atomic, auto & local) { return make_scope_guard([&]{ atomic += local; }); };
		
		// Each thread is either adding individual_data, or atomically adding all of
		// bulk_data. The reader thread should only see units of individual_data or
		// bulk_data, never a partial update.
		auto const reader_threads = create_threads(reader.threads, [&]{
			auto data = std::vector<int>{};

			auto local_number_of_reads = std::uint64_t(0);
			auto const update_number_of_reads = update_atomic(number_of_reads, local_number_of_reads);

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
				++local_number_of_reads;
				local_items_read += size(data);
					if (*it == individual_data) {
						++it;
						wait(reader.cost_per_item);
					} else {
						for (auto const expected : bulk_data) {
							assert(it != data.end());
							assert(*it == expected);
							++it;
							wait(reader.cost_per_item);
						}
					}
				}
				wait(reader.cost_per_batch);
			};
			try {
				while (true) {
					data = queue.pop_all(std::move(data));
					process_data();
					data.clear();
				}
			} catch (boost::thread_interrupted const &) {
				data = queue.try_pop_all(std::move(data));
				process_data();
			}
		});

		auto const writer_threads = create_threads(number_of_writers, [&]{
			auto local_number_of_writes = std::uint64_t(0);
			auto const update_count_of_writes = update_atomic(number_of_writes, local_number_of_writes);
			while (!boost::this_thread::interruption_requested()) {
				++local_number_of_writes;
				queue.append(bulk_data.begin(), bulk_data.end());
			}
		});
	
		boost::this_thread::sleep_until(start + boost::chrono::milliseconds(1000));
	}

	auto const end = now();
	
	assert(items_read == number_of_writes * bulk_size);

	auto const time_taken = boost::chrono::duration_cast<boost::chrono::microseconds>(end - start).count();
	std::cout << "Millions of messages / second: " << static_cast<double>(items_read) / time_taken << '\n';
}

}	// namespace

int main(int argc, char ** argv) {
	namespace po = boost::program_options;
	po::options_description description("Allowed options");
	description.add_options()
		("help", "produce help message")
		("item-cost", po::value<std::size_t>()->default_value(0), "Amount of work it takes to process an item on the read side (independent of batch size), measured in reads of memory")
		("batch-cost", po::value<std::size_t>()->default_value(0), "Amount work it takes to process a batch of items on the read side, measured in reads of memory. If there is no benefit from batching, this should be 0")
		("readers", po::value<std::size_t>()->default_value(1), "Number of threads reading data (minimum of 1)")
		("writers", po::value<std::size_t>()->default_value(1), "Number of threads writing data (minimum of 1)")
		("batch-size", po::value<std::size_t>()->default_value(2000), "Number of elements the writers are adding at a time")
	;
	
	po::variables_map options;
	po::store(po::parse_command_line(argc, argv, description), options);
	po::notify(options);

	if (options.count("help")) {
		std::cout << description << '\n';
		return 0;
	}
	
	auto const cost_per_item = options["item-cost"].as<std::size_t>();
	auto const cost_per_batch = options["batch-cost"].as<std::size_t>();
	auto const number_of_readers = options["readers"].as<std::size_t>();
	auto const number_of_writers = options["writers"].as<std::size_t>();
	auto const batch_size = options["batch-size"].as<std::size_t>();
	
	if (number_of_readers == 0) {
		std::cerr << "Must have at least one reader thread\n";
		return 1;
	}
	if (number_of_writers == 0) {
		std::cerr << "Must have at least one writer thread\n";
		return 1;
	}
	
	test_int();
	test_string();

#ifndef _MSC_VER
	test_copy_move();
#endif
	test_timeout();
	test_blocking();
	
	auto const reader = Reader{number_of_readers, cost_per_item, cost_per_batch};
	test_ordering(reader, number_of_writers, batch_size);
}

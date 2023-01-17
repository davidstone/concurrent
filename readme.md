# Introduction

If you have multiple threads that need to communicate, you need some way to do so safely and efficiently. A thread-safe queue allows you to do this. In this model, one or more threads are "producer" or "writer" threads, and one or more threads are "consumer" or "reader" threads.

# Highlights

* The bulk interface is faster than other queues for many common load patterns
* Almost any element type is supported with very few requirements
* Supports any number of producers
* Supports any number of consumers (although it works best with only one)
* Can dynamically grow and shrink in memory as needed
* The interface allows for very little undefined behavior: for the most part, as long as your special member functions like move constructors do not reference the queue you are inserting into, you are fine
* Header-only

# Tutorial / Quick start

This is a sample program that has two producer threads that are generating the numbers from 0 to 9 and one reader thread that prints the numbers as it sees them. This demonstrates typical usage of a `concurrent::unbounded_queue`.

```
#include <thread>

import concurrent_queue;

struct producer_t {
	explicit producer_t(concurrent::unbounded_queue<int> & queue):
		thread([&]{
			for (int n = 0; n != 10; ++n) {
				queue.push(n);
			}
		})
	{
	}
private:
	std::jthread thread;
};

struct consumer_t {
	explicit consumer_t(concurrent::unbounded_queue<int> & queue):
		thread([&](std::stop_token token) {
			while (!token.stop_requested()) {
				for (int const value : queue.pop_all()) {
					std::cout << value << ' ';
				}
				std::cout << '\n';
			}
		})
	{
	}
private:
	std::jthread thread;
};

int main() {
	auto queue = concurrent::unbounded_queue<int>();
	auto consumer = consumer_t(queue);
	auto producers = std::array<producer_t, 2>{
		producer_t(queue),
		producer_t(queue)
	};
}
```

If you run this program, you might see output like this:

```
0 1 2 3 4 5 6 7 8 9 
0 1 2 3 4 5 6 7 8 9 
```

Or this:

```
0 
1 
2 
3 
4 
5 
6 
7 
8 
9 
0 
1 
2 
3 
4 
5 
6 
7 
8 
9 
```

Or something in the middle like this:

```
0 1 2 3 4 
5 0 1 6 7 2 8 
3 4 5 6 
7 
8 
9 9
```

The output of the queue is guaranteed to be consistent with the input. This means that elements that a single thread adds to the queue first will be removed first. If two threads have any sort of sequencing guarantees, the queue will respect that.

## Optimizing your access

The call to `push` has the same rules around when memory is allocated as `std::vector`. This means that most of the time, push is quick, but sometimes, it is very slow. It is not good for code to be slow inside your lock, as this will dramatically reduce your concurrency. What can we do to minimize these memory allocations? We can slightly change how we write our code on the consumer side, when we call `pop_all`. If we change our consumer thread to look like this:

```
struct consumer_t {
	explicit consumer_t(concurrent::unbounded_queue<int> & queue):
		thread([&](std::stop_token token) {
			auto buffer = concurrent::unbounded_queue<int>::container_type();
			while (!token.stop_requested()) {
				buffer = queue.pop_all(std::move(buffer));
				for (int const value : buffer) {
					std::cout << value << ' ';
				}
				std::cout << '\n';
				buffer.clear();
			}
		})
	{
	}
private:
	std::jthread thread;
};
```

This will ensure that we only allocate memory at the high-memory points of our application. This can lead to significant gains in performance.

You can also call `reserve` on the queue before you begin adding elements to it, and call reserve on the vector you pass as an argument to `pop_all`. Using this approach, if you `reserve` more than you `push` before you call `pop_all`, you never allocate memory inside a lock.

## Tricky parts

You might even see this as your output:

```
0 
1 
2 
3 
4 
5 
6 
7 
8 
9 
```

This is because of the time between requesting a stop and checking whether a stop was requested. If you need to guarantee that you read all elements that have been added into the queue at shutdown, you need to call `try_pop_all` one final time before shutting down.

# [Reference](reference.md)

# Q&A

## Is this thing lock-free? Isn't lock-free better?

No, it is not lock-free, but that is not necessarily a bad thing.

Many people have a few misconceptions over what it means for something to be lock-free and why they want it. If all you care about is overall throughput, you do not necessarily want a lock-free queue. "Lock-free" refers specifically to a guarantee that the system as a whole can make progress even if some (but not all) of the threads are suspended indefinitely.

Lock-free queues may have better worst-case performance guarantees than lock-based queues by virtue of making fewer system calls and being less reliant on the thread scheduler.

The main goal of this queue is to maximize total throughput. It achieves this goal by giving the user as many elements back as possible on a pop operation. This means that there are fewer requests back to the queue for more data, and thus, less contention.

## How does this queue compare to other queues?

There are many other concurrent queue implementations. Most of them are lock-free queues that place certain restrictions on the element type you can enqueue. boost::lockfree::queue requires elements have a trivial assignment operator and a trivial destructor. Intel's TBB (Threading Building Blocks) requires elements have a default constructor and does not provide the lock-free guarantee. moodycamel::ConcurrentQueue requires move-assignable types. All of them have an interface that requires the user to first construct an object to pass in by reference, which is then populated by the queue.

This queue, on the other hand, requires very little of your elements. The minimal interface required of your type is just what is needed to be able to call `emplace_back` on a vector of your types: [MoveInsertable](http://en.cppreference.com/w/cpp/concept/MoveInsertable) and [EmplaceConstructible](http://en.cppreference.com/w/cpp/concept/EmplaceConstructible). If your type does not meet the MoveInsertable requirement (typically because it is not copyable or movable), then it is possible to remove this requirement by telling the queue to store the data internally in a `std::deque` instead of a `std::vector`. This typically leads to slightly less efficient code, but allows you to enqueue virtually any type.

This queue also uses a value-based interface (all pop functions return a value). This makes it easier to work with in many situations and does not require you to needlessly construct a value just to overwrite it when you pop an element from the queue.

## OK, but what about performance? How fast is this?

It's a bit difficult to answer that question, as it depends heavily on hardware, number of threads, the message rate, and the specific type of elements you are enqueing.

To try to answer this question and show that I did not just write a benchmark that suites my queue but does not reflect reality, this queue is integrated into the [moodycamel benchmarks](https://github.com/davidstone/concurrentqueue). This shows that when the queue is used in ways similar to how it is shown above (mostly, when you use the bulk-dequeue operation), this queue tends to outperform all other queues. When you do not use the queue properly, it performs similar to or slightly worse than the `boost::lockfree::queue`.

To help you tune the number of readers and writers on your particular hardware, there is also a test program included in this repository. Simply compile and run `source/queue.cpp` (pass `--help` to see the configuration parameters).

Using this test application on my computer and properly tuning the number of readers, writers, and batch size (elements per write), my laptop is able to push through about 3.5 billion int / second at a maximum, including time spent writing the values and reading them back to ensure we read the correct values. This works out to about 82% of the theoretical maximum for my memory bus (using an Intel i7 6700HQ) if the only data transfer happening were the data in the queue. This same maximum can come from any recent version of gcc or clang on Linux or under minGW, or from Visual Studio 2017.

The exact right balance of readers, writers, and batch size seems to be slightly different with Visual Studio vs. gcc or clang, so these next example numbers (for my particular hardware) are for gcc or clang with `-O3 -match=native -flto`. The queue can process 1 billion int per second with 1 reader thread, 1 writer thread, and 1000 int added per batch. This throughput scales approximately linearly with batch size up until this point, so a batch size of 500 gives you approximately 500 million int per second. After that, it's a little trickier to improve throughput, but as far as I can tell, the ideal number on this type of processor gets you up to 3.5 billion int per second with 3 reader threads, 1 writer thread, and a batch size of 5080.

Using the queue in the worst possible way can degrade performance pretty significantly from this point. The worst case for the queue is to have many (10+) readers doing almost no work per item with a single writer adding 1 element per batch. This case gives you only 550,000 int / second.

To help understand how the numbers work in the middle, 1 reader and 4 writers writing 40 elements per batch gives about 100 million int per second through the queue.
# Getting Started

You may want to consult [the tutorial](readme.md).

# Queues supported

There are three types of queues provided by the concurrent library: `basic_unbounded_queue`, `basic_blocking_queue`, and `basic_dropping_queue`. These queues all have different behavior when the user attempts to add elements but the queue is  already full.

## basic_unbounded_queue

	template<typename Container>
	struct basic_unbounded_queue;
	
	template<typename T, typename Allocator = std::allocator<T>>
	using unbounded_queue = basic_unbounded_queue<std::vector<T, Allocator>>;

`basic_unbounded_queue` is the simplest queue: it does not have the concept of being full. It allows the user to continue to add elements until the underlying container is unable to allocate any more space.

## basic_blocking_queue

	template<typename Container>
	struct basic_blocking_queue;
	
	template<typename T, typename Allocator = std::allocator<T>>
	using blocking_queue = basic_blocking_queue<std::vector<T, Allocator>>;

`basic_blocking_queue` accepts a size parameter in its constructor. If the queue already has at least `max_size` elements when the user attempts to add data (by calling `append`, `emplace`, or `push`), that call will block until the queue is reduced in size to have fewer than `max_size` elements.

## basic_dropping_queue

	template<typename Container>
	struct basic_dropping_queue;
	
	template<typename T, typename Allocator = std::allocator<T>>
	using dropping_queue = basic_dropping_queue<std::vector<T, Allocator>>;

`basic_dropping_queue` accepts a size parameter in its constructor. If the queue already has at least `max_size` elements when the user attempts to add data (by calling `append`, `emplace`, or `push`), the queue is first cleared so that it has 0 elements, then the new elements are added.

# Reference

## Template Parameters

| Parameter | Definition |
| :--------- | :--------- |
| Container | The underlying container that the queue uses to store its data. The requirements and exception guarantees for any operation on the queue exactly match the documented call to the underlying container, except where noted otherwise. `Container` must be [DefaultConstructible](http://en.cppreference.com/w/cpp/concept/DefaultConstructible) |

## Member Types

| Type | Definition |
| :--------- | :--------- |
| container_type |  `Container` template parameter |
| value_type | `container_type::value_type` |


## General Behavior

Except where noted otherwise, all operations are thread-safe.

Any call that is documented as blocking can throw an exception of type `boost::thread_interrupted` if the the thread that is blocked is interrupted. See the documentation for `boost::thread::interrupt`.

Some definitions refer to "`container`" as though it were a data member. This value is for exposition only, and can be assumed to be a value of type `container_type` that contains all current values in the queue.

The behavior of any function which accepts a `time_point` or `duration` parameter is undefined if any operations on the `time_point` or `duration` calls any member functions of the queue object.


## Special Member Functions

| Function | Definition |
| :--------- | :--------- |
| constructor | `basic_unbounded_queue` is default constructible. This constructs an empty queue. `basic_blocking_queue` and `basic_dropping_queue` are constructible from `container_type::size_type`. This constructs an empty queue and sets the `max_size` to that value. |
| move constructor | Moves the contents of the queue and the `max_size` (if present) from the old queue to the new queue. Not thread safe, as the assumption is that the source of the move is a temporary. Only defined if `container_type` is [MoveConstructible](http://en.cppreference.com/w/cpp/concept/MoveConstructible) |
| copy constructor | Deleted |
| move assignment | Deleted |
| copy assignment | Deleted |
| destructor | Destroys the underlying container. The behavior is undefined if any threads are accessing the queue when the destructor is called. |


## append

### `auto append(Iterator first, Sentinel last)`

#### Definition

Adds elements to the underlying container by calling `container.insert(container.end(), first, last)`.

This should be preferred over `emplace` or `push` where possible, as it typically leads to better performance.

Returns the number of elements dropped for `basic_dropping_queue`, otherwise returns `void`.

Elements added to the queue by this operation are guaranteed to be added atomically. No other elements will be added in the middle, and if a consumer pops everything off the queue, they will either see all of the elements in the range or none of them.

#### Requirements

`container.insert(container.end(), first, last)` must be a valid expression that adds all elements in the range `[first, last)` to the end of the container. The behavior is undefined if calling `container.insert(container.end(), first, last)` would call any member functions on this queue object.


## emplace

### `auto emplace(Args && ... args)`

#### Definition

Adds an element to the underlying container by calling `container.emplace_back(std::forward<Args>(args)...)`. Returns the number of elements dropped for `basic_dropping_queue`, otherwise returns `void`.

#### Requirements

`container.emplace_back(std::forward<Args>(args)...)` must be a valid expression that adds a single item into the container. For `std::vector` as the `container_type`, this requires that `value_type` is [MoveInsertable](http://en.cppreference.com/w/cpp/concept/MoveInsertable) and [EmplaceConstructible](http://en.cppreference.com/w/cpp/concept/EmplaceConstructible). The behavior is undefined if calling `container.emplace_back(std::forward<Args>(args)...)` would call any member functions on this queue object.


## push

### `auto push(value_type && value)`

Equivalent to `emplace(std::move(value))`

### `auto push(value_type const & value)`

Equivalent to `emplace(value)`


## pop_all

### `container_type pop_all(container_type storage = container_type{})`

#### Definition

Blocks until there is data in the queue. Returns all messages in the queue. After this call, the queue's internal container is equal to the original value of `storage`. This overload will never return an empty container.

#### Requirements

`container_type` must model the [Swappable](http://en.cppreference.com/w/cpp/concept/Swappable) and [MoveConstructible](http://en.cppreference.com/w/cpp/concept/MoveConstructible) concepts. The behavior is undefined if `using std::swap; swap(container, storage);` calls any member functions on this queue object.

### `container_type pop_all(boost::chrono::time_point<Clock, Duration> timeout, container_type storage = container_type{})`

#### Definition

Blocks until there is data in the queue or `timeout` is reached. Returns all messages in the queue. After this call, the queue's internal container is equal to the original value of `storage`. This overload will only return an empty container if the timeout is reached and there is still no data in the queue.

This function may block beyond `timeout` due to scheduling or resource contention delays.

#### Requirements

`container_type` must model the [Swappable](http://en.cppreference.com/w/cpp/concept/Swappable) and [MoveConstructible](http://en.cppreference.com/w/cpp/concept/MoveConstructible) concepts. The behavior is undefined if `using std::swap; swap(container, storage);` calls any member functions on this queue object.

### `container_type pop_all(boost::chrono::duration<Rep, Period> timeout, container_type storage = container_type{})`

#### Definition

Blocks until there is data in the queue or `timeout` time has passed. Returns all messages in the queue. After this call, the queue's internal container is equal to the original value of `storage`. This overload will only return an empty container if `timeout` time has passed and there is still no data in the queue.

A steady clock is used to measure the duration. This function may block for longer than `timeout` due to scheduling or resource contention delays. 

#### Requirements

`container_type` must model the [Swappable](http://en.cppreference.com/w/cpp/concept/Swappable) and [MoveConstructible](http://en.cppreference.com/w/cpp/concept/MoveConstructible) concepts. The behavior is undefined if `using std::swap; swap(container, storage);` calls any member functions on this queue object.


## try_pop_all

### container_type try_pop_all(container_type storage = container_type{})

#### Definition

Returns all messages in the queue. After this call, the queue's internal container is equal to the original value of `storage`. This overload will return an empty container if the queue is empty when `try_pop_all` is called.

#### Requirements

`container_type` must model the [Swappable](http://en.cppreference.com/w/cpp/concept/Swappable) and [MoveConstructible](http://en.cppreference.com/w/cpp/concept/MoveConstructible) concepts. The behavior is undefined if `using std::swap; swap(container, storage);` calls any member functions on this queue object.


## pop_one

### `value_type pop_one()`

#### Definition

Blocks until there is data in the queue. Returns the first element in the queue and removes it from the queue.

#### Requirements

`value_type` must model the [MoveConstructible](http://en.cppreference.com/w/cpp/concept/MoveConstructible) concept. The behavior is undefined if the move constructor calls any member functions on this queue object.

The expression `container.front()` must produce a value of (possible reference-qualified) `value_type`. `container.pop_front()` must be valid and remove the first element from the container. The behavior is undefined if `container.front()` or `container.pop_front()` calls any member functions on this queue object.

### `boost::optional<value_type> pop_one(boost::chrono::time_point<Clock, Duration> timeout)`

#### Definition

Blocks until there is data in the queue or `timeout` is reached. Returns the first element in the queue and removes it from the queue. If there is no element in the queue because the timeout was reached, returns `boost::none`. 

This function may block beyond `timeout` due to scheduling or resource contention delays.

#### Requirements

`value_type` must model the [MoveConstructible](http://en.cppreference.com/w/cpp/concept/MoveConstructible) concept. The behavior is undefined if the move constructor calls any member functions on this queue object.

The expression `container.front()` must produce a value of (possible reference-qualified) `value_type`. `container.pop_front()` must be valid and remove the first element from the container. The behavior is undefined if `container.front()` or `container.pop_front()` calls any member functions on this queue object.

### `boost::optional<value_type> pop_one(boost::chrono::duration<Rep, Period> timeout)`

#### Definition

Blocks until there is data in the queue or `timeout` time has passed. Returns the first element in the queue and removes it from the queue. If there is no element in the queue because the timeout was reached, returns `boost::none`. 

A steady clock is used to measure the duration. This function may block for longer than `timeout` due to scheduling or resource contention delays. 

#### Requirements

`value_type` must model the [MoveConstructible](http://en.cppreference.com/w/cpp/concept/MoveConstructible) concept. The behavior is undefined if the move constructor calls any member functions on this queue object.

The expression `container.front()` must produce a value of (possible reference-qualified) `value_type`. `container.pop_front()` must be valid and remove the first element from the container. The behavior is undefined if `container.front()` or `container.pop_front()` calls any member functions on this queue object.


## try_pop_one

### `boost::optional<value_type> try_pop_one()`

#### Definition

Returns the first element in the queue and removes it from the queue if the queue is not empty, otherwise, returns `boost::none`. 

#### Requirements

`value_type` must model the [MoveConstructible](http://en.cppreference.com/w/cpp/concept/MoveConstructible) concept. The behavior is undefined if the move constructor calls any member functions on this queue object.

The expression `container.front()` must produce a value of (possible reference-qualified) `value_type`. `container.pop_front()` must be valid and remove the first element from the container. The behavior is undefined if `container.front()` or `container.pop_front()` calls any member functions on this queue object.


## clear

### `void clear()`

#### Definition

Calls `container.clear()`.

#### Requirements

The behavior is undefined is `container.clear()` calls any member functions on this queue.


## reserve

### `void reserve(Size size)`

#### Definition

Calls `container.reserve(size)`

#### Requirements

The behavior is undefined is `container.reserve(size)` calls any member functions on this queue.


## size

### `auto size() const`

#### Definition

Returns `container.size()`

#### Requirements

The behavior is undefined is `container.size()` calls any member functions on this queue.


## max_size

### `container_type::size_type max_size() const`

#### Definition

Returns the maximum size of the queue.

#### Requirements

`max_size` is only defined for `basic_blocking_queue` and `basic_dropping_queue`, not for `basic_unbounded_queue`. `container_type::size_type` must model the [CopyConstructible](http://en.cppreference.com/w/cpp/concept/CopyConstructible) concept. The behavior is undefined if the copy constructor of `container_type::size_type` calls any member functions on this queue object.


# Supported Compilers

This library requires C++14 support. The specific features used require one of the following (or newer)

* clang 3.4
* gcc 4.9
* Visual Studio 2015 (MSVC 19.0, Visual Studio 14)


// Copyright David Stone 2023.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

export module concurrent.locked_access;

import std_module;

namespace concurrent {

template<typename T, typename Mutex>
struct locked_t {
	static_assert(std::is_reference_v<T>);
	constexpr locked_t(T value_, Mutex & mutex):
		m_value(value_),
		m_lock(mutex)
	{
	}
	constexpr auto value() const & -> T {
		return m_value;
	}
	auto value() && = delete;
private:
	T m_value;
	std::unique_lock<Mutex> m_lock;
};
template<typename T, typename Mutex>
locked_t(T &, Mutex &) -> locked_t<T &, Mutex>;

export template<typename T, typename Mutex = std::mutex>
struct locked_access {
	constexpr auto locked() & {
		return locked_t(m_value, m_mutex);
	}
	constexpr auto locked() const & {
		return locked_t(m_value, m_mutex);
	}
	constexpr auto unlocked() const -> T const & {
		return m_value;
	}

private:
	mutable Mutex m_mutex;
	[[no_unique_address]] T m_value;
};

} // namespace concurrent
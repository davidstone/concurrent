module;

#include <operators/forward.hpp>

export module concurrent.map;

import containers;
import std_module;

namespace concurrent {

template<typename Mapped>
struct inserted_t {
	Mapped & mapped;
	bool inserted;
};

// All operations on the map are thread-safe. Keeping operations on the mapped
// value thread safe is up to the user. For a non-thread-safe mapped type, use
// `locked_access<Mapped>`.
export template<
	typename Key,
	typename Mapped,
	typename Hash = std::hash<Key>,
	typename KeyEqual = std::equal_to<>,
	typename Mutex = std::mutex
>
struct map {
	map() = default;

	template<typename K = Key>
	auto lookup(K && key) const & -> Mapped const * {
		auto _ = std::scoped_lock(m_mutex);
		return containers::lookup(m_map, OPERATORS_FORWARD(key));
	}
	template<typename K = Key>
	auto lookup(K && key) & -> Mapped * {
		auto _ = std::scoped_lock(m_mutex);
		return containers::lookup(m_map, OPERATORS_FORWARD(key));
	}

	template<typename K = Key>
	auto try_emplace(K && key, auto && ... mapped_args) & -> inserted_t<Mapped> {
		auto _ = std::scoped_lock(m_mutex);
		auto const inserted = m_map.try_emplace(
			OPERATORS_FORWARD(key),
			OPERATORS_FORWARD(mapped_args)...
		);
		return {inserted.first->second, inserted.second};
	}

	auto for_each(this auto & self, auto && function) -> void {
		auto _ = std::scoped_lock(self.m_mutex);
		for (auto & element : self.m_map) {
			function(element);
		}
	}

private:
	std::unordered_map<Key, Mapped, Hash, KeyEqual> m_map;
	mutable Mutex m_mutex;
};

} // namespace concurrent

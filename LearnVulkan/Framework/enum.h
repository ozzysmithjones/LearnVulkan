#pragma once
#include <concepts>
#include <type_traits>
#include <bit>

template<typename T>
	requires std::is_enum_v<T>
class EnumBitset
{
public:

	EnumBitset(uint64_t value) : mask(value) {}
	EnumBitset() : mask(0) {}

	template<std::same_as<T> ... Args>
	EnumBitset(Args... enum_values) : mask(((1ull << static_cast<uint64_t>(enum_values)) | ...)) {}

	bool is_set(T enum_value) const {
		return (1ull << static_cast<uint64_t>(enum_value)) & mask;
	}

	void set(T enum_value) {
		return mask |= (1ull << static_cast<uint64_t>(enum_value));
	}

	void clear(T enum_value) {
		return mask &= ~(1ull << static_cast<uint64_t>(enum_value));
	}

	template<T enum_value>
	bool is_set() const {
		return (1ull << static_cast<uint64_t>(enum_value)) & mask;
	}

	bool is_empty() const {
		return mask == 0;
	}

	template<T enum_value>
	void set() {
		return mask |= (1ull << static_cast<uint64_t>(enum_value));
	}

	template<T enum_value>
	void clear() {
		return mask &= ~(1ull << static_cast<uint64_t>(enum_value));
	}

	std::size_t count() const {
		return std::popcount(mask);
	}

	bool contains(EnumBitset<T> other) {
		return (mask & other.mask) == other.mask;
	}

	template<std::invocable<T> F>
	void for_each(F&& func) const {
		uint64_t it = mask;
		while (it) {
			uint64_t index = std::countr_zero(it);
			it &= ~(1ull << index);
			func(static_cast<T>(index));
		}
	}

	uint64_t get_value() const {
		return mask;
	}

private:
	uint64_t mask;
};

template<typename T>
static EnumBitset<T> merge(EnumBitset<T> a, EnumBitset<T> b) {
	return EnumBitset(a.get_value() | b.get_value());
}

template<typename T>
static EnumBitset<T> intersect(EnumBitset<T> a, EnumBitset<T> b) {
	return EnumBitset(a.get_value() & b.get_value());
}




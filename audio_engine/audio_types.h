#ifndef AUDIO_TYPES_H
#define AUDIO_TYPES_H

#include <cstdint>
#include <chrono>



namespace audio_engine
{
	template <typename T, typename... U>
	std::vector<std::decay_t<T>> make_vector(T&& arg, U&&... args) {
		auto result = std::vector<std::decay_t<T>>{};
		result.reserve(1 + sizeof...(args));
		result.emplace_back(std::forward<T>(arg));
		(result.emplace_back(std::forward<U>(args)), ...);
		return result;
	}

	template <typename T, typename U>
	struct allocator_rebind {
		typedef typename std::allocator_traits<T>::template rebind_alloc<U> type;
	};

	template <typename T, typename U>
	typename allocator_rebind<T, U>::type convert_allocator(const T& alloc) {
		return typename allocator_rebind<T, U>::type(alloc);
	}

	constexpr size_t sample_block_size = 480;
	constexpr auto sample_rate = 48000;

	using sample = float;
	using sample_block = sample[sample_block_size];
	using sample_state = uint8_t; //at most 256 (0-255) sample states supported on a single audio buffer 
	using sample_duration_t = std::chrono::duration<long long, std::ratio<1, sample_rate>>;
	using ring_buffer_allocator_t = std::allocator<std::byte>;

	constexpr std::chrono::microseconds sample_duration_us = std::chrono::duration_cast<std::chrono::microseconds>(sample_duration_t(1));
}

#endif
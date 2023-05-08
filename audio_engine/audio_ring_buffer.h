#ifndef AUDIO_RING_BUFFER_H
#define AUDIO_RING_BUFFER_H

#include "audio_types.h"
#include <memory>
#include <emmintrin.h>
#include <immintrin.h>
#include <stdexcept>
#include <concepts>

namespace audio_engine {
	static constexpr uint8_t sample_block_state_error = 0xFD;
	static constexpr uint8_t sample_block_state_processing = 0xFE;
	static constexpr uint8_t sample_block_state_processed = 0xFF;
	static constexpr uint8_t sample_block_state_default = 0x0;

	/// <summary>
	/// storage for buffer data
	/// </summary>
	struct audio_ring_buffer_storage
	{
		using byte_allocator_t = typename std::allocator_traits<ring_buffer_allocator_t>::template rebind_alloc<std::byte>;
		

		__m128i* m_sample_states; //16 bytes per m128 (1 byte per sample_block)
		//there shouldn't be any padding here because __m128i is 16byte aligned
		sample_block* m_sample_blocks;
		//array of 32bit (4 byte), may have padding afterwards but I forced a modulo16 block_count
		void* m_memory; //will use our default destructor which goes to the right allocator
		const size_t m_block_count;
		
		audio_ring_buffer_storage(size_t block_count) 
			: 
			m_block_count(block_count)
		{

			if (block_count == 0)
				throw std::domain_error("audio_ring_buffer_storage(block_count) : block_count must be greater than 0");
			
			if (block_count % 16 != 0) 
				throw std::domain_error("audio_ring_buffer_storage(block_count) : block_count must be a multiple of 16");

			
			uintptr_t data = reinterpret_cast<uintptr_t>(byte_allocator_t().allocate(15 + block_count + block_count * sizeof(sample_block)));
			m_memory = reinterpret_cast<void*>(data);
			
			
			m_sample_states = reinterpret_cast<__m128i*>(data - data % 16);
			m_sample_blocks = reinterpret_cast<sample_block*>(reinterpret_cast<uintptr_t>(m_memory) + block_count);
		}

		~audio_ring_buffer_storage() {
			if (m_memory != nullptr)
				byte_allocator_t().deallocate((std::byte*)m_memory, size());
		}

		audio_ring_buffer_storage(const audio_ring_buffer_storage&) = delete;
		audio_ring_buffer_storage& operator=(const audio_ring_buffer_storage&) = delete;

		audio_ring_buffer_storage(audio_ring_buffer_storage&& other) :
			m_memory(std::move(other.m_memory)),
			m_block_count(other.m_block_count)
		{
			if (m_block_count == 0)
				throw std::domain_error("audio_ring_buffer_storage(block_count) : block_count must be greater than 0");

			if (m_block_count % 16 != 0)
				throw std::domain_error("audio_ring_buffer_storage(block_count) : block_count must be a multiple of 16");

			other.m_memory = nullptr;
			m_sample_states = other.m_sample_states;
			m_sample_blocks = other.m_sample_blocks;
		}
		audio_ring_buffer_storage& operator=(audio_ring_buffer_storage&& other) {
			std::swap(m_memory, other.m_memory);
			return *this;
		}

		size_t size() {
			return m_block_count * (1 + sizeof(sample_block));
		}
	};

	inline int clamp(int x, int min, int max) {
		return std::max(min, std::min(max, x));
	}

	inline int byte_index(__m128i array, __m128i target) {
		__m128i cmp = _mm_cmpeq_epi8(std::move(array), std::move(target));
		int mask = _mm_movemask_epi8(cmp);
		int res = _tzcnt_u32(mask); //little endian so I subtract 32
		return res;
	}

	inline int byte_index_inverse(__m128i array, __m128i target) {
		__m128i cmp = _mm_cmpeq_epi8(std::move(array), std::move(target));
		int mask = _mm_movemask_epi8(cmp);
		int res = _tzcnt_u32(~mask); //little endian so I subtract 32
		return res;
	}

	/// <summary>
	/// A ring buffer implementation for storing audio samples in contiguous array of sample_blocks (float32[480]) 
	/// </summary>
	/// <typeparam name="block_count"></typeparam>
	class audio_ring_buffer {
	public:
		size_t const m_block_count;

	private:
		audio_ring_buffer_storage m_storage;

		const audio_ring_buffer_storage& get_storage() const {
			return m_storage;
		};

	public:
		audio_ring_buffer(size_t block_count = std::chrono::duration_cast<sample_duration_t>(std::chrono::seconds(1)).count() / sample_block_size)
			: m_storage(block_count),
			m_block_count(block_count)
		{
			clear();
		}

		//disable easy copying because I think you should have to be very explicit in wanting to expensively copy the buffer
		audio_ring_buffer(const audio_ring_buffer& other) = delete;
		audio_ring_buffer& operator=(const audio_ring_buffer& other) = delete;

		audio_ring_buffer(audio_ring_buffer&& other) noexcept :
			m_storage(std::move(other.m_storage)),
			m_block_count(other.m_block_count)
		{

		};
		audio_ring_buffer& operator=(audio_ring_buffer&& other) noexcept {
			std::swap(m_storage, other.m_storage);
			return *this;
		};
		
		sample_state* get_block_states() const {
			return reinterpret_cast<sample_state*>(m_storage.m_sample_states);
		};
		sample_block* get_blocks() const {
			return m_storage.m_sample_blocks;
		};

		sample_state& get_block_state(int idx) {
			return get_block_states()[idx % m_block_count];
		}
		sample_block& get_block(int idx) {
			return m_storage.m_sample_blocks[idx % m_block_count];
		}

		/// <summary>
		/// finds the index of the firstt sample_block matching the state
		/// </summary>
		/// <param name="state">the sample_state to search for</param>
		/// <returns>a sample_block index if found, or -1</returns>
		int get_first_match_idx(sample_state state) const {
			__m128i block = _mm_set1_epi8(state);
			sample_state* states = get_block_states();
			//loop m128i to find matches 
			int idx = -1;
			for (int i = 0; i < m_block_count; i+=16) {
				__m128i& test_arr = *reinterpret_cast<__m128i*>(&states[i]);
				int local_idx = 0;
				if ((local_idx = byte_index(test_arr, block)) < 16) {
					idx = local_idx + i;
					break;
				}
			}

			return idx;
		};

		/// <summary>
		/// finds the index of the first sample_block NOT matching the state
		/// </summary>
		/// <param name="state">the sample_state to search for mismatches against</param>
		/// <returns>a sample_block index if found, or -1</returns>
		int get_first_nonmatch_idx(sample_state state) const {
			__m128i block = _mm_set1_epi8(state);
			sample_state* states = get_block_states();
			//loop m128i to find matches 
			int idx = -1;
			for (int i = 0; i < m_block_count; i += 16) {
				__m128i& test_arr = *reinterpret_cast<__m128i*>(&states[i]);
				int local_idx = 0;
				if ((local_idx = byte_index_inverse(test_arr, block)) < 16) {
					idx = local_idx + i;
					break;
				}
			}

			return idx;
		}

		void set_state(int block_idx, sample_state state) {
			get_block_states()[block_idx] = state;
		};

		void clear() {
			memset(m_storage.m_memory, 0, m_storage.size());
		};

		audio_ring_buffer copy() const {
			auto temporary = audio_ring_buffer(m_block_count);
			copy_to(temporary, 0);
		};
		
		void copy_to(audio_ring_buffer& dest, uint32_t samples_offset = 0) const {
			copy_slice_to(dest, 0, samples_offset, m_block_count * sample_block_size);
		};



		/// <summary>
		/// copies a slice of data to the destination buffer, handles wrapping data within the buffer bounds, rejects slices larger than the smallest of either buffer.
		/// 
		/// supports partial sample_block copying, but will round down the sample_blocks to the left, i.e first block in the slice would be the state byte preceding the slice pos in buffer,
		/// last block in slice would be the byte before the state byte preceding the final sample of the slice in the source buffer. 
		/// 
		/// This behavior is only desirable when copying e.g infinitely from a circular buffer through to another e.g delay effects, where you don't want a later stage in the pipeline to process
		/// a partial sample_block
		/// 
		/// Generally keep samples_range and samples_offset as multiples of sample_block size unless this behavior is explicitly needed
		/// </summary>
		/// <typeparam name="dest_block_count">count of sample_blocks in the destination buffer</typeparam>
		/// <param name="dest:			">the buffer to copy the data to</param>
		/// <param name="sample_idx_from:	">the sample index to start copying from</param>
		/// <param name="sample_idx_to:		">the sample index to copy to</param>
		/// <param name="samples_range:		">the range of samples to copy</param>
		void copy_slice_to(audio_ring_buffer& dest, uint32_t sample_idx_from, uint32_t sample_idx_to, uint32_t samples_range) const {
			auto block_count = m_block_count;
			auto dest_block_count = dest.m_block_count;
			auto from_buffer_size = block_count * sample_block_size; 
			auto to_buffer_size = dest_block_count * sample_block_size; 

			auto min_buffer_size = std::min(from_buffer_size, to_buffer_size);
			if (samples_range > min_buffer_size)
				throw std::domain_error("samples_range must not exceed the size of the smallest buffer (to, from)");

			audio_ring_buffer_storage intermediate_buffer(block_count); //this is a bit inefficient but makes this easier to maintain
			memset(intermediate_buffer.m_memory, 0, intermediate_buffer.size());

			sample* raw_from_samples = reinterpret_cast<sample*>(get_blocks());
			sample_state* raw_from_states = reinterpret_cast<sample_state*>(get_block_states());
			sample* raw_tmp_samples = reinterpret_cast<sample*>(intermediate_buffer.m_sample_blocks);
			sample_state* raw_tmp_states = reinterpret_cast<sample_state*>(intermediate_buffer.m_sample_states);
			sample* raw_dest_samples = reinterpret_cast<sample*>(dest.get_blocks());
			sample_state* raw_dest_states = reinterpret_cast<sample_state*>(dest.get_block_states());

			uint32_t wrapped_to = (sample_idx_to) % to_buffer_size;

			//memory space
			uintptr_t from_samples = reinterpret_cast<uintptr_t>(raw_from_samples) + sample_idx_from * sizeof(sample);
			uintptr_t from_state = reinterpret_cast<uintptr_t>(raw_from_states) + sample_idx_from / sample_block_size;
			uintptr_t tmp_samples = reinterpret_cast<uintptr_t>(raw_tmp_samples);
			uintptr_t tmp_state = reinterpret_cast<uintptr_t>(raw_tmp_states);
			uintptr_t to_samples = reinterpret_cast<uintptr_t>(raw_dest_samples) + wrapped_to * sizeof(sample);
			uintptr_t to_state = reinterpret_cast<uintptr_t>(raw_dest_states) + wrapped_to / sample_block_size;


			//bounds calculations (in sample size not byte size)
			uint32_t tmp_unwrap_size = ((samples_range-1) % (from_buffer_size - sample_idx_from)) +1;
			uint32_t tmp_wrap_size = samples_range - tmp_unwrap_size;
			uint32_t to_unwrap_size = ((samples_range-1) % (to_buffer_size - sample_idx_to)) + 1;
			uint32_t to_wrap_size = samples_range - to_unwrap_size;

			//memory space
			uintptr_t from_unwrap_state = from_state;
			uintptr_t from_wrap_state = from_unwrap_state - sample_idx_from / sample_block_size;
			

			//buffer(state) space
			//intentionally truncate the last block, this is to be consistent with keeping the 0th partial block (from) 
			uint32_t slice_block_count = samples_range / sample_block_size;
			uint32_t tmp_wrap_size_state = tmp_wrap_size / sample_block_size;
			uint32_t tmp_unwrap_size_state = slice_block_count - tmp_wrap_size_state;
			uint32_t to_wrap_size_state = to_wrap_size / sample_block_size;
			uint32_t to_unwrap_size_state = slice_block_count - to_wrap_size_state;




			//uint32_t to_wrap_size_state = to_wrap_size / sample_block_size;
			//uint32_t to_unwrap_size_state = dest_block_count / sample_block_size;

			//copy into the temp buffer

			memcpy(
				reinterpret_cast<void*>(tmp_samples),
				reinterpret_cast<void*>(from_samples + (from_buffer_size - tmp_unwrap_size - sample_idx_from) * sizeof(sample)),
				static_cast<size_t>(tmp_unwrap_size * sizeof(sample))
			);
			memcpy(
				reinterpret_cast<void*>(tmp_samples + tmp_unwrap_size * sizeof(sample)),
				reinterpret_cast<void*>(from_samples - sample_idx_from * sizeof(sample)),
				static_cast<size_t>(tmp_wrap_size * sizeof(sample))
			);
			memcpy(
				reinterpret_cast<void*>(tmp_state),
				reinterpret_cast<void*>(from_unwrap_state),
				static_cast<size_t>(tmp_unwrap_size_state)
			);
			memcpy(
				reinterpret_cast<void*>(tmp_state + tmp_unwrap_size_state),
				reinterpret_cast<void*>(from_wrap_state),
				static_cast<size_t>(tmp_wrap_size_state)
			);


			//copy from the temp buffer into the target buffer
			memcpy(
				reinterpret_cast<void*>(to_samples),
				reinterpret_cast<void*>(tmp_samples),
				static_cast<size_t>(to_unwrap_size) * sizeof(sample)
			);
			memcpy(
				reinterpret_cast<void*>(raw_dest_samples),
				reinterpret_cast<void*>(tmp_samples + to_unwrap_size * sizeof(sample)),
				static_cast<size_t>(to_wrap_size) * sizeof(sample)
			);
			memcpy(
				reinterpret_cast<void*>(to_state),
				reinterpret_cast<void*>(tmp_state),
				static_cast<size_t>(to_unwrap_size_state)
			);
			memcpy(
				reinterpret_cast<void*>(raw_dest_states),
				reinterpret_cast<void*>(tmp_state + to_unwrap_size_state),
				static_cast<size_t>(to_wrap_size_state)
			);
		};



		
		

		
	};
};
#endif

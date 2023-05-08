#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

#include "audio_types.h"
#include "audio_ring_buffer.h"

#include <vector>
#include <functional>

namespace audio_engine {
	enum pipeline_execution_state : uint8_t {
		STOPPED = 0,
		PAUSED = 1,
		EXECUTING = 2,
	};

	struct pipeline_state {
		std::atomic<uint64_t> generator_flush_count;
		std::atomic<uint64_t> processing_flush_count;
		std::atomic<uint64_t> output_flush_count; //doesn't even flush to anything but I guess its useful to know how many times the output has cycled in total?
		std::atomic<uint8_t> execution_state;

		pipeline_state(
			uint64_t gfc,
			uint64_t pfc,
			uint64_t ofc,
			uint8_t es
		) : generator_flush_count(gfc),
			processing_flush_count(pfc),
			output_flush_count(ofc),
			execution_state(es)
		{}
	};

	//TODO: replace inheritance RTTI with concept-enforced semantics RTTI
	class pipeline_stage {
	private:
		
	protected:
		friend class audio_pipeline;

		const uint8_t m_entry_block_state;
		const uint8_t m_thread_count;
		const uint8_t m_in_buffer_idx;
		const uint8_t m_out_buffer_idx;
		//offset from inbuffer to outbuffer used in effects like delay 
		const uint8_t m_offset; 
		std::atomic<bool> m_flushing;

	public:
		pipeline_stage(uint8_t entry_block_state, uint8_t thread_count = 1, uint8_t in_buffer_idx = 0, uint8_t out_buffer_idx = 0, uint8_t offset = 0);

		pipeline_stage(const pipeline_stage&) noexcept = default;
		pipeline_stage& operator=(const pipeline_stage&) noexcept = default;

		pipeline_stage(pipeline_stage&&) noexcept = default;
		pipeline_stage& operator=(pipeline_stage&) noexcept = default;

		__forceinline uint8_t get_entry_state() const noexcept;

		//returns the output state, only gets called on blocks matching the entry state
		virtual sample_state process_block(
			const pipeline_state& state, 
			const sample_block& in_block, 
			sample_block& out_block,
			int block_count
		) noexcept = 0;


		virtual void init(std::vector<audio_ring_buffer>& buffers) = 0;

		virtual void cleanup() noexcept = 0;
	};


	class audio_pipeline
	{
	private:
		pipeline_state m_state;
		std::vector<std::unique_ptr<pipeline_stage>> m_generator_stages;
		std::vector<std::unique_ptr<pipeline_stage>> m_processing_stages; //owns the stages, holds them by ptr to not slice the dynamic class data if we ever did a copy
		std::vector<std::unique_ptr<pipeline_stage>> m_output_stages;
		std::vector<audio_ring_buffer> m_generator_buffers;
		std::vector<audio_ring_buffer> m_processing_buffers;
		std::vector<audio_ring_buffer> m_output_buffers;
		std::vector<std::jthread> m_threads;

	public:
		//accept implicits e.g. initializer_list of unique_ptr<pipeline_stage>
		~audio_pipeline() {
			for (auto& stage : m_generator_stages)
				stage->cleanup();
			for (auto& stage : m_processing_stages)
				stage->cleanup();
			for (auto& stage : m_output_stages)
				stage->cleanup();
		}

		audio_pipeline(
			std::vector<std::unique_ptr<pipeline_stage>> generator_stages,
			std::vector<std::unique_ptr<pipeline_stage>> processing_stages,
			std::vector<std::unique_ptr<pipeline_stage>> output_stages,
			std::vector<audio_ring_buffer> generator_buffers,
			std::vector<audio_ring_buffer> processing_buffers,
			std::vector<audio_ring_buffer> output_buffers
		)
			:
			m_state(
				std::atomic<uint64_t>(0),
				std::atomic<uint64_t>(0),
				std::atomic<uint64_t>(0),
				std::atomic<uint8_t>(pipeline_execution_state::STOPPED)
			),
			m_generator_stages(std::move(generator_stages)),
			m_processing_stages(std::move(processing_stages)),
			m_output_stages(std::move(output_stages)),
			m_threads(),
			m_generator_buffers(std::move(generator_buffers)),
			m_processing_buffers(std::move(processing_buffers)),
			m_output_buffers(std::move(output_buffers))
		{
			if (m_output_stages.size() == 0)
				throw std::runtime_error("audio_pipeline::audio_pipeline(...) requires at least one output stage");
		}

		audio_pipeline(
			std::vector<std::unique_ptr<pipeline_stage>> generator_stages,
			std::vector<std::unique_ptr<pipeline_stage>> processing_stages,
			std::vector<std::unique_ptr<pipeline_stage>> output_stages
		)
			: audio_engine::audio_pipeline::audio_pipeline(
				std::move(generator_stages),
				std::move(processing_stages),
				std::move(output_stages),
				std::move(std::vector<audio_ring_buffer>(1)),
				std::move(std::vector<audio_ring_buffer>(1)),
				std::move(std::vector<audio_ring_buffer>(1))
			)
		{

		};
	
		uint8_t get_state() const {
			return m_state.execution_state;
		};

		void stop() {
			m_state.execution_state.store(pipeline_execution_state::STOPPED);


		};
		void pause() {
			m_state.execution_state.store(pipeline_execution_state::PAUSED);
		};
		
		void add_processing_stage(pipeline_stage& stage);
		void add_output_stage(pipeline_stage& stage);

		void stage_worker(
			std::reference_wrapper<std::unique_ptr<pipeline_stage>> rp_stage, 
			std::reference_wrapper<audio_ring_buffer> rfrom_buffer, 
			std::reference_wrapper<audio_ring_buffer> rto_buffer
		)
		{
			
			auto& p_stage = rp_stage.get();
			auto& from_buffer = rfrom_buffer.get();
			auto& to_buffer = rto_buffer.get();
			uint8_t state;


			//until the execution is halted
			while ( (state = get_state()) != pipeline_execution_state::STOPPED)
			{
				//only do work while the pipeline is executing (not paused or some other stalling state)
				//only do work while the stage is not flushing 
				//(all stages in a group (generators), (processors), (outputters) are set to flushing together when the group is flushing)
				if (state == pipeline_execution_state::EXECUTING && !p_stage->m_flushing.load())
				{
					
					auto idx = from_buffer.get_first_match_idx(p_stage->m_entry_block_state);
					auto flush_count = m_state.generator_flush_count.load();
					auto dst_idx = idx + p_stage->m_offset;

					/// <summary>
					/// If we can do an atomic CAS onto the block state (it's in the expected state, put into processing state) 
					/// then this thread for this stage is allowed to operate on the block
					/// 
					/// if there was a spurious failure and another thread didn't sneak in and claim the block, we retry
					/// </summary>
					auto expected_state = p_stage->m_entry_block_state; //copy it because we don't want it overwritten
					bool exchanged = false;
					do {
						exchanged = std::atomic_ref<uint8_t>(from_buffer.get_block_state(idx)).compare_exchange_weak(
							expected_state,
							sample_block_state_processing,
							std::memory_order_release,
							std::memory_order_relaxed
						);
					}
					while (expected_state == p_stage->m_entry_block_state && !exchanged && !p_stage->m_flushing.load());

					if (exchanged) {

						auto& from_block = from_buffer.get_block(idx);
						auto& to_block = to_buffer.get_block(dst_idx);

						uint8_t out_state = p_stage->process_block(
							m_state,
							from_block,
							to_block,
							flush_count * m_generator_buffers.back().m_block_count + (dst_idx) //block_count is set to the unwrapped destination block num
							//this is useful for temporal stages it has temporal continuity with buffer wrapping
						);

						//atomically store the output state into the blocks from, to 
						std::atomic_ref<uint8_t>(from_buffer.get_block_state(dst_idx)).store(out_state);
						std::atomic_ref<uint8_t>(to_buffer.get_block_state(dst_idx)).store(out_state);
					}
				}
			}
		};

		void run()
		{
			m_state.execution_state = pipeline_execution_state::EXECUTING;
			
			for (auto& stage : m_generator_stages) {
				stage->init(m_generator_buffers);

				for (int i = 0; i < stage->m_thread_count; i++)
				{
					auto& from_buffer = m_generator_buffers[stage->m_in_buffer_idx];
					auto& to_buffer = m_generator_buffers[stage->m_out_buffer_idx];
					auto b = std::bind(&audio_pipeline::stage_worker, this, std::ref(stage), std::ref(from_buffer), std::ref(to_buffer));
					m_threads.push_back(std::move(std::jthread(std::move(b))));
				}

			}

			for (auto& stage : m_processing_stages) {
				stage->init(m_processing_buffers);

				for (int i = 0; i < stage->m_thread_count; i++)
				{
					auto& from_buffer = m_processing_buffers[stage->m_in_buffer_idx];
					auto& to_buffer = m_processing_buffers[stage->m_out_buffer_idx];
					auto b = std::bind(&audio_pipeline::stage_worker, this, std::ref(stage), std::ref(from_buffer), std::ref(to_buffer));
					m_threads.push_back(std::move(std::jthread(std::move(b))));
				}
			}

			for (auto& stage : m_output_stages) {
				stage->init(m_output_buffers);

				for (int i = 0; i < stage->m_thread_count; i++)
				{
					auto& from_buffer = m_output_buffers[stage->m_in_buffer_idx];
					auto& to_buffer = m_output_buffers[stage->m_out_buffer_idx];
					auto b = std::bind(&audio_pipeline::stage_worker, this, std::ref(stage), std::ref(from_buffer), std::ref(to_buffer));
					m_threads.push_back(std::move(std::jthread(std::move(b))));
				}
			}

			while (m_state.execution_state != pipeline_execution_state::STOPPED) {
				
				//check flush on generator_buffers (all blocks are stalled out - already processed!)
				if (
					m_generator_buffers.back().get_first_nonmatch_idx(sample_block_state_processed) == -1 //if the generator buffer is processed 100%
					&& m_processing_buffers.front().get_first_nonmatch_idx(sample_block_state_default) == -1 //and the processing stage collection is already flushed
					&& m_processing_buffers.back().get_first_nonmatch_idx(sample_block_state_default) == -1
				)
				{

					for (auto& stage : m_generator_stages) 
						stage->m_flushing.store(true);
					for (auto& stage : m_processing_stages)
						stage->m_flushing.store(true);

					std::atomic_thread_fence(std::memory_order_acquire);

					m_state.generator_flush_count.fetch_add(1);
					m_generator_buffers.back().copy_to(m_processing_buffers.front());
					m_generator_buffers.front().clear();
					memset(m_processing_buffers.front().get_block_states(), m_processing_stages.front()->m_entry_block_state, m_processing_buffers.front().m_block_count);
					memset(m_generator_buffers.back().get_block_states(), sample_block_state_default, m_generator_buffers.back().m_block_count);

					std::atomic_thread_fence(std::memory_order_release);

					for (auto& stage : m_generator_stages) 
						stage->m_flushing.store(false);
					for (auto& stage : m_processing_stages)
						stage->m_flushing.store(false);
				}

				//check flush on processing_buffers (all blocks are stalled out - already processed!)
				if (
					m_processing_buffers.back().get_first_nonmatch_idx(sample_block_state_processed) == -1
					&& m_output_buffers.front().get_first_nonmatch_idx(sample_block_state_default) == -1 //and the processing stage collection is already flushed
					&& m_output_buffers.back().get_first_nonmatch_idx(sample_block_state_default) == -1
				)
				{

					for (auto& stage : m_processing_stages)
						stage->m_flushing.store(true);
					for (auto& stage : m_output_stages)
						stage->m_flushing.store(true);

					std::atomic_thread_fence(std::memory_order_acquire);

					m_state.processing_flush_count.fetch_add(1);
					m_processing_buffers.back().copy_to(m_output_buffers.front());
					m_processing_buffers.front().clear(); 
					memset(m_output_buffers.front().get_block_states(), m_output_stages.front()->m_entry_block_state, m_output_buffers.front().m_block_count);
					memset(m_processing_buffers.back().get_block_states(), sample_block_state_default, m_processing_buffers.back().m_block_count);

					std::atomic_thread_fence(std::memory_order_release);

					for (auto& stage : m_processing_stages)
						stage->m_flushing.store(false);
					for (auto& stage : m_output_stages)
						stage->m_flushing.store(false);
				}

				
			}//end while-executing loop

			m_threads.clear(); //will invoke the destructor of all of the threads for the stages, they are std::jthread so this will block until they rejoin
			
			//TODO: make some span/reference only collection that tracks all of the stages
			for (auto& stage : m_generator_stages)
				stage->cleanup();
			for (auto& stage : m_processing_stages)
				stage->cleanup();
			for (auto& stage : m_output_stages)
				stage->cleanup();
		};

		void run_async()
		{
			auto binding = std::bind(&audio_pipeline::run, this);
			std::thread t(binding);
			t.detach();
		};
		//void set_processing_stages
	};

};

#endif
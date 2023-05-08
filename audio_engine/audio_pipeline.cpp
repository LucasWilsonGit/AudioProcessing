#include "audio_pipeline.h"


audio_engine::pipeline_stage::pipeline_stage(
	uint8_t entry_block_state, 
	uint8_t thread_count, 
	uint8_t in_buffer_idx,
	uint8_t out_buffer_idx,
	uint8_t offset
) 
	: 
	m_entry_block_state(entry_block_state),
	m_thread_count(thread_count),
	m_in_buffer_idx(in_buffer_idx),
	m_out_buffer_idx(out_buffer_idx),
	m_offset(offset),
	m_flushing(false)
{
}

uint8_t audio_engine::pipeline_stage::get_entry_state() const noexcept
{
	return m_entry_block_state;
}




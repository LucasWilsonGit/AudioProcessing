#include "dumpPCM_stage.h"

char* dumpPCM_stage::get_out_buffer()
{
	static char buf[s_out_buf_size];
	return buf;
}

audio_engine::sample_state dumpPCM_stage::process_block(const audio_engine::pipeline_state& state, const audio_engine::sample_block& in_block, audio_engine::sample_block& out_block, int block_count) noexcept
{
	for (uint64_t i = 0; i < audio_engine::sample_block_size; i++) {
		if (!std::isnan(in_block[i]))
			m_file.write((const char*)&in_block[i], sizeof(audio_engine::sample));
	}

	return audio_engine::sample_block_state_default;
}

void dumpPCM_stage::init(std::vector<audio_engine::audio_ring_buffer>& buffers)
{
	m_file = std::ofstream(m_filename, std::ios::binary);
	m_file.rdbuf()->pubsetbuf(get_out_buffer(), s_out_buf_size);
}

void dumpPCM_stage::cleanup() noexcept
{
	m_file.flush();
	m_file.close();
}

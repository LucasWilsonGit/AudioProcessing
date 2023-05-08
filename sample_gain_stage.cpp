#include "sample_gain_stage.h"

audio_engine::sample_state sample_gain_stage::process_block(const audio_engine::pipeline_state& state, const audio_engine::sample_block& in_block, audio_engine::sample_block& out_block, int block_count)
{

    for (uint64_t i = 0; i < audio_engine::sample_block_size; i++)
    {
        uint64_t i_sample = audio_engine::sample_block_size * block_count + i;

        out_block[i] = in_block[i] * m_multiplier;
    }

    return 2;
};

void sample_gain_stage::init(std::vector<audio_engine::audio_ring_buffer>& buffers) {};

void sample_gain_stage::cleanup() {};

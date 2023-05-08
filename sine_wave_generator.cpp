#include "sine_wave_generator.h"
#include <numbers>

constexpr int cexpr_ceil(float in) {
    int truncated = (int)in;
    return (in - truncated >= 0.5) ? truncated + 1 : truncated;
}

audio_engine::sample_state sine_wave_generator::process_block(const audio_engine::pipeline_state& state, const audio_engine::sample_block& in_block, audio_engine::sample_block& out_block, int block_count) noexcept
{

    for (uint64_t i = 0; i < audio_engine::sample_block_size; i++)
    {
        uint64_t i_sample = audio_engine::sample_block_size * block_count + i;

        auto ceil = cexpr_ceil(audio_engine::sample_rate / m_freq);

        float time = (i_sample % ceil) / (float)audio_engine::sample_rate;
        //modulo to stay near the origin to avoid float imprecision

        out_block[i] = std::sinf(time * m_freq * 2 * std::numbers::pi);//custom_sine(time, m_freq, 1.f);
    }

    return audio_engine::sample_block_state_processed;
};

void sine_wave_generator::init(std::vector<audio_engine::audio_ring_buffer>& buffers) {};

void sine_wave_generator::cleanup() noexcept {};
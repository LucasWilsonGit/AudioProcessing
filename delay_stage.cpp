#include "delay_stage.h"
#include "audio_engine/audio_types.h"

#include <chrono>
#include <cmath>

delay_stage::delay_stage(std::chrono::milliseconds delay_ms)
    : audio_engine::pipeline_stage(2, 1, 0, 1, std::chrono::duration_cast<audio_engine::sample_duration_t>(delay_ms).count()),
    m_delay_ms(std::move(delay_ms))
{

}

audio_engine::sample_state delay_stage::process_block(const audio_engine::pipeline_state& state, const audio_engine::sample_block& in_block, audio_engine::sample_block& out_block, int block_count)
{
    memcpy(out_block, in_block, sizeof(audio_engine::sample_block));
    return audio_engine::sample_block_state_processed;
}

void delay_stage::init(std::vector<audio_engine::audio_ring_buffer>& buffers)
{
    auto samples_duration = std::chrono::duration_cast<audio_engine::sample_duration_t>(m_delay_ms).count();

    //write 0 (silence) into the delay quantity samples, NaN into the rest
    for (int i = 0; i < buffers.front().m_block_count; i++) {
        for (int j = 0; j < audio_engine::sample_block_size; j++) {
            buffers.front().get_block(i)[j] = std::nanf("1");
        }
    }
    memset(buffers.front().get_blocks(), 0, samples_duration * sizeof(audio_engine::sample));

    //mark the blocks as post delay state
    memset(buffers.front().get_block_states(), 2, buffers.front().m_block_count);
}

void delay_stage::cleanup()
{
}


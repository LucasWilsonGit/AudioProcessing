#ifndef DELAY_STAGE_H
#define DELAY_STAGE_H

#include "audio_engine/audio.h"

class delay_stage : public audio_engine::pipeline_stage
{
private:
	std::chrono::milliseconds m_delay_ms;
public:
	delay_stage(std::chrono::milliseconds delay_ms);

    audio_engine::sample_state process_block(
        const audio_engine::pipeline_state& state,
        const audio_engine::sample_block& in_block,
        audio_engine::sample_block& out_block,
        int block_count
    ) override;

    void init(std::vector<audio_engine::audio_ring_buffer>& buffers) override;
    void cleanup() override;
};

#endif
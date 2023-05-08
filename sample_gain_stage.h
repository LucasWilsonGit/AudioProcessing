#ifndef SAMPLE_GAIN_STAGE_H
#define SAMPLE_GAIN_STAGE_H

#include "audio_engine/audio.h"
class sample_gain_stage : public audio_engine::pipeline_stage
{
private:
    float m_multiplier;
public:
    sample_gain_stage(float multiplier) : 
        audio_engine::pipeline_stage(1), //take the processed output written from the generator by the pipeline flush
        m_multiplier(multiplier)
    {};

    audio_engine::sample_state process_block(
        const audio_engine::pipeline_state& state,
        const audio_engine::sample_block& in_block,
        audio_engine::sample_block& out_block,
        int block_count
    ) noexcept override;

    void init(std::vector<audio_engine::audio_ring_buffer>& buffers) override;
    void cleanup() noexcept override;
};

#endif



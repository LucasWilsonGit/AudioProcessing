
#include "audio_engine/audio.h"

class sine_wave_generator : public audio_engine::pipeline_stage
{
private:
    float m_freq;
public:
	sine_wave_generator(float freq) : 
        audio_engine::pipeline_stage(audio_engine::sample_block_state_default),
        m_freq(freq)
	{};

    audio_engine::sample_state process_block(
        const audio_engine::pipeline_state& state,
        const audio_engine::sample_block& in_block,
        audio_engine::sample_block& out_block,
        int block_count
    ) override;

    void init(std::vector<audio_engine::audio_ring_buffer>& buffers) override;

    void cleanup() override;
};


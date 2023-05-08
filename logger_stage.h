#ifndef LOGGER_STAGE_H
#define LOGGER_STAGE_H

#include "audio_engine/audio.h"
#include <mutex>

class logger_stage : public audio_engine::pipeline_stage
{
private:
    static char* get_out_buffer();

public:
    //only 1 thread I didn't make this threadsafe around logging, generally file operations aren't threadsafe - but buffered ones are, writing something to switch between
    //atomic and locking cout buffer read/writes depending on buffer state was overkill for the prompt
	logger_stage() : audio_engine::pipeline_stage(3, 1)
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


#endif


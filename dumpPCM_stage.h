#ifndef DUMPPCM_STAGE_H
#define DUMPPCM_STAGE_H

#include "audio_engine/audio.h"
#include <fstream>

class dumpPCM_stage : public audio_engine::pipeline_stage
{
private:
    static constexpr size_t s_out_buf_size = 32;
    std::string m_filename;
    std::ofstream m_file;
    static char* get_out_buffer();

public:
    dumpPCM_stage(std::string filename = "dumpPCM_default")
        : pipeline_stage(3, 1),
        m_filename(std::move(filename))
    {}

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
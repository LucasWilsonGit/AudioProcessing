#include "logger_stage.h"
#include <iostream>
#include <mutex>
#include <memory>

char* logger_stage::get_out_buffer() {
    static char buf[1024]{ 0 }; //threadsafe since C++11 will zero initialize a buffer in some memory that is bound to app lifetime without crash at exit (global static destructor antipattern)

    return buf;
}

audio_engine::sample_state logger_stage::process_block(const audio_engine::pipeline_state& state, const audio_engine::sample_block& in_block, audio_engine::sample_block& out_block, int block_count) noexcept
{
    for (int i = 0; i < audio_engine::sample_block_size; i++)
    {
        if (!std::isnan(in_block[i]))
            std::cout << in_block[i] << "\n";
    }

    return audio_engine::sample_block_state_default;
}

void logger_stage::init(std::vector<audio_engine::audio_ring_buffer>& buffers)
{
    //std::cout.rdbuf()->pubsetbuf(get_out_buffer(), 8);
}

void logger_stage::cleanup() noexcept
{
    std::cout.flush();
}

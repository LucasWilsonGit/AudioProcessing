#include "audio_engine/audio.h"
#include "sine_wave_generator.h"
#include "sample_gain_stage.h"
#include "delay_stage.h"
#include "logger_stage.h"
#include "dumpPCM_stage.h"

int main()
{
    std::vector<std::unique_ptr<audio_engine::pipeline_stage>> generator_stages;
    generator_stages.emplace_back();

    //when working with multiple buffers on a stage, the from state should be left in the sample_block_state_processed state.
    //this will cause a serial stall on the pipeline for that block for that buffer which will propagate up avoid data being generated for a block
    //that wouldn't be able to accept it or worse would miss it and it would be overwritten
    //it basically makes the pipeline respect it's bottlenecks and not waste cycles regenerating temporal sample blocks 

    //we have to use make_vector because the initializer_list constructor will try to copy which is a deleted operation on my RAII objects which
    //manage memory
    audio_engine::audio_pipeline pipeline(
        audio_engine::make_vector(
            std::unique_ptr<audio_engine::pipeline_stage>(new sine_wave_generator(1000.f))
        ), //GENERATOR_STAGES
        audio_engine::make_vector(
            std::unique_ptr<audio_engine::pipeline_stage>(new sample_gain_stage(2.f)),
            std::unique_ptr<audio_engine::pipeline_stage>(new delay_stage(std::chrono::milliseconds(100)))
        ), //PROCESSING STAGES
        audio_engine::make_vector(
            std::unique_ptr<audio_engine::pipeline_stage>(new logger_stage())
            //std::unique_ptr<audio_engine::pipeline_stage>(new dumpPCM_stage("PCM_dump"))
        ), //OUTPUT STAGES
        audio_engine::make_vector(
            audio_engine::audio_ring_buffer(96) 
        ),
        audio_engine::make_vector( 
            audio_engine::audio_ring_buffer(96), 
            audio_engine::audio_ring_buffer(96) 
        ), //2 buffers, another buffer is needed for the delay
        audio_engine::make_vector(
            audio_engine::audio_ring_buffer(96) 
        )
    );
    pipeline.run();
}


/**
 * @file main.cpp
 * @brief A simple audio orchestrator, spins up the orchestrator and starts the DSP execution thread.
 * @author Case Zumbrum
 */

#include <iostream>
#include <packet_structure.h>
#include "io_system/io_controller.h"
#include "audio_orchestrator.h"
#include "effects/echo_effect.h"

#include <thread>
#include <chrono>

void passthrough(orchestrator_buffer_t* input, orchestrator_buffer_t* output){
    std::copy(input->begin(), input->end(), output->begin());
}



int main(){
    Audio_Orchestrator a("/tmp/gs_demo/capture.pcm", "/tmp/gs_demo/playback.pcm");
    EchoEffect echo(
        44100,  // Sample rate
        300.0f, // Delay (ms)
        0.75f,   // Decay
        0.5f,   // Wet
        1.0f    // Dry
    );

    unsigned int echo_index = a.register_transform(&echo);

    a.set_active_transform(echo_index);

    a.start();
    std::this_thread::sleep_for(std::chrono::seconds(20));
    std::cout << "setting delay to 0" << std::endl;
    echo.tweak(0, 0); 

    std::this_thread::sleep_for(std::chrono::seconds(20));
    std::cout << "setting delay to 0.4s" << std::endl;
    echo.tweak(0, 400); 

    a.execution_thread.join();
    a.stop();
    return 0;
}
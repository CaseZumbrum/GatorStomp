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
#include "effects/passthrough_effect.h"

#include <functional>
#include <thread>
#include <chrono>

int main(){
    // todo take in pipe/socket as arguments

    Audio_Orchestrator a("/tmp/gs_demo/capture.pcm", "/tmp/gs_demo/playback.pcm");

    auto boundFunc = std::bind(&Audio_Orchestrator::handle_message, &a, std::placeholders::_1);
    IO_Controller server("/tmp/gs.sock", boundFunc);

    server.start();

    PassthroughEffect passthrough;

    unsigned int pass_index = a.register_transform(&passthrough);

    EchoEffect echo(
        44100,  // Sample rate
        300.0f, // Delay (ms)
        0.75f,   // Decay
        0.5f,   // Wet
        1.0f    // Dry
    );
    unsigned int echo_index = a.register_transform(&echo);

    a.set_active_transform(pass_index);

    a.start();
    a.execution_thread.join();
    a.stop();
    return 0;
}
/**
 * @file main.cpp
 * @brief A simple calculator example for testing the audio orchestrator I/O structure. Sets up and receives a single message.
 * @author Case Zumbrum
 */

#include <iostream>
#include <packet_structure.h>
#include "io_system/io_controller.h"
#include "audio_orchestrator.h"
#include <thread>
#include <chrono>

void passthrough(orchestrator_buffer_t* input, orchestrator_buffer_t* output){
    std::copy(input->begin(), input->end(), output->begin());
}

int main(){
    Audio_Orchestrator a("/tmp/audio_input", "/tmp/audio_output");
    a.set_active_transform(a.register_transform(passthrough));
    a.start();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    a.stop();
    return 0;
}
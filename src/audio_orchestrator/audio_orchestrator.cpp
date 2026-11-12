/**
 * @file audio_orchestrator.cpp
 * @brief Implementation of the audio_orchestrator class, which is the hub for all DSP in gatorstomp
 * @todo move tweaks into me!
 * @author Case Zumbrum
 */

#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdio>
#include <poll.h>
#include <signal.h>

#include "audio_io_fifo.h"
#include "audio_orchestrator.h"
#include "effects/effect.h"

Audio_Orchestrator::Audio_Orchestrator(std::string input_pipe_path, std::string output_pipe_path){
    this->input_pipe_path = input_pipe_path;
    this->output_pipe_path = output_pipe_path;

    // open the output fifo
    this->output_pipe_fd = audio_fifo_open(this->output_pipe_path.c_str(), 1, &this->stop_requested);
    if (this->output_pipe_fd < 0) {
        throw std::runtime_error("Failed opening FIFO for writing.");
    }

    // open the input fifo
    this->input_pipe_fd = audio_fifo_open(this->input_pipe_path.c_str(), 0, &this->stop_requested);
    if (this->input_pipe_fd < 0) {
        throw std::runtime_error("Failed opening FIFO for reading.");
    }
}

Audio_Orchestrator::~Audio_Orchestrator(){
    // stop the execution thread
    this->stop();

    // close file descriptors
    close(this->input_pipe_fd);
    close(this->output_pipe_fd);
}

unsigned int Audio_Orchestrator::register_transform(Effect* transform){
    this->transforms.push_back(transform);
    return this->transforms.size() - 1;
}

void Audio_Orchestrator::set_active_transform(unsigned int index){
    this->active_transform = this->transforms.at(index);
}

bool Audio_Orchestrator::start(){
    // if the thread is already running, we can't start it
    if(this->execution_thread.joinable()){
        return false;
    }

    // make sure there is an active transform
    if(this->active_transform == nullptr){
        return false;
    }

    // we are about to start the thread, make sure it is setup to actually run
    this->keepRunning = true;

    // spin up the execution thread with it's entrypoint (acting on this instance)
    this->execution_thread = std::thread(&Audio_Orchestrator::execution_thread_runner, this);
    return true;
}

bool Audio_Orchestrator::stop(){
    // tell the thread to stop
    this->keepRunning = false;

    // join the thread (wait for it to finish)
    if (this->execution_thread.joinable()) {
        this->execution_thread.join();
    }

    return true;
} 

void Audio_Orchestrator::execution_thread_runner() {
    // until we are told to stop
    while (this->keepRunning) {
        // read in a buffer from the input fifo
        ssize_t bytesRead = audio_fifo_read_block(this->input_pipe_fd, this->input_buffer.data(), this->input_buffer.size(), &this->stop_requested);

        // success
        if (bytesRead > 0) {

            // apply the active transform
            this->active_transform->process(
                &this->input_buffer,
                &this->output_buffer
            );

            // write hte output buffer to the output fifo
            ssize_t bytesWritten = audio_fifo_write_all(this->output_pipe_fd, this->output_buffer.data(), bytesRead);

            // failure
            if (bytesWritten == -1) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("FIFO write failed");
                    break;
                }
            }
        }
        // fail
        else if (bytesRead == -1) {
            std::cout << errno << " ERROR" << std::endl;
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                perror("FIFO read failed");
                break;
            }
        }
        // zero bytes read, the buffers are empty and something is up
        else{
            std::cout << "0 bytes read, stopping..." << std::endl;
            break;
        }
    }
}
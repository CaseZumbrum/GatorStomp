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

Audio_Orchestrator::Audio_Orchestrator(std::string input_pipe_path, std::string output_pipe_path){
    this->input_pipe_path = input_pipe_path;
    this->output_pipe_path = output_pipe_path;

    this->output_pipe_fd = audio_fifo_open(this->output_pipe_path.c_str(), 1, &this->stop_requested);
    if (this->output_pipe_fd < 0) {
        throw std::runtime_error("Failed opening FIFO for writing.");
    }

    this->input_pipe_fd = audio_fifo_open(this->input_pipe_path.c_str(), 0, &this->stop_requested);
    if (this->input_pipe_fd < 0) {
        throw std::runtime_error("Failed opening FIFO for reading.");
    }
}

Audio_Orchestrator::~Audio_Orchestrator(){

    if (this->execution_thread.joinable()) {
        this->keepRunning = false;
        this->execution_thread.join();
    }

    close(this->input_pipe_fd);
    close(this->output_pipe_fd);
}

unsigned int Audio_Orchestrator::register_transform(transform_function_t transform){
    this->transforms.push_back(transform);
    return this->transforms.size() - 1;
}

bool Audio_Orchestrator::set_active_transform(unsigned int index){
    this->active_transform = this->transforms.at(index);
    return true;
}

bool Audio_Orchestrator::start(){
    if(this->execution_thread.joinable()){
        return false;
    }

    if(this->active_transform == nullptr){
        return false;
    }

    this->keepRunning = true;

    this->execution_thread = std::thread(&Audio_Orchestrator::execution_thread_runner, this);
    return true;
}

bool Audio_Orchestrator::stop(){
    this->keepRunning = false;

    if (this->execution_thread.joinable()) {
        this->execution_thread.join();
    }

    return true;
} 

void Audio_Orchestrator::execution_thread_runner() {

    struct pollfd input_poll_fd;

    input_poll_fd.fd = this->input_pipe_fd;
    input_poll_fd.events = POLLIN;
    input_poll_fd.revents = 0;

    while (this->keepRunning) {

        // int poll_result = poll(&input_poll_fd, 1, 100);

        // if (poll_result == -1) {
        //     if (errno == EINTR) {
        //         continue;
        //     }

        //     perror("poll failed");
        //     break;
        // }

        // if (poll_result == 0) {
        //     continue;
        // }

        // if (input_poll_fd.revents & (POLLERR | POLLNVAL)) {
        //     std::cerr << "FIFO polling error\n";
        //     break;
        // }

        // if (input_poll_fd.revents & POLLIN) {

            ssize_t bytesRead = audio_fifo_read_block(this->input_pipe_fd, this->input_buffer.data(), this->input_buffer.size(), &this->stop_requested);

            if (bytesRead > 0) {

                this->active_transform(
                    &this->input_buffer,
                    &this->output_buffer
                );

                ssize_t bytesWritten = audio_fifo_write_all(this->output_pipe_fd, this->output_buffer.data(), bytesRead);

                if (bytesWritten == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("FIFO write failed");
                        break;
                    }
                }
            }
            else if (bytesRead == -1) {
                std::cout << errno << " ERROR" << std::endl;
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    perror("FIFO read failed");
                    break;
                }
            }
            else{
                std::cout << "0 bytes read, stopping..." << std::endl;
                break;
            }


    }
    close(this->input_pipe_fd);
    close(this->output_pipe_fd);
}
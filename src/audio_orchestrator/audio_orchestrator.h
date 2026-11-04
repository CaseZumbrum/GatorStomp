#pragma once

#include <string>
#include <array>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>

#define ORCHESTRATOR_BUFFER_SIZE 1024

typedef std::array<uint32_t,ORCHESTRATOR_BUFFER_SIZE> orchestrator_buffer_t;
typedef std::function<void(orchestrator_buffer_t*, orchestrator_buffer_t*)> transform_function_t;

class Audio_Orchestrator{

    private:
        std::string input_pipe_path;
        std::string output_pipe_path;
        int input_pipe_fd;
        int output_pipe_fd;

        orchestrator_buffer_t input_buffer;
        orchestrator_buffer_t output_buffer;

        std::vector<transform_function_t> transforms;

        transform_function_t active_transform;

        std::thread execution_thread;
        std::atomic<bool> keepRunning{true};
    
        void execution_thread_runner();

    public:
        Audio_Orchestrator(std::string input_pipe_path, std::string output_pipe_path);
        ~Audio_Orchestrator();

        unsigned int register_transform(transform_function_t transform);

        bool set_active_transform(unsigned int index);

        bool start();
        bool stop();

};
/**
 * @file audio_orchestrator.h
 * @brief Definition of the audio_orchestrator class, which is the hub for all DSP in gatorstomp
 * @todo move tweaks into me!
 * @author Case Zumbrum
 */

#pragma once

#include <string>
#include <array>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <signal.h>
#include "effects/effect.h"

/** @brief class for the Audio Orchestrator */
class Audio_Orchestrator{

    private:
        /** @brief path to the input pipe, will be opened during construction */
        std::string input_pipe_path;
        /** @brief path to the output pipe, will be opened during construction */
        std::string output_pipe_path;
        /** @brief fd for the input pipe, will be retrieved during construction */
        int input_pipe_fd;
        /** @brief fd for the output pipe, will be retrieved during construction */
        int output_pipe_fd;

        /** @brief audio buffer that is filled by the input fifo */
        orchestrator_buffer_t input_buffer;
        /** @brief audio buffer that is passed to the output fifo */
        orchestrator_buffer_t output_buffer;

        /** @brief vector of transformation functions */
        std::vector<Effect*> transforms;

        /** @brief the currently active transform */
        Effect* active_transform;

        /** @brief flag for the execution thread to keep running, set to false to stop the thread */
        std::atomic<bool> keepRunning{true};

        /** @brief signal to stop the fifo worker, should stay 0 during normal operaion*/
        volatile sig_atomic_t stop_requested = 0;
    
        /** @brief entrypoint for the execution thread */
        void execution_thread_runner();

    public:
        /** 
         * @brief constructor for Audio Orchestrator, opens pipe fds
         * @param input_pipe_path path on the filesystem to an input fifo, should be the capture path of an audio worker
         * @param output_pipe_path path on the filesystem to an output fifo, should be the playback path of an audio worker
         */
        Audio_Orchestrator(std::string input_pipe_path, std::string output_pipe_path);

        /** 
         * @brief destructor for Audio Orchestrator, closes pipe fds and kills any active threads
         */
        ~Audio_Orchestrator();

        /**
         * @brief add an effect to the orchestrators list
         * @return index of the effect in the vector
         */
        unsigned int register_transform(Effect* transform);

        /**
         * @brief sets the active index
         */
        void set_active_transform(unsigned int index);

        /** 
         * @brief start the execution thread
         * @return success/fail of the start
         */
        bool start();
        /**
         * @brief stop the execution thread
         * @return success/fail of the stop
         */
        bool stop();

        /** @brief the execution thread for the orchestrator, pointer value may change between after a stop/start */
        std::thread execution_thread;

};
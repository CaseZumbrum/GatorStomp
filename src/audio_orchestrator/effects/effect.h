#pragma once

#include <array>

#define ORCHESTRATOR_BUFFER_SIZE 512*6

/** @brief type for audio buffer arrays */
typedef std::array<uint8_t,ORCHESTRATOR_BUFFER_SIZE> orchestrator_buffer_t;

class Effect{
    public:
        virtual void process(orchestrator_buffer_t* input, orchestrator_buffer_t* output) = 0;
        virtual void tweak(uint8_t index, float value) = 0;
};

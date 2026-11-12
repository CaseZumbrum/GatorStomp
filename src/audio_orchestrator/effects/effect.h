#pragma once

#include "../audio_orchestrator.h"

class Effect{
    public:
        virtual void process(orchestrator_buffer_t* input, orchestrator_buffer_t* output) = 0;
        virtual void tweak(uint8_t index, float value) = 0;
};

/**
 * @file passthrough_effect.h
 * @brief A simple passthrough effect
 * @author Case Zumbrum
 */

#pragma once

#include <cstdint>
#include "effect.h"

/** @brief Class for the passthrough effect */
class PassthroughEffect : public Effect{
public:
    /**
     * @brief Constructor
     */
    PassthroughEffect(){}

    /** 
     * @brief function that processes an audio buffer, just a wrapper on std::copy for passthrough
     * @param input pointer to the input buffer
     * @param output pointer to the output buffer
     */
    void process(
        orchestrator_buffer_t* input,
        orchestrator_buffer_t* output
    ) override {
        std::copy(input->begin(), input->end(), output->begin());
    }

    /**
     * @brief function used by the orchestrator for tweaking the effect, does nothing for passthrough
     * @param index unused
     * @param value unused
     */
    void tweak(
        uint8_t index,
        int8_t value
    ) override {}
};

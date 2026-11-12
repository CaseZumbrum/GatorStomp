/**
 * @file echo_effect.h
 * @brief A simple echo effect, tweakable
 * @author Case Zumbrum
 */

#pragma once

#include <cstdint>
#include <vector>
#include <cstddef>

#include "effect.h"

/** @brief Class for the echo effect */
class EchoEffect : public Effect{
public:
    /**
     * @brief Constructor, initializes buffers and attributes
     * @param sample_rate sample rate for the signal, needed to accurately perform the echo delay (in Hz)
     * @param delay_ms amount of time for a single delay pulse
     * @param decay how much the echo should be scaled by
     * @param wet how much the output should include delay
     * @param dry how much the output should include the original
     */
    EchoEffect(
        unsigned int sample_rate = 44100,
        float delay_ms = 300.0f,
        float decay = 0.5f,
        float wet = 0.5f,
        float dry = 1.0f
    );

    /** @brief reset the delay buffer */
    void reset();

    /** 
     * @brief setter for delay
     * @param delay_ms delay in ms
     */
    void set_delay(float delay_ms);
    /** 
     * @brief setter for decay
     * @param decay decay
     */
    void set_decay(float decay);
    /** 
     * @brief setter for wet value
     * @param wet wet value 
     */
    void set_wet(float wet);
    /** 
     * @brief setter for dry value
     * @param dry dry value 
     */
    void set_dry(float dry);

    /** 
     * @brief function that processes an audio buffer
     * @param input pointer to the input buffer
     * @param output pointer to the output buffer
     */
    void process(
        orchestrator_buffer_t* input,
        orchestrator_buffer_t* output
    ) override;

    /**
     * @brief function used by the orchestrator for tweaking the effect
     * @param index 0: delay_ms, 1: decay rate, 2: wet, 3: dry
     * @param value value for the tweak
     */
    void tweak(
        uint8_t index,
        float value
    ) override;

private:
    /** @brief number of channels for the audio */
    static constexpr size_t CHANNELS = 2;
    /** @brief number of bytes in a single sample */
    static constexpr size_t BYTES_PER_SAMPLE = 3;
    /** @brief number of bytes in a single frame */
    static constexpr size_t FRAME_BYTES =
        CHANNELS * BYTES_PER_SAMPLE;

    /** @brief sampling rate for the system */
    unsigned int sample_rate;

    /** @brief number of samples to delay */
    size_t delay_samples;
    /** @brief where we are in the delay buffer */
    size_t write_position;

    /** @brief how strong the delay is */
    float decay;
    /** @brief how much of the output should be a delayed signal */
    float wet;
    /** @brief how much of the output should be the original signal */
    float dry;

    /** @brief delay buffer, one for each channel */
    std::vector<float> delay_buffer[CHANNELS];

    /** 
     * @brief converts a pcm 3 byte sample into an int 
     * @param data input pcm sample
     * @return decoded sample
     */
    int32_t decode_sample(const uint8_t* data) const;

    /**
     * @brief encode a sample to a 3 byte pcm sample
     * @param sample the sample to encode
     * @param data the buffer to fill
     */
    void encode_sample(
        int32_t sample,
        uint8_t* data
    ) const;
};

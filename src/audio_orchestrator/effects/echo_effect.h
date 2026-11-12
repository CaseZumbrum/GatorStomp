
#pragma once

#include <cstdint>
#include <vector>
#include <cstddef>

#include "effect.h"

class EchoEffect : public Effect{
public:
    EchoEffect(
        unsigned int sample_rate = 44100,
        float delay_ms = 300.0f,
        float decay = 0.5f,
        float wet = 0.5f,
        float dry = 1.0f
    );

    void reset();

    void set_delay(float delay_ms);
    void set_decay(float decay);
    void set_wet(float wet);
    void set_dry(float dry);

    void process(
        orchestrator_buffer_t* input,
        orchestrator_buffer_t* output
    ) override;

    void tweak(
        uint8_t index,
        int8_t value
    ) override;

private:
    static constexpr size_t CHANNELS = 2;
    static constexpr size_t BYTES_PER_SAMPLE = 3;
    static constexpr size_t FRAME_BYTES =
        CHANNELS * BYTES_PER_SAMPLE;

    unsigned int sample_rate;

    size_t delay_samples;
    size_t write_position;

    float decay;
    float wet;
    float dry;

    // Independent delay history for left and right channels.
    std::vector<float> delay_buffer[CHANNELS];

    int32_t decode_sample(const uint8_t* data) const;

    void encode_sample(
        int32_t sample,
        uint8_t* data
    ) const;

    void update_delay(float delay_ms);
};

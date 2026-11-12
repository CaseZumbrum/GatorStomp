
#include "echo_effect.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

EchoEffect::EchoEffect(
    unsigned int sample_rate,
    float delay_ms,
    float decay,
    float wet,
    float dry
)
    : sample_rate(sample_rate),
      delay_samples(0),
      write_position(0),
      decay(std::clamp(decay, 0.0f, 1.0f)),
      wet(std::clamp(wet, 0.0f, 1.0f)),
      dry(std::clamp(dry, 0.0f, 1.0f))
{
    if (sample_rate == 0)
        throw std::invalid_argument("Invalid sample rate");

    update_delay(delay_ms);
}

int32_t EchoEffect::decode_sample(const uint8_t* data) const
{
    int32_t sample =
        static_cast<int32_t>(data[0]) |
        (static_cast<int32_t>(data[1]) << 8) |
        (static_cast<int32_t>(data[2]) << 16);

    // Sign extend 24-bit PCM.
    if (sample & 0x800000)
        sample |= static_cast<int32_t>(0xFF000000);

    return sample;
}

void EchoEffect::encode_sample(
    int32_t sample,
    uint8_t* data
) const
{
    sample = std::clamp(
        sample,
        -8388608,
        8388607
    );

    uint32_t value = static_cast<uint32_t>(sample);

    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
}

void EchoEffect::update_delay(float delay_ms)
{
    delay_ms = std::max(delay_ms, 1.0f);

    delay_samples = std::max(
        static_cast<size_t>(
            sample_rate * delay_ms / 1000.0f
        ),
        static_cast<size_t>(1)
    );

    for (auto& channel : delay_buffer)
        channel.assign(delay_samples, 0.0f);

    write_position = 0;
}

void EchoEffect::process (
    orchestrator_buffer_t* input,
    orchestrator_buffer_t* output
)
{
    if (!input || !output)
        return;

    constexpr size_t TOTAL_FRAMES =
        ORCHESTRATOR_BUFFER_SIZE / FRAME_BYTES;

    for (size_t i = 0; i < TOTAL_FRAMES; ++i) {

        for (size_t channel = 0; channel < CHANNELS; ++channel) {

            size_t offset =
                i * FRAME_BYTES +
                channel * BYTES_PER_SAMPLE;

            int32_t original =
                decode_sample(&(*input)[offset]);

            float delayed =
                delay_buffer[channel][write_position];

            // Mix original audio and delayed audio.
            float result =
                dry * static_cast<float>(original) +
                wet * delayed;

            encode_sample(
                static_cast<int32_t>(std::round(result)),
                &(*output)[offset]
            );

            // Store current input plus feedback.
            delay_buffer[channel][write_position] =
                static_cast<float>(original) +
                decay * delayed;
        }

        write_position++;

        if (write_position >= delay_samples)
            write_position = 0;
    }
}

void EchoEffect::reset()
{
    for (auto& channel : delay_buffer)
        std::fill(
            channel.begin(),
            channel.end(),
            0.0f
        );

    write_position = 0;
}

void EchoEffect::tweak(uint8_t index, int8_t value) {
    this->set_delay(static_cast<float>(value)/64 * 1000);
}

void EchoEffect::set_delay(float delay_ms)
{
    update_delay(delay_ms);
}

void EchoEffect::set_decay(float decay)
{
    this->decay = std::clamp(decay, 0.0f, 1.0f);
}

void EchoEffect::set_wet(float wet)
{
    this->wet = std::clamp(wet, 0.0f, 1.0f);
}

void EchoEffect::set_dry(float dry)
{
    this->dry = std::clamp(dry, 0.0f, 1.0f);
}

/**
 * @file audio_io.h
 * @brief Blocking ALSA and raw-file audio pipeline interface.
 * @details Samples are signed packed 24-bit little-endian interleaved PCM
 * (S24_3LE), three bytes per channel. The tested configuration is 96000 Hz,
 * stereo (six bytes per frame). Raw files have no headers or format metadata.
 */
#ifndef AUDIO_IO_H
#define AUDIO_IO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration borrowed for the duration of audio_io_run().
 * @details Set unused paths/device names to NULL. Input files take precedence
 * over capture devices; playback and output-file recording may both be enabled.
 * Paths, device strings, and this configuration must remain valid during the run.
 */
typedef struct {
    /** @brief Requested frames per second; must be nonzero. Tested at 96000. */
    unsigned int sample_rate;
    /** @brief Interleaved channel count; use 2 for stereo. Must be nonzero. */
    unsigned int channels;
    /** @brief Maximum frames passed to the callback per block; must be nonzero. */
    unsigned int frames_per_buffer;
    /** @brief Raw PCM input path, or NULL to use capture_device. */
    const char *input_file;
    /** @brief ALSA capture device name, required when input_file is NULL. */
    const char *capture_device;
    /** @brief Optional ALSA playback device name; NULL disables playback. */
    const char *playback_device;
    /** @brief Optional raw PCM output path, truncated on open. */
    const char *output_file;
} AudioIOConfig;

/**
 * @brief DSP callback that modifies a PCM block in place, synchronously.
 * @param buffer Borrowed writable PCM bytes; valid only during the callback.
 * Contains frames * channels * 3 bytes, not floating-point samples.
 * @param frames Number of complete frames, possibly less than frames_per_buffer.
 * @param channels Number of interleaved channels in each frame.
 * @param user_data Opaque caller data passed unchanged by audio_io_run().
 * @return No value; this callback cannot report a processing failure.
 * @note Do not resize/free/retain buffer. Preserve the frame count and format.
 * Blocking DSP work delays capture/playback and may cause overruns/underruns.
 */
typedef void (*AudioProcessCallback)(
    uint8_t *buffer,
    size_t frames,
    unsigned int channels,
    void *user_data
);

/**
 * @brief Run input, optional DSP processing, recording, and playback.
 * @param config Borrowed configuration with nonzero rate, channels, and block size.
 * At least one input and one output must be configured. Must not be NULL.
 * @param process Optional synchronous DSP callback; NULL passes PCM unchanged.
 * @param user_data Borrowed callback context; may be NULL.
 * @return 0 after successful input EOF and playback drain; -1 on invalid
 * configuration, allocation, file I/O, or unrecoverable ALSA failure.
 * @details Uses blocking ALSA I/O, S24_3LE interleaved access, disabled software
 * resampling, and a requested 100 ms device latency. Partial ALSA transfers are
 * handled and recoverable ALSA errors are retried. File EOF must fall on a
 * complete PCM frame. Diagnostics are printed to stderr and completion to stdout.
 * @warning Live capture runs indefinitely: there is no cancellation API, pipe
 * transport, or graceful signal shutdown. Recovery may lose audio; this API does
 * not promise real-time scheduling, hardware support, or ADC/DAC verification.
 * Outputs may be partial after failure. Input and output paths must refer to
 * different files; opening the output truncates any existing contents.
 */
int audio_io_run(const AudioIOConfig *config, AudioProcessCallback process, void *user_data);

#ifdef __cplusplus
}
#endif

#endif

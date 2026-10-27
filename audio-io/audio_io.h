
#ifndef AUDIO_IO_H
#define AUDIO_IO_H

#include <stddef.h>
#include <stdint.h>

// Configuration for the GatorStomp audio interface
typedef struct {
    unsigned int sample_rate; //Number of audio frames per second.
    unsigned int channels; //Number of audio channels
    unsigned int frames_per_buffer; //How many audio frames the program processes at a time

    const char *input_file; //Path to a raw PCM input file
    const char *capture_device; //ALSA device name for actual capture
    const char *playback_device; //ALSA device name for output
    const char *output_file; //Optional file where processed audio is saved
} AudioIOConfig;

// Function supplied by the DSP team
typedef void (*AudioProcessCallback)( //
    uint8_t *buffer, //Points to the audio samples, stored as bytes. FOR CASE: buffer contains packed 24-bit PCM bytes, not an array of floating-point samples
    size_t frames, //Tells DSP how many audio frames are in the buffer
    unsigned int channels, //Tells DSP the number of channels
    void *user_data //Allows the calling program to pass additional information, such as effect settings or processing statistics.
);

// Run the capture -> DSP -> playback pipeline
// Returns 0 on success, negative on failure
int audio_io_run(const AudioIOConfig *config,AudioProcessCallback process,void *user_data); //Passes additional data through to the DSP callback

#endif

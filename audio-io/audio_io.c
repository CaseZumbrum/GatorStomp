
#include "audio_io.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define BYTES_PER_SAMPLE 3 

// Open and configure an ALSA PCM device
static int open_pcm(
    snd_pcm_t **handle, //Receives a pointer to an ALSA handle pointer, allowing the function to return the opened device through that pointer.
    const char *device, //Receives the ALSA device name, such as "null".
    snd_pcm_stream_t stream, //Specifies whether the device is for capture or playback.
    const AudioIOConfig *config
){
    int err=snd_pcm_open(handle, device, stream, 0); 
    // handle receives the opened device.
    // device identifies which ALSA device to use.
    // stream selects input or output.
    // 0 requests ordinary blocking mode

    if (err<0){
        fprintf(stderr, "ALSA open failed: %s\n",snd_strerror(err));
        return -1;
    }

    err=snd_pcm_set_params(*handle,SND_PCM_FORMAT_S24_3LE,SND_PCM_ACCESS_RW_INTERLEAVED,config->channels,config->sample_rate,0,100000);
    // *handle	The ALSA device that was just opened.
    // SND_PCM_FORMAT_S24_3LE	Signed 24-bit little-endian samples, packed into three bytes.
    // SND_PCM_ACCESS_RW_INTERLEAVED	Specifies that left/right channel samples are stored together in memory.
    // config->channels	Requests two channels in our current configuration.
    // config->sample_rate	Requests 96,000 frames per second.
    // 0	Disables ALSA's automatic software sample-rate resampling.
    // 100000	Requests approximately 100,000 microseconds (100 ms) of buffering latency.
    
    if (err<0) {
        fprintf(stderr, "ALSA configuration failed: %s\n",snd_strerror(err));
        snd_pcm_close(*handle);
        *handle = NULL;
        return -1;
    }
    return 0;
}

// Run ADC -> DSP -> DAC audio pipeline
int audio_io_run(
    const AudioIOConfig *config,
    AudioProcessCallback process,
    void *user_data
)
{
    if (!config || !config->sample_rate ||
        !config->channels || !config->frames_per_buffer) {
        fprintf(stderr, "Invalid audio configuration\n");
        return -1;
    }

    if ((!config->input_file && !config->capture_device) ||
        (!config->output_file && !config->playback_device)) {
        fprintf(stderr, "Missing audio input or output\n");
        return -1;
    }

    size_t frame_bytes = (size_t)config->channels *BYTES_PER_SAMPLE;

    if (frame_bytes / BYTES_PER_SAMPLE != config->channels || config->frames_per_buffer > SIZE_MAX / frame_bytes) {
        fprintf(stderr, "Audio buffer size is too large\n");
        return -1;
    }

    size_t buffer_bytes =(size_t)config->frames_per_buffer * frame_bytes;

    uint8_t *buffer = malloc(buffer_bytes);

    if (!buffer){
        fprintf(stderr, "Cannot allocate audio buffer\n");
        return -1;
    }

    FILE *input = NULL;
    FILE *output = NULL;

    snd_pcm_t *capture = NULL;
    snd_pcm_t *playback = NULL;

    unsigned long total_frames = 0;
    int status = -1;
    int err;

    // Step 1: Initialize ADC input
    if (config->input_file){
        input = fopen(config->input_file, "rb");

        if (!input) {
            perror("Opening input file");
            goto cleanup;
        }
    } 
    else{
        if (open_pcm(&capture, config->capture_device,SND_PCM_STREAM_CAPTURE, config) < 0)
            goto cleanup;
    }

    // Step 2: Initialize DAC output
    if (config->playback_device) {
        if (open_pcm(&playback, config->playback_device,SND_PCM_STREAM_PLAYBACK, config) < 0)
            goto cleanup;
    }

    if (config->output_file) {
        output = fopen(config->output_file, "wb");

        if (!output) {
            perror("Opening output file");
            goto cleanup;
        }
    }

    // Step 3: Continuous audio processing loop
    while (1) {
        size_t frames_read = 0;

        // Read from file or ALSA capture device
        if (input) {
            size_t bytes_read =fread(buffer, 1, buffer_bytes, input);

            if (bytes_read == 0) {
                if (ferror(input)) {
                    perror("Reading input");
                    goto cleanup;
                }

                break; // End of file
            }

            if (bytes_read % frame_bytes != 0) {
                fprintf(stderr, "Incomplete PCM frame\n");
                goto cleanup;
            }

            frames_read = bytes_read / frame_bytes;
        } else {
            snd_pcm_sframes_t result =
                snd_pcm_readi(capture, buffer,config->frames_per_buffer);

            if (result < 0) {
                err = snd_pcm_recover(capture, (int)result, 0);

                if (err < 0) {
                    fprintf(stderr, "Capture failed: %s\n",snd_strerror(err));
                    goto cleanup;
                }

                continue;
            }

            if (result == 0)
                continue;

            frames_read = (size_t)result;
        }

        // Step 4: Send buffer to DSP processing
        if (process) {
            process(buffer, frames_read,config->channels, user_data);
        }

        // Step 5: Save processed samples if requested
        if (output) {
            if (fwrite(buffer, frame_bytes, frames_read,output) != frames_read) {
                perror("Writing output");
                goto cleanup;
            }
        }

        // Step 6: Send samples to ALSA playback
        if (playback) {
            size_t written = 0;

            while (written < frames_read) {
                snd_pcm_sframes_t result = snd_pcm_writei(playback,buffer + written * frame_bytes,frames_read - written);

                if (result < 0) {
                    err = snd_pcm_recover(playback, (int)result, 0);

                    if (err < 0) {
                        fprintf(stderr, "Playback failed: %s\n",snd_strerror(err));
                        goto cleanup;
                    }

                    continue;
                }

                if (result == 0)
                    continue;

                written += (size_t)result;
            }
        }

        total_frames += frames_read;
    }

    if (playback) {
        err = snd_pcm_drain(playback);

        if (err < 0) {
            fprintf(stderr, "ALSA drain failed: %s\n",snd_strerror(err));
            goto cleanup;
        }
    }

    printf("GatorStomp audio processing completed!\n");
    printf("Frames processed: %lu\n", total_frames);

    status = 0;

cleanup:
    if (input)
        fclose(input);

    if (output)
        fclose(output);

    if (capture)
        snd_pcm_close(capture);

    if (playback)
        snd_pcm_close(playback);

    free(buffer);

    return status;
}

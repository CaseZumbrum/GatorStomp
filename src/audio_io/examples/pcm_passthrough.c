/** @file pcm_passthrough.c
 * @brief Minimal orchestrator/DSP stand-in: forwards complete PCM unchanged.
 */
#define _POSIX_C_SOURCE 200809L
#include "audio_io_fifo.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/** @brief Stop requests belong to this executable, not the reusable library. */
static volatile sig_atomic_t stop_requested;

static void handle_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}
/*Function below handles 3 types of signals
Signal	What it does
SIGINT	Requests a stop, such as when you press Ctrl+C
SIGTERM	Requests a stop, such as when systemd stops the process
SIGPIPE	Ignored so a broken pipe produces an error instead of immediately terminating the program*/
static int setup_signals(void)
{
    struct sigaction action = {0};

    sigemptyset(&action.sa_mask);
    action.sa_handler = handle_stop;
    if (sigaction(SIGINT, &action, NULL) < 0 || sigaction(SIGTERM, &action, NULL) < 0)
        return -1;
    
    action.sa_handler = SIG_IGN; /* A disconnected reader becomes EPIPE. */
    return sigaction(SIGPIPE, &action, NULL);
}

int main(int argc, char **argv) //build/bin/audio_io_passthrough capture.pcm playback.pcm 128
{
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: %s CAPTURE_FIFO PLAYBACK_FIFO [FRAMES]\n", argv[0]);
        return EXIT_FAILURE;
    }
    char *end = NULL;
    errno = 0;
    unsigned long frames = argc == 4 ? strtoul(argv[3], &end, 10) : 128;

    //verifies that the provided buffer size is a valid positive number within the supported limit
    if (errno || (end && (*end || end == argv[3] || argv[3][0] == '-')) ||
        !frames || frames > AUDIO_IO_MAX_BLOCK_FRAMES) {
        fprintf(stderr, "Invalid block size\n");
        return EXIT_FAILURE;
    }

    int capture = -1, playback = -1, error = 0;
    size_t total = 0;
    uint8_t *buffer = NULL;

    if (setup_signals() < 0){ 
        error = errno; 
        goto cleanup; 
    }

    //number of frames*(bytes per frame)
    buffer = malloc(frames * AUDIO_IO_FRAME_BYTES);

    if (!buffer){ 
        error = ENOMEM; 
        goto cleanup; 
    }
    /*The pass-through program is a reader of the capture FIFO.
    The pass-through program is a writer of the playback FIFO.*/
    /* Connect downstream first. The three programs may be started in any order. */
    playback = audio_fifo_open(argv[2], 1, &stop_requested);
    if (playback < 0){ 
        error = errno; 
        goto cleanup; 
    }

    //
    capture = audio_fifo_open(argv[1], 0, &stop_requested);
    if (capture < 0){ 
        error = errno; 
        goto cleanup; 
    }

    while(1){
        //reads data into buffer
        ssize_t bytes = audio_fifo_read_block(capture, buffer, frames * AUDIO_IO_FRAME_BYTES, &stop_requested);
        if (bytes < 0){ 
            error = errno; 
            break; 
        }
        if (!bytes)
            break;

        //writes data into the playback FIFO, just forwarding same buffer we got
        if (audio_fifo_write_all(playback, buffer, (size_t)bytes) < 0) {
            error = errno;
            break;
        }

        //increase number of frames transferred
        total += (size_t)bytes / AUDIO_IO_FRAME_BYTES;
    }

cleanup:
    //close both fifos and releases allocated buffer
    if (capture >= 0)
        close(capture);

    if (playback >= 0)
        close(playback); /* EOF tells the DAC to finish and drain. */

    free(buffer);
    
    if (error && !(error == ECANCELED && stop_requested)) {
        fprintf(stderr, "Pass-through: ");
        errno = error;
        perror("PCM transfer");
        return EXIT_FAILURE;
    }
    printf("Pass-through: %zu frames forwarded\n", total);
    return EXIT_SUCCESS;
}

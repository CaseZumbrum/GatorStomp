/** @file audio_io_worker.c
 * @brief Private capture/playback process executable, launched by the C API.
 * @details FD 3 is PCM (capture writes, playback reads), FD 4 sends an atomic
 * errno status at startup and completion, and capture FD 5 detects parent stop
 * by EOF. Workers never use the peripheral SOCK_SEQPACKET protocol.
 */
#define _POSIX_C_SOURCE 200809L
#include "audio_io_process.h"
#include "audio_io_process_internal.h"
#include <alsa/asoundlib.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { PCM_FD = 3, STATUS_FD = 4, STOP_FD = 5 };

/** @brief Stop at the next complete block when the parent closes its stop end. */
static int stop_requested(void)
{
    struct pollfd p = {STOP_FD, POLLIN, 0};
    int rc;
    do { rc = poll(&p, 1, 0); } while (rc < 0 && errno == EINTR);
    return rc != 0; /* Invalid/missing stop descriptor also stops capture safely. */
}

/** @brief Fill a byte block or return a short final block at EOF. */
static ssize_t read_block(int fd, uint8_t *buffer, size_t bytes)
{
    size_t used = 0;
    while (used < bytes) {
        ssize_t n = read(fd, buffer + used, bytes - used);
        if (n > 0) { used += (size_t)n; continue; }
        if (n == 0)
            break;
        if (errno == EINTR)
            continue;
        if (errno != EAGAIN || audio_pipe_wait(fd, POLLIN, -1) < 0)
            return -1;
    }
    if (used % AUDIO_IO_FRAME_BYTES) { errno = EPROTO; return -1; }
    return (ssize_t)used;
}

/** @brief Recover ALSA errors, translating failures to errno for the parent. */
static int recover_pcm(snd_pcm_t *pcm, int error)
{
    /* Avoid snd_pcm_recover's potentially unbounded suspend-resume retry loop.
     * Preparing after suspension can lose samples but remains cancellable. */
    int rc = error == -ESTRPIPE ? snd_pcm_prepare(pcm) :
                                snd_pcm_recover(pcm, error, 1);
    if (rc < 0) { errno = -rc; return -1; }
    return 0;
}

/** @brief Retry a nonblocking ALSA operation after a short, cancellable wait. */
static int wait_pcm(snd_pcm_t *pcm)
{
    int rc = snd_pcm_wait(pcm, 50);
    if (rc < 0)
        return recover_pcm(pcm, rc);
    return 0;
}

/** @brief Transfer capture blocks from ALSA or a raw fixture into the PCM pipe. */
static int capture_audio(snd_pcm_t *pcm, int file, uint8_t *buffer, size_t frames)
{
    while (!stop_requested()) {
        ssize_t bytes;
        if (pcm) {
            snd_pcm_sframes_t n = snd_pcm_readi(pcm, buffer, frames);
            if (n == -EAGAIN || n == 0) {
                if (wait_pcm(pcm) < 0)
                    return -1;
                continue;
            }
            if (n < 0) {
                if (recover_pcm(pcm, (int)n) < 0)
                    return -1;
                continue;
            }
            bytes = n * AUDIO_IO_FRAME_BYTES;
        } else {
            bytes = read_block(file, buffer, frames * AUDIO_IO_FRAME_BYTES);
            if (bytes < 0)
                return -1;
            if (bytes == 0)
                return 0;
        }
        /* Finish this entire block even if stop arrives during a short write.
         * The parent must keep draining; abort can kill a blocked worker. */
        if (audio_pipe_write_all(PCM_FD, buffer, (size_t)bytes, -1) < 0)
            return -1;
    }
    return 0;
}

/** @brief Receive complete PCM blocks and handle short ALSA/file writes. */
static int playback_audio(snd_pcm_t *pcm, int file, uint8_t *buffer, size_t frames)
{
    for (;;) {
        ssize_t bytes = read_block(PCM_FD, buffer, frames * AUDIO_IO_FRAME_BYTES);
        if (bytes < 0)
            return -1;
        if (bytes == 0)
            break;
        if (!pcm) {
            if (audio_pipe_write_all(file, buffer, (size_t)bytes, -1) < 0)
                return -1;
            continue;
        }
        size_t used = 0, count = (size_t)bytes / AUDIO_IO_FRAME_BYTES;
        while (used < count) {
            snd_pcm_sframes_t n = snd_pcm_writei(pcm,
                buffer + used * AUDIO_IO_FRAME_BYTES, count - used);
            if (n == -EAGAIN || n == 0) {
                if (wait_pcm(pcm) < 0)
                    return -1;
                continue;
            }
            if (n < 0) {
                if (recover_pcm(pcm, (int)n) < 0)
                    return -1;
                continue;
            }
            used += (size_t)n;
        }
    }
    if (pcm) {
        int rc;
        while ((rc = snd_pcm_drain(pcm)) == -EAGAIN) {
            if (wait_pcm(pcm) < 0)
                return -1;
        }
        if (rc < 0) { errno = -rc; return -1; }
    }
    return 0;
}

/** @brief Open only regular raw files; reject FIFOs/devices before truncation. */
static int open_file(const char *path, int capture)
{
    int flags = (capture ? O_RDONLY : O_WRONLY | O_CREAT) | O_NONBLOCK | O_CLOEXEC;
    int fd = open(path, flags, 0600);
    if (fd < 0)
        return -1;
    struct stat st;
    if (fstat(fd, &st) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        errno = EINVAL;
        return -1;
    }
    if (!capture && ftruncate(fd, 0) < 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    return fd;
}

int main(int argc, char **argv)
{
    if (argc != 5 || (strcmp(argv[1], "capture") && strcmp(argv[1], "playback")) ||
        (strcmp(argv[2], "file") && strcmp(argv[2], "alsa")))
        return EXIT_FAILURE;
    int capture = !strcmp(argv[1], "capture");
    char *end;
    errno = 0;
    unsigned long frames = strtoul(argv[4], &end, 10);
    int error = (errno || *end || !frames || frames > AUDIO_IO_MAX_BLOCK_FRAMES) ?
                EINVAL : 0;
    uint8_t *buffer = NULL;
    snd_pcm_t *pcm = NULL;
    int file = -1;
    if (!error) {
        buffer = malloc(frames * AUDIO_IO_FRAME_BYTES);
        if (!buffer)
            error = ENOMEM;
    }
    if (!error && !strcmp(argv[2], "file")) {
        file = open_file(argv[3], capture);
        if (file < 0)
            error = errno;
    } else if (!error) {
        int rc = snd_pcm_open(&pcm, argv[3], capture ? SND_PCM_STREAM_CAPTURE :
                              SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
        if (rc >= 0)
            rc = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S24_3LE,
                    SND_PCM_ACCESS_RW_INTERLEAVED, AUDIO_IO_CHANNELS,
                    AUDIO_IO_RATE, 0, 100000);
        if (rc < 0)
            error = -rc;
    }
    int ready = !error;
    if (audio_pipe_write_all(STATUS_FD, &error, sizeof(error), -1) < 0) {
        error = errno;
        ready = 0;
    }
    if (ready) {
        int rc = capture ? capture_audio(pcm, file, buffer, frames) :
                           playback_audio(pcm, file, buffer, frames);
        if (rc < 0)
            error = errno;
    }
    if (pcm) {
        if (capture || error)
            snd_pcm_drop(pcm);
        int rc = snd_pcm_close(pcm);
        if (rc < 0 && !error)
            error = -rc;
    }
    if (file >= 0 && close(file) < 0 && !error)
        error = errno;
    /* Data EOF and status are independent: EOF never hides a worker failure. */
    close(PCM_FD);
    if (capture)
        close(STOP_FD);
    if (ready)
        audio_pipe_write_all(STATUS_FD, &error, sizeof(error), -1);
    close(STATUS_FD);
    free(buffer);
    if (error)
        fprintf(stderr, "audio_io %s: %s\n", argv[1], strerror(error));
    return error ? EXIT_FAILURE : EXIT_SUCCESS;
}

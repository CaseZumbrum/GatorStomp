/** @file audio_io_pipe.c
 * @brief Partial-transfer, deadline, and SIGPIPE handling for PCM transport.
 */
#define _POSIX_C_SOURCE 200809L
#include "audio_io_process_internal.h"
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

int64_t audio_pipe_now(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

int audio_pipe_wait(int fd, short events, int64_t deadline)
{
    for (;;) {
        int timeout = -1;
        if (deadline >= 0) {
            int64_t now = audio_pipe_now();
            if (now < 0)
                return -1;
            int64_t left = deadline - now;
            if (left <= 0) {
                errno = ETIMEDOUT;
                return -1;
            }
            timeout = left > INT_MAX ? INT_MAX : (int)left;
        }
        struct pollfd p = {fd, events, 0};
        int rc = poll(&p, 1, timeout);
        if (rc > 0) {
            if (p.revents & POLLNVAL) {
                errno = EBADF;
                return -1;
            }
            /* HUP/ERR must reach read/write to yield EOF/EPIPE. */
            return 0;
        }
        if (rc == 0) {
            errno = ETIMEDOUT;
            return -1;
        }
        if (errno != EINTR)
            return -1;
    }
}

ssize_t audio_pipe_write(int fd, const void *data, size_t bytes)
{
    sigset_t blocked, previous, pending;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGPIPE);
    int rc = pthread_sigmask(SIG_BLOCK, &blocked, &previous);
    if (rc != 0) {
        errno = rc;
        return -1;
    }
    if (sigpending(&pending) < 0) {
        int saved = errno;
        pthread_sigmask(SIG_SETMASK, &previous, NULL);
        errno = saved;
        return -1;
    }
    int already_pending = sigismember(&pending, SIGPIPE);
    ssize_t written = write(fd, data, bytes);
    int saved = errno;
    if (written < 0 && saved == EPIPE && !already_pending) {
        struct timespec zero = {0, 0};
        while (sigtimedwait(&blocked, NULL, &zero) < 0 && errno == EINTR) {}
    }
    pthread_sigmask(SIG_SETMASK, &previous, NULL);
    errno = saved;
    return written;
}

int audio_pipe_write_all(int fd, const void *data, size_t bytes, int64_t deadline)
{
    const unsigned char *p = data;
    while (bytes) {
        if (audio_pipe_wait(fd, POLLOUT, deadline) < 0)
            return -1;
        ssize_t n = audio_pipe_write(fd, p, bytes);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -1;
        }
        if (n == 0) {
            errno = EIO;
            return -1;
        }
        p += n;
        bytes -= (size_t)n;
    }
    return 0;
}

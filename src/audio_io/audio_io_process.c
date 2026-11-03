/** @file audio_io_process.c
 * @brief POSIX worker launch, frame transport, and bounded child lifecycle.
 */
#define _GNU_SOURCE
#include "audio_io_process.h"
#include "audio_io_process_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

struct AudioIOProcess {
    int capture_fd, playback_fd, stop_fd;
    int capture_status, playback_status;
    pid_t capture_pid, playback_pid;
    size_t block_frames, buffered;
    uint8_t *buffer;
    int timeout_ms, eof, stopped, write_failed;
};

/** @brief Close an owned descriptor and mark it unavailable. */
static void close_fd(int *fd)
{
    if (*fd >= 0)
        close(*fd); /* Linux closes the FD even when close reports EINTR. */
    *fd = -1;
}

/** @brief Reserve 3..5 for worker arguments; all pipe ends are CLOEXEC/nonblocking. */
static int make_pipe(int pair[2])
{
    int raw[2];
    pair[0] = pair[1] = -1;
    if (pipe2(raw, O_CLOEXEC | O_NONBLOCK) < 0)
        return -1;
    for (int i = 0; i < 2; ++i) {
        pair[i] = fcntl(raw[i], F_DUPFD_CLOEXEC, 6);
        if (pair[i] < 0) {
            int saved = errno;
            close(raw[0]);
            close(raw[1]);
            close_fd(&pair[0]);
            errno = saved;
            return -1;
        }
    }
    close(raw[0]);
    close(raw[1]);
    return 0;
}

/** @brief Spawn with only standard streams and assigned transport descriptors. */
static int launch(const AudioIOProcessConfig *c, int capture, int data, int status,
                  int stop, pid_t *pid)
{
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    int rc = posix_spawn_file_actions_init(&actions);
    if (rc != 0) { errno = rc; return -1; }
    rc = posix_spawnattr_init(&attr);
    if (rc != 0) {
        posix_spawn_file_actions_destroy(&actions);
        errno = rc;
        return -1;
    }
    sigset_t mask, defaults;
    sigemptyset(&mask);
    sigemptyset(&defaults);
    sigaddset(&defaults, SIGPIPE);
    sigaddset(&defaults, SIGTERM);
    sigaddset(&defaults, SIGINT);
#define SPAWN_CHECK(call) do { rc = (call); if (rc != 0) goto done; } while (0)
    SPAWN_CHECK(posix_spawnattr_setsigmask(&attr, &mask));
    SPAWN_CHECK(posix_spawnattr_setsigdefault(&attr, &defaults));
    SPAWN_CHECK(posix_spawnattr_setflags(&attr,
                POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF));
    SPAWN_CHECK(posix_spawn_file_actions_adddup2(&actions, data, 3));
    SPAWN_CHECK(posix_spawn_file_actions_adddup2(&actions, status, 4));
    if (capture)
        SPAWN_CHECK(posix_spawn_file_actions_adddup2(&actions, stop, 5));
    SPAWN_CHECK(posix_spawn_file_actions_addclosefrom_np(&actions, capture ? 6 : 5));
    char block[32];
    snprintf(block, sizeof(block), "%zu", c->frames_per_buffer);
    const char *file = capture ? c->input_file : c->output_file;
    const char *device = capture ? c->capture_device : c->playback_device;
    char *argv[] = {(char *)c->worker_path, capture ? "capture" : "playback",
                    file ? "file" : "alsa", (char *)(file ? file : device),
                    block, NULL};
    rc = posix_spawn(pid, c->worker_path, &actions, &attr, argv, environ);
done:
    posix_spawnattr_destroy(&attr);
    posix_spawn_file_actions_destroy(&actions);
    if (rc != 0) { errno = rc; return -1; }
    return 0;
#undef SPAWN_CHECK
}

/** @brief Read one atomic worker status (positive errno, or zero). */
static int read_status(int fd, int64_t deadline, int *error)
{
    for (;;) {
        ssize_t n = read(fd, error, sizeof(*error));
        if (n == sizeof(*error)) {
            if (*error < 0) { errno = EPROTO; return -1; }
            return 0;
        }
        if (n >= 0) { errno = EIO; return -1; }
        if (errno == EINTR)
            continue;
        if (errno != EAGAIN || audio_pipe_wait(fd, POLLIN, deadline) < 0)
            return -1;
    }
}

/** @brief Reap only the named child, killing it when its deadline expires. */
static int reap(pid_t *pid, int64_t deadline, int *status)
{
    for (;;) {
        pid_t rc = waitpid(*pid, status, WNOHANG);
        if (rc == *pid) { *pid = -1; return 0; }
        if (rc < 0 && errno != EINTR) { *pid = -1; return -1; }
        int64_t now = audio_pipe_now();
        if (now < 0 || now >= deadline) {
            int saved = now < 0 ? errno : ETIMEDOUT;
            kill(*pid, SIGKILL);
            while (waitpid(*pid, status, 0) < 0 && errno == EINTR) {}
            *pid = -1;
            errno = saved;
            return -1;
        }
        struct timespec pause = {0, 5000000};
        nanosleep(&pause, NULL);
    }
}

void audio_io_process_abort(AudioIOProcess **owner)
{
    if (!owner || !*owner)
        return;
    int saved = errno;
    AudioIOProcess *p = *owner;
    close_fd(&p->capture_fd);
    close_fd(&p->playback_fd);
    close_fd(&p->stop_fd);
    pid_t *pids[] = {&p->capture_pid, &p->playback_pid};
    /* Kill both before waiting, so neither can hold the other up. */
    for (size_t i = 0; i < 2; ++i)
        if (*pids[i] > 0)
            kill(*pids[i], SIGKILL);
    for (size_t i = 0; i < 2; ++i)
        if (*pids[i] > 0)
            while (waitpid(*pids[i], NULL, 0) < 0 && errno == EINTR) {}
    close_fd(&p->capture_status);
    close_fd(&p->playback_status);
    free(p->buffer);
    free(p);
    *owner = NULL;
    errno = saved;
}

int audio_io_process_start(const AudioIOProcessConfig *c, AudioIOProcess **out)
{
    if (!out) { errno = EINVAL; return -1; }
    *out = NULL;
    if (!c || !c->worker_path || !*c->worker_path ||
        (!c->input_file && !c->capture_device) ||
        (!c->output_file && !c->playback_device) ||
        !c->frames_per_buffer || c->frames_per_buffer > AUDIO_IO_MAX_BLOCK_FRAMES ||
        c->timeout_ms <= 0) {
        errno = EINVAL;
        return -1;
    }
    if (c->input_file && c->output_file) {
        struct stat in, output;
        if (!strcmp(c->input_file, c->output_file) ||
            (stat(c->input_file, &in) == 0 && stat(c->output_file, &output) == 0 &&
             in.st_dev == output.st_dev && in.st_ino == output.st_ino)) {
            errno = EINVAL;
            return -1;
        }
    }
    AudioIOProcess *p = calloc(1, sizeof(*p));
    if (!p)
        return -1;
    p->capture_fd = p->playback_fd = p->stop_fd = -1;
    p->capture_status = p->playback_status = -1;
    p->capture_pid = p->playback_pid = -1;
    p->block_frames = c->frames_per_buffer;
    p->timeout_ms = c->timeout_ms;
    p->buffer = malloc(p->block_frames * AUDIO_IO_FRAME_BYTES);
    if (!p->buffer) { audio_io_process_abort(&p); return -1; }
    int capture[2] = {-1, -1}, playback[2] = {-1, -1}, stop[2] = {-1, -1};
    int cs[2] = {-1, -1}, ps[2] = {-1, -1};
    int rc = -1, error = 0;
    int64_t now = audio_pipe_now();
    if (now < 0)
        goto cleanup;
    int64_t deadline = now + c->timeout_ms;
    if (make_pipe(capture) < 0 || make_pipe(playback) < 0 || make_pipe(stop) < 0 ||
        make_pipe(cs) < 0 || make_pipe(ps) < 0)
        goto cleanup;
    if (launch(c, 0, playback[0], ps[1], -1, &p->playback_pid) < 0)
        goto cleanup;
    close_fd(&playback[0]);
    close_fd(&ps[1]);
    if (read_status(ps[0], deadline, &error) < 0)
        goto cleanup;
    if (error) { errno = error; goto cleanup; }
    if (launch(c, 1, capture[1], cs[1], stop[0], &p->capture_pid) < 0)
        goto cleanup;
    close_fd(&capture[1]);
    close_fd(&cs[1]);
    close_fd(&stop[0]);
    if (read_status(cs[0], deadline, &error) < 0)
        goto cleanup;
    if (error) { errno = error; goto cleanup; }
    p->capture_fd = capture[0]; capture[0] = -1;
    p->playback_fd = playback[1]; playback[1] = -1;
    p->stop_fd = stop[1]; stop[1] = -1;
    p->capture_status = cs[0]; cs[0] = -1;
    p->playback_status = ps[0]; ps[0] = -1;
    *out = p;
    rc = 0;
cleanup: {
        int saved = errno;
        for (int i = 0; i < 2; ++i) {
            close_fd(&capture[i]); close_fd(&playback[i]); close_fd(&stop[i]);
            close_fd(&cs[i]); close_fd(&ps[i]);
        }
        if (rc < 0)
            audio_io_process_abort(&p);
        errno = saved;
        return rc;
    }
}

ssize_t audio_io_process_read_frames(AudioIOProcess *p, uint8_t *buffer, size_t capacity)
{
    if (!p || !buffer || !capacity || capacity > p->block_frames) {
        errno = EINVAL;
        return -1;
    }
    size_t wanted = capacity * AUDIO_IO_FRAME_BYTES;
    int64_t now = audio_pipe_now();
    if (now < 0)
        return -1;
    int64_t deadline = now + p->timeout_ms;
    while (p->buffered < wanted && !p->eof) {
        if (audio_pipe_wait(p->capture_fd, POLLIN, deadline) < 0)
            return -1;
        ssize_t n = read(p->capture_fd, p->buffer + p->buffered, wanted - p->buffered);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -1;
        }
        if (n == 0) { p->eof = 1; break; }
        p->buffered += (size_t)n;
    }
    if (p->eof && p->buffered % AUDIO_IO_FRAME_BYTES) {
        errno = EPROTO;
        return -1;
    }
    size_t bytes = p->buffered < wanted ? p->buffered : wanted;
    memcpy(buffer, p->buffer, bytes);
    p->buffered -= bytes;
    memmove(p->buffer, p->buffer + bytes, p->buffered);
    return (ssize_t)(bytes / AUDIO_IO_FRAME_BYTES);
}

int audio_io_process_write_frames(AudioIOProcess *p, const uint8_t *buffer, size_t frames)
{
    if (!p || !buffer || !frames || frames > p->block_frames || p->write_failed) {
        errno = EINVAL;
        return -1;
    }
    int64_t now = audio_pipe_now();
    if (now < 0)
        return -1;
    if (audio_pipe_write_all(p->playback_fd, buffer, frames * AUDIO_IO_FRAME_BYTES,
                             now + p->timeout_ms) < 0) {
        p->write_failed = 1;
        return -1;
    }
    return 0;
}

int audio_io_process_request_capture_stop(AudioIOProcess *p)
{
    if (!p) { errno = EINVAL; return -1; }
    if (p->stopped)
        return 0;
    /* Closing the sole write end is an idempotent, SIGPIPE-free stop request. */
    close_fd(&p->stop_fd);
    p->stopped = 1;
    return 0;
}

int audio_io_process_finish(AudioIOProcess **owner, AudioIOProcessResult *result)
{
    if (!owner || !*owner) { errno = EINVAL; return -1; }
    AudioIOProcess *p = *owner;
    AudioIOProcessResult r = {0, 0, 0, 0};
    int64_t now = audio_pipe_now();
    int saved_clock = errno;
    int64_t deadline = now < 0 ? 0 : now + p->timeout_ms;
    audio_io_process_request_capture_stop(p);
    close_fd(&p->capture_fd);
    close_fd(&p->playback_fd); /* Playback sees EOF only after every writer closes. */
    int *errors[] = {&r.capture_error, &r.playback_error};
    int *statuses[] = {&r.capture_wait_status, &r.playback_wait_status};
    int fds[] = {p->capture_status, p->playback_status};
    pid_t *pids[] = {&p->capture_pid, &p->playback_pid};
    for (size_t i = 0; i < 2; ++i) {
        if (read_status(fds[i], deadline, errors[i]) < 0)
            *errors[i] = errno;
        if (reap(pids[i], deadline, statuses[i]) < 0)
            *errors[i] = errno;
        else if ((!WIFEXITED(*statuses[i]) || WEXITSTATUS(*statuses[i]) != 0) &&
                 !*errors[i])
            *errors[i] = EIO;
    }
    if (p->write_failed && !r.playback_error)
        r.playback_error = EIO;
    if (now < 0 && !r.capture_error)
        r.capture_error = saved_clock;
    audio_io_process_abort(owner);
    if (result)
        *result = r;
    if (r.capture_error || r.playback_error) {
        errno = r.capture_error ? r.capture_error : r.playback_error;
        return -1;
    }
    return 0;
}

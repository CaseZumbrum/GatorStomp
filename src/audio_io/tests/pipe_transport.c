/** @file pipe_transport.c
 * @brief Deterministic fault workers exercise the real public process API.
 * @details The test executable doubles as a worker protocol fixture, allowing
 * fragmented writes, early exit, stalled startup/completion, and stop to be tested
 * without timing-dependent hardware failures. All children are separately exec'd.
 */
#define _GNU_SOURCE
#include "audio_io_process.h"
#include "audio_io_process_internal.h"
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "line %d: %s (errno=%d: %s)\n", __LINE__, #condition, errno, strerror(errno)); \
    return -1; } } while (0)

/** @brief Sleep for fault injection, not for synchronization. */
static void delay_ms(long ms)
{
    struct timespec wait = {ms / 1000, (ms % 1000) * 1000000};
    while (nanosleep(&wait, &wait) < 0 && errno == EINTR) {}
}

/** @brief Private worker fixture using the production FD and status contract. */
static int fault_worker(char **argv)
{
    int capture = !strcmp(argv[1], "capture");
    const char *mode = argv[3];
    int error = 0;
    /* The launcher must close unrelated FDs even if not marked CLOEXEC. */
    if (fcntl(1000, F_GETFD) != -1 || errno != EBADF)
        error = EBADF;
    if (!strcmp(mode, "hang-start"))
        for (;;) pause();
    if (!strcmp(mode, "bad-start"))
        error = ENODEV;
    if (audio_pipe_write_all(4, &error, sizeof(error), -1) < 0 || error)
        return EXIT_FAILURE;
    if (!strcmp(mode, "crash")) {
        kill(getpid(), SIGKILL);
        return EXIT_FAILURE;
    }
    if (capture) {
        uint8_t bytes[17 * AUDIO_IO_FRAME_BYTES];
        for (size_t i = 0; i < sizeof(bytes); ++i)
            bytes[i] = (uint8_t)i;
        size_t count = !strcmp(mode, "incomplete") ? 7 : sizeof(bytes);
        int continuous = !strcmp(mode, "continuous");
        do {
            struct pollfd stop = {5, POLLIN, 0};
            if (poll(&stop, 1, 0) > 0)
                break;
            if (!strcmp(mode, "runtime-failure")) { error = EIO; break; }
            for (size_t i = 0; i < count; ++i) {
                if (audio_pipe_write_all(3, bytes + i, 1, -1) < 0) {
                    error = errno;
                    break;
                }
                if (!strcmp(mode, "slow") && i == 0)
                    delay_ms(150);
            }
        } while (continuous && !error);
    } else if (!strcmp(mode, "stall-playback")) {
        for (;;) pause();
    } else if (strcmp(mode, "exit-playback")) {
        uint8_t bytes[13]; /* Deliberately not a multiple of six. */
        for (;;) {
            ssize_t n = read(3, bytes, sizeof(bytes));
            if (n > 0)
                continue;
            if (n == 0)
                break;
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN || audio_pipe_wait(3, POLLIN, -1) < 0) {
                error = errno;
                break;
            }
        }
        if (!strcmp(mode, "hang-final"))
            for (;;) pause();
    } else {
        error = EIO;
    }
    close(3);
    audio_pipe_write_all(4, &error, sizeof(error), -1);
    return error ? EXIT_FAILURE : EXIT_SUCCESS;
}

/** @brief Create a config selecting fixture capture/playback modes. */
static AudioIOProcessConfig config_for(const char *capture, const char *playback)
{
    AudioIOProcessConfig c = AUDIO_IO_PROCESS_CONFIG_INIT;
    c.worker_path = "/proc/self/exe";
    c.input_file = capture;
    c.output_file = playback;
    c.frames_per_buffer = 17;
    c.timeout_ms = 1000;
    return c;
}

/** @brief Forward frames through the real C API and count them. */
static int pump(AudioIOProcess *p, size_t *total, int verify)
{
    uint8_t block[17 * AUDIO_IO_FRAME_BYTES];
    *total = 0;
    for (;;) {
        ssize_t n = audio_io_process_read_frames(p, block, 17);
        if (n < 0)
            return -1;
        if (n == 0)
            return 0;
        if (verify)
            for (size_t i = 0; i < (size_t)n * AUDIO_IO_FRAME_BYTES; ++i)
                CHECK(block[i] == (uint8_t)i);
        CHECK(audio_io_process_write_frames(p, block, (size_t)n) == 0);
        *total += (size_t)n;
    }
}

static int lifecycle_tests(const char *real_worker)
{
    AudioIOProcess *p = NULL;
    AudioIOProcessResult r;
    AudioIOProcessConfig c = config_for("fragment", "sink");
    size_t total;
    int sentinel = open("/dev/null", O_RDONLY);
    CHECK(sentinel >= 0);
    CHECK(dup2(sentinel, 1000) == 1000);
    close(sentinel);
    CHECK(audio_io_process_start(&c, &p) == 0);
    close(1000);
    CHECK(pump(p, &total, 1) == 0 && total == 17);
    CHECK(audio_io_process_get_capture_fd(p) == -1 && errno == EBUSY);
    CHECK(audio_io_process_get_playback_fd(p) == -1 && errno == EBUSY);
    CHECK(audio_io_process_finish(&p, &r) == 0 && p == NULL);
    CHECK(r.capture_error == 0 && r.playback_error == 0);
    CHECK(WIFEXITED(r.capture_wait_status) && WEXITSTATUS(r.capture_wait_status) == 0);
    CHECK(WIFEXITED(r.playback_wait_status) && WEXITSTATUS(r.playback_wait_status) == 0);

    c = config_for("incomplete", "sink");
    CHECK(audio_io_process_start(&c, &p) == 0);
    uint8_t block[17 * AUDIO_IO_FRAME_BYTES];
    CHECK(audio_io_process_read_frames(p, block, 17) == -1 && errno == EPROTO);
    audio_io_process_abort(&p);
    CHECK(p == NULL);
    audio_io_process_abort(&p); /* Idempotent cleanup. */

    c = config_for("runtime-failure", "sink");
    CHECK(audio_io_process_start(&c, &p) == 0);
    CHECK(audio_io_process_read_frames(p, block, 17) == 0);
    CHECK(audio_io_process_finish(&p, &r) == -1 && r.capture_error == EIO);

    c = config_for("continuous", "sink");
    CHECK(audio_io_process_start(&c, &p) == 0);
    CHECK(audio_io_process_read_frames(p, block, 17) == 17);
    CHECK(audio_io_process_write_frames(p, block, 17) == 0);
    CHECK(audio_io_process_request_capture_stop(p) == 0);
    CHECK(audio_io_process_request_capture_stop(p) == 0);
    CHECK(pump(p, &total, 1) == 0);
    CHECK(audio_io_process_finish(&p, &r) == 0);

    c = config_for("fragment", "bad-start");
    CHECK(audio_io_process_start(&c, &p) == -1 && errno == ENODEV && !p);
    c = config_for("bad-start", "sink");
    CHECK(audio_io_process_start(&c, &p) == -1 && errno == ENODEV && !p);
    c = config_for("fragment", "hang-start");
    c.timeout_ms = 100;
    int64_t before = audio_pipe_now();
    CHECK(audio_io_process_start(&c, &p) == -1 && errno == ETIMEDOUT && !p);
    CHECK(audio_pipe_now() - before < 1500);

    c = config_for("fragment", "hang-final");
    c.timeout_ms = 200;
    CHECK(audio_io_process_start(&c, &p) == 0);
    CHECK(pump(p, &total, 1) == 0);
    before = audio_pipe_now();
    CHECK(audio_io_process_finish(&p, &r) == -1 && r.playback_error == ETIMEDOUT && !p);
    CHECK(WIFSIGNALED(r.playback_wait_status));
    CHECK(audio_pipe_now() - before < 1500);

    c = config_for("slow", "sink");
    c.timeout_ms = 60;
    CHECK(audio_io_process_start(&c, &p) == 0);
    CHECK(audio_io_process_read_frames(p, block, 17) == -1 && errno == ETIMEDOUT);
    /* The helper may own a partial frame; a getter must not bypass it. */
    CHECK(audio_io_process_get_capture_fd(p) == -1 && errno == EBUSY);
    delay_ms(180);
    CHECK(audio_io_process_read_frames(p, block, 17) == 17);
    for (size_t i = 0; i < sizeof(block); ++i)
        CHECK(block[i] == (uint8_t)i);
    CHECK(audio_io_process_write_frames(p, block, 17) == 0);
    CHECK(audio_io_process_read_frames(p, block, 17) == 0);
    CHECK(audio_io_process_finish(&p, &r) == 0);

    c = config_for("fragment", "stall-playback");
    c.timeout_ms = 200;
    CHECK(audio_io_process_start(&c, &p) == 0);
    int write_rc;
    do { write_rc = audio_io_process_write_frames(p, block, 17); } while (write_rc == 0);
    CHECK(errno == ETIMEDOUT);
    CHECK(audio_io_process_write_frames(p, block, 17) == -1 && errno == EINVAL);
    audio_io_process_abort(&p);
    CHECK(!p);

    c = config_for("fragment", "exit-playback");
    CHECK(audio_io_process_start(&c, &p) == 0);
    int rc;
    do { rc = audio_io_process_write_frames(p, block, 17); } while (rc == 0);
    CHECK(errno == EPIPE); /* Test process has the default SIGPIPE disposition. */
    CHECK(audio_io_process_finish(&p, &r) == -1 && r.playback_error == EIO);

    c = config_for("fragment", "sink");
    c.worker_path = "/definitely/missing/gatorstomp-worker";
    CHECK(audio_io_process_start(&c, &p) == -1 && errno == ENOENT && !p);
    c = config_for("fragment", "sink");
    c.frames_per_buffer = 0;
    CHECK(audio_io_process_start(&c, &p) == -1 && errno == EINVAL && !p);
    c = config_for("same", "same");
    CHECK(audio_io_process_start(&c, &p) == -1 && errno == EINVAL && !p);
    c = config_for("/definitely/missing/input.raw", "/tmp/unused.raw");
    c.worker_path = real_worker;
    /* Playback uses ALSA null here so a failed capture cannot leave a test file. */
    c.output_file = NULL;
    c.playback_device = "null";
    CHECK(audio_io_process_start(&c, &p) == -1 && errno == ENOENT && !p);
    /* Exercise actual snd_pcm_readi/writei without physical hardware. */
    c = config_for(NULL, NULL);
    c.worker_path = real_worker;
    c.capture_device = "null";
    c.playback_device = "null";
    CHECK(audio_io_process_start(&c, &p) == 0);
    CHECK(audio_io_process_read_frames(p, block, 17) == 17);
    CHECK(audio_io_process_write_frames(p, block, 17) == 0);
    CHECK(audio_io_process_request_capture_stop(p) == 0);
    CHECK(pump(p, &total, 0) == 0);
    CHECK(audio_io_process_finish(&p, &r) == 0);

    /* Malformed input is also reported by the production worker's status API. */
    char input[] = "/tmp/gatorstomp-malformed-XXXXXX";
    int fixture = mkstemp(input);
    CHECK(fixture >= 0);
    CHECK(write(fixture, "1234567", 7) == 7);
    close(fixture);
    c = config_for(input, NULL);
    c.worker_path = real_worker;
    c.playback_device = "null";
    CHECK(audio_io_process_start(&c, &p) == 0);
    CHECK(audio_io_process_read_frames(p, block, 17) == 0);
    CHECK(audio_io_process_finish(&p, &r) == -1 && r.capture_error == EPROTO);
    unlink(input);

    c = config_for("crash", "sink");
    CHECK(audio_io_process_start(&c, &p) == 0);
    CHECK(audio_io_process_read_frames(p, block, 17) == 0);
    CHECK(audio_io_process_finish(&p, &r) == -1 && r.capture_error == EIO);
    CHECK(WIFSIGNALED(r.capture_wait_status));
    CHECK(waitpid(-1, NULL, WNOHANG) == -1 && errno == ECHILD);
    return 0;
}

/** @brief Raw blocking reads deliberately split frames into five-byte pieces. */
static ssize_t direct_read_block(int fd, uint8_t *buffer, size_t capacity)
{
    size_t used = 0;
    while (used < capacity) {
        size_t request = capacity - used;
        if (request > 5)
            request = 5;
        ssize_t n = read(fd, buffer + used, request);
        if (n > 0) {
            used += (size_t)n;
        } else if (n == 0) {
            break;
        } else if (errno != EINTR) {
            return -1;
        }
    }
    if (used % AUDIO_IO_FRAME_BYTES) { errno = EPROTO; return -1; }
    return (ssize_t)used;
}

/** @brief Exercise standard write with deliberately unaligned byte chunks. */
static int direct_write_block(int fd, const uint8_t *buffer, size_t bytes)
{
    while (bytes) {
        size_t request = bytes > 7 ? 7 : bytes;
        ssize_t n = write(fd, buffer, request);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return -1;
        bytes -= (size_t)n;
        buffer += n;
    }
    return 0;
}

static int direct_pump(int capture, int playback, size_t *total)
{
    uint8_t block[17 * AUDIO_IO_FRAME_BYTES];
    *total = 0;
    for (;;) {
        ssize_t bytes = direct_read_block(capture, block, sizeof(block));
        if (bytes < 0)
            return -1;
        if (!bytes)
            return 0;
        for (size_t i = 0; i < (size_t)bytes; ++i)
            CHECK(block[i] == (uint8_t)i);
        CHECK(direct_write_block(playback, block, (size_t)bytes) == 0);
        *total += (size_t)bytes / AUDIO_IO_FRAME_BYTES;
    }
}

/** @brief Check borrowed blocking FDs, mode exclusion, EOF, stop, and EPIPE. */
static int direct_access_tests(const char *real_worker)
{
    CHECK(audio_io_process_get_capture_fd(NULL) == -1 && errno == EINVAL);
    CHECK(audio_io_process_get_playback_fd(NULL) == -1 && errno == EINVAL);
    AudioIOProcess *p = NULL;
    AudioIOProcessResult result;
    AudioIOProcessConfig c = config_for("fragment", "sink");
    CHECK(audio_io_process_start(&c, &p) == 0);
    int capture = audio_io_process_get_capture_fd(p);
    int playback = audio_io_process_get_playback_fd(p);
    CHECK(capture >= 0 && playback >= 0 && capture != playback);
    CHECK(audio_io_process_get_capture_fd(p) == capture);
    CHECK(audio_io_process_get_playback_fd(p) == playback);
    int flags = fcntl(capture, F_GETFL);
    CHECK(flags >= 0 && !(flags & O_NONBLOCK) && (flags & O_ACCMODE) == O_RDONLY);
    flags = fcntl(playback, F_GETFL);
    CHECK(flags >= 0 && !(flags & O_NONBLOCK) && (flags & O_ACCMODE) == O_WRONLY);
    CHECK(fcntl(capture, F_GETFD) & FD_CLOEXEC);
    CHECK(fcntl(playback, F_GETFD) & FD_CLOEXEC);
    uint8_t block[17 * AUDIO_IO_FRAME_BYTES];
    CHECK(audio_io_process_read_frames(p, block, 17) == -1 && errno == EBUSY);
    CHECK(audio_io_process_write_frames(p, block, 17) == -1 && errno == EBUSY);
    size_t total;
    CHECK(direct_pump(capture, playback, &total) == 0 && total == 17);
    CHECK(audio_io_process_finish(&p, &result) == 0 && !p);
    CHECK(fcntl(capture, F_GETFD) == -1 && errno == EBADF);
    CHECK(fcntl(playback, F_GETFD) == -1 && errno == EBADF);

    c = config_for("continuous", "sink");
    CHECK(audio_io_process_start(&c, &p) == 0);
    capture = audio_io_process_get_capture_fd(p);
    playback = audio_io_process_get_playback_fd(p);
    CHECK(capture >= 0 && playback >= 0);
    CHECK(direct_read_block(capture, block, sizeof(block)) == sizeof(block));
    CHECK(direct_write_block(playback, block, sizeof(block)) == 0);
    CHECK(audio_io_process_request_capture_stop(p) == 0);
    CHECK(audio_io_process_request_capture_stop(p) == 0);
    CHECK(direct_pump(capture, playback, &total) == 0);
    CHECK(audio_io_process_finish(&p, &result) == 0 && !p);

    c = config_for("incomplete", "sink");
    CHECK(audio_io_process_start(&c, &p) == 0);
    capture = audio_io_process_get_capture_fd(p);
    playback = audio_io_process_get_playback_fd(p);
    CHECK(capture >= 0 && playback >= 0);
    CHECK(direct_pump(capture, playback, &total) == -1 && errno == EPROTO);
    audio_io_process_abort(&p);
    CHECK(!p);
    CHECK(fcntl(capture, F_GETFD) == -1 && errno == EBADF);
    CHECK(fcntl(playback, F_GETFD) == -1 && errno == EBADF);

    c = config_for("fragment", "exit-playback");
    CHECK(audio_io_process_start(&c, &p) == 0);
    capture = audio_io_process_get_capture_fd(p);
    playback = audio_io_process_get_playback_fd(p);
    CHECK(capture >= 0 && playback >= 0);
    CHECK(direct_read_block(capture, block, sizeof(block)) == sizeof(block));
    CHECK(direct_read_block(capture, block, sizeof(block)) == 0);
    /* Direct writes require caller signal policy. Block/consume SIGPIPE only
     * here, preserving the process default and the calling thread's old mask. */
    sigset_t set, previous;
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    CHECK(sigprocmask(SIG_BLOCK, &set, &previous) == 0);
    ssize_t written;
    do { written = write(playback, block, sizeof(block)); }
    while (written >= 0 || errno == EINTR);
    CHECK(errno == EPIPE);
    struct timespec zero = {0, 0};
    CHECK(sigtimedwait(&set, NULL, &zero) == SIGPIPE);
    CHECK(sigprocmask(SIG_SETMASK, &previous, NULL) == 0);
    CHECK(audio_io_process_finish(&p, &result) == -1 && result.playback_error == EIO);

    /* A raw writer can truncate a frame: the real playback worker must reject
     * it at EOF, and finish must expose that worker error to the caller. */
    char input[] = "/tmp/gatorstomp-direct-XXXXXX";
    int fixture = mkstemp(input);
    CHECK(fixture >= 0);
    CHECK(write(fixture, "123456", 6) == 6);
    close(fixture);
    c = config_for(input, NULL);
    c.worker_path = real_worker;
    c.playback_device = "null";
    CHECK(audio_io_process_start(&c, &p) == 0);
    capture = audio_io_process_get_capture_fd(p);
    playback = audio_io_process_get_playback_fd(p);
    CHECK(capture >= 0 && playback >= 0);
    CHECK(direct_read_block(capture, block, sizeof(block)) == 6);
    CHECK(direct_read_block(capture, block, sizeof(block)) == 0);
    CHECK(write(playback, "1234567", 7) == 7);
    CHECK(audio_io_process_finish(&p, &result) == -1 && result.playback_error == EPROTO);
    unlink(input);
    CHECK(waitpid(-1, NULL, WNOHANG) == -1 && errno == ECHILD);
    return 0;
}

static volatile sig_atomic_t signals_seen;
static void interrupted(int signal_number)
{
    (void)signal_number;
    ++signals_seen;
}

/** @brief Test SIGPIPE pending/mask preservation and interrupted poll retry. */
static int signal_tests(void)
{
    int pipe_fds[2];
    CHECK(pipe2(pipe_fds, O_CLOEXEC | O_NONBLOCK) == 0);
    close(pipe_fds[0]);
    sigset_t set, old, pending;
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    CHECK(sigprocmask(SIG_BLOCK, &set, &old) == 0);
    CHECK(raise(SIGPIPE) == 0);
    CHECK(audio_pipe_write(pipe_fds[1], "x", 1) == -1 && errno == EPIPE);
    CHECK(sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE));
    struct timespec zero = {0, 0};
    CHECK(sigtimedwait(&set, NULL, &zero) == SIGPIPE);
    CHECK(sigprocmask(SIG_SETMASK, &old, NULL) == 0);
    close(pipe_fds[1]);

    struct sigaction action = {0}, previous;
    action.sa_handler = interrupted;
    sigemptyset(&action.sa_mask);
    CHECK(sigaction(SIGUSR1, &action, &previous) == 0);
    CHECK(pipe2(pipe_fds, O_CLOEXEC | O_NONBLOCK) == 0);
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        close(pipe_fds[0]);
        delay_ms(20);
        kill(getppid(), SIGUSR1);
        delay_ms(20);
        int ok = write(pipe_fds[1], "x", 1) == 1;
        _exit(ok ? 0 : 1);
    }
    close(pipe_fds[1]);
    CHECK(audio_pipe_wait(pipe_fds[0], POLLIN, audio_pipe_now() + 1000) == 0);
    CHECK(signals_seen > 0);
    close(pipe_fds[0]);
    int status;
    while (waitpid(child, &status, 0) < 0)
        CHECK(errno == EINTR);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(sigaction(SIGUSR1, &previous, NULL) == 0);
    return 0;
}

/** @brief Count open descriptors, including this temporary directory stream. */
static int fd_count(void)
{
    DIR *directory = opendir("/proc/self/fd");
    if (!directory)
        return -1;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL)
        if (entry->d_name[0] != '.')
            ++count;
    closedir(directory);
    return count;
}

int main(int argc, char **argv)
{
    if (argc == 5)
        return fault_worker(argv);
    if (argc != 2)
        return EXIT_FAILURE;
    struct sigaction default_pipe = {0};
    default_pipe.sa_handler = SIG_DFL;
    sigemptyset(&default_pipe.sa_mask);
    if (sigaction(SIGPIPE, &default_pipe, NULL) < 0)
        return EXIT_FAILURE;
    int descriptors = fd_count();
    if (descriptors < 0 || lifecycle_tests(argv[1]) < 0 ||
        direct_access_tests(argv[1]) < 0 || signal_tests() < 0)
        return EXIT_FAILURE;
    struct sigaction pipe_after;
    if (sigaction(SIGPIPE, NULL, &pipe_after) < 0 || pipe_after.sa_handler != SIG_DFL) {
        fprintf(stderr, "Process-wide SIGPIPE disposition changed\n");
        return EXIT_FAILURE;
    }
    if (fd_count() != descriptors) {
        fprintf(stderr, "File descriptor leak detected\n");
        return EXIT_FAILURE;
    }
    puts("Pipe lifecycle, fragmented frames, timeouts, errors, SIGPIPE and EINTR passed.");
    return EXIT_SUCCESS;
}

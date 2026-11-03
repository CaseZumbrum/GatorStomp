/**
 * @file audio_io_process.h
 * @brief Linux child-process transport for 96 kHz stereo S24_3LE audio.
 * @details Each frame is six bytes. PCM pipes contain no headers or control
 * messages. One caller thread owns a handle; do not call its functions concurrently.
 * The application must not reap library-owned children, set SIGCHLD to SIG_IGN,
 * or enable SA_NOCLDWAIT. Library functions are not async-signal-safe: signal
 * handlers should set a flag for the owning thread instead of calling this API.
 * The existing audio_io_run() API is independent and unchanged.
 *
 * @par Direct POSIX integration
 * Start workers, get the capture/read and playback/write descriptors, and use
 * read()/write() declared in @c unistd.h. Both endpoints are blocking. Pipes
 * carry bytes, not frames: retain partial input frames, retry EINTR and short
 * writes, and reject EOF with leftover bytes (six bytes per frame). Only process
 * complete frames. A successful read may return fewer bytes than requested.
 * Choose direct access or the frame helper separately for each pipe and never
 * mix them on that pipe during one handle's lifetime; attempts return EBUSY.
 *
 * @par Ownership and shutdown
 * Getters borrow the library's descriptors; do not close, duplicate, replace,
 * change descriptor flags, or pass them to other processes. Aliases of the
 * playback writer can prevent EOF and playback drain. The library closes both
 * endpoints in finish()/abort(); all borrowed FD values are invalid afterwards.
 * Request capture stop, continue forwarding complete frames until capture EOF,
 * then call finish() to close playback input, drain, and collect worker status.
 * On a transfer error, call abort(). Never call lifecycle functions concurrently
 * with read()/write(). EOF alone does not prove worker success: check finish().
 *
 * @par Blocking and signals
 * timeout_ms applies to library helpers and startup/finish, not direct syscalls.
 * A direct read/write can block indefinitely; the application owns cancellation
 * and signal policy. A raw write to a disconnected playback pipe raises SIGPIPE
 * and fails with EPIPE. Arrange an application policy (for example SIG_IGN, or
 * blocking/consuming SIGPIPE in the writing thread) before writing. The getters
 * do not install handlers or provide the frame helper's SIGPIPE protection.
 *
 * @par Existing orchestrator
 * The C++ target needs to link audio_io, depend on audio_io_worker, include this
 * header, and supply an explicit deployed worker path in its startup config.
 * Keep IO_Controller's blocking constructor/recv_msg off the running audio loop;
 * see examples/orchestrator_integration.cpp for a startup handshake and separate
 * control task. That finite example chooses SIGPIPE=SIG_IGN for its own process.
 * A production control task still needs its own shutdown/unblock policy.
 * SOCK_SEQPACKET/gs_socket_msg control traffic never enters the PCM pipes.
 */
#ifndef AUDIO_IO_PROCESS_H
#define AUDIO_IO_PROCESS_H
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Fixed wire-format constants; block sizes are measured in frames. */
enum { AUDIO_IO_RATE = 96000, AUDIO_IO_CHANNELS = 2, AUDIO_IO_FRAME_BYTES = 6,
       AUDIO_IO_MAX_BLOCK_FRAMES = 65536 };
/** @brief Opaque owner of PCM/status/stop descriptors and both child PIDs. */
typedef struct AudioIOProcess AudioIOProcess;

/** @brief Borrowed startup configuration; strings need only survive start(). */
typedef struct {
    const char *worker_path;     /**< Explicit path to audio_io_worker (not PATH lookup). */
    const char *capture_device;  /**< ALSA capture name; ignored with input_file. */
    const char *playback_device; /**< ALSA playback name; ignored with output_file. */
    const char *input_file;      /**< Optional finite raw source for hardware-free use. */
    const char *output_file;     /**< Optional raw sink, truncated on startup. */
    size_t frames_per_buffer;   /**< 1..AUDIO_IO_MAX_BLOCK_FRAMES; default 128. */
    int timeout_ms;             /**< Positive deadline per helper/start/finish; not raw FD I/O. */
} AudioIOProcessConfig;
/** @brief Default configuration; caller must supply worker_path. */
#define AUDIO_IO_PROCESS_CONFIG_INIT {NULL, "default", "default", NULL, NULL, 128, 3000}

/** @brief Worker completion results, including raw waitpid() statuses.
 * Errors are positive errno values; EIO also represents abnormal child exit or
 * missing final status. ETIMEDOUT means cleanup had to kill an unresponsive child.
 */
typedef struct {
    int capture_error;       /**< Capture startup/runtime/cleanup error, or zero. */
    int playback_error;      /**< Playback startup/runtime/cleanup error, or zero. */
    int capture_wait_status; /**< Inspect with WIFEXITED/WEXITSTATUS/WIFSIGNALED. */
    int playback_wait_status;/**< Inspect with WIFEXITED/WEXITSTATUS/WIFSIGNALED. */
} AudioIOProcessResult;

/** @brief Start two exec'd workers and wait for both startup acknowledgements.
 * @param config Valid configuration; fixed format is always 96000/stereo/S24_3LE.
 * @param out Receives ownership on success; set to NULL on failure. Must not
 * already own a running handle.
 * @return 0 on success; -1 with errno on failure, after closing/reaping children.
 * @details Uses pipe2/posix_spawn/waitpid. Only standard descriptors and each
 * worker's assigned PCM/status/stop descriptors survive exec. Input and output
 * files must be different regular files (existing aliases are rejected).
 * Startup is not transactional: an output file may be truncated before failure.
 */
int audio_io_process_start(const AudioIOProcessConfig *config, AudioIOProcess **out);

/** @brief Borrow the blocking read end of the capture PCM pipe.
 * @param process Live handle owned by the calling thread.
 * @return Readable FD (including zero), or -1 with errno: EINVAL for NULL,
 * EBUSY if read_frames() already selected helper access, or a fcntl() error.
 * @details The first successful call selects direct capture access and clears
 * O_NONBLOCK on this endpoint only. Repeated calls return the same borrowed FD.
 * Do not close it or mix it with read_frames(); see the file ownership contract.
 */
int audio_io_process_get_capture_fd(AudioIOProcess *process);

/** @brief Borrow the blocking write end of the playback PCM pipe.
 * @param process Live handle owned by the calling thread.
 * @return Writable FD (including zero), or -1 with errno: EINVAL for NULL,
 * EBUSY if write_frames() already selected helper access, or a fcntl() error.
 * @details The first successful call selects direct playback access and clears
 * O_NONBLOCK on this endpoint only. Repeated calls return the same borrowed FD.
 * Caller must handle SIGPIPE/EPIPE, partial writes, and EINTR. Do not close or
 * duplicate it: finish() must close the sole parent writer to deliver EOF.
 */
int audio_io_process_get_playback_fd(AudioIOProcess *process);

/** @brief Read up to capacity complete capture frames into borrowed storage.
 * @return Frame count, zero at clean EOF, or -1 with errno (EPROTO for truncated
 * frames). Fills capacity unless EOF intervenes. capacity is 1..configured block.
 * @details A timed-out read retains accumulated bytes for the next call. Other
 * read errors require abort. EOF alone does not establish worker success: finish
 * must collect final status. No descriptor is transferred to the caller.
 * Selects helper capture access; cannot be mixed with get_capture_fd() on this
 * handle. Returns -1/EBUSY if direct capture access was already selected.
 */
ssize_t audio_io_process_read_frames(AudioIOProcess *process, uint8_t *buffer,
                                    size_t capacity);
/** @brief Write complete frames, retrying short writes and EINTR.
 * @return 0 on success; -1 with errno on failure. Any write failure is terminal:
 * some bytes may already have reached playback; abort rather than retrying a block.
 * @details frames is 1..configured block. SIGPIPE is blocked only in the calling
 * thread during writes; newly generated SIGPIPE is consumed before mask restore.
 * Existing signal dispositions and preexisting pending signals are preserved.
 * Selects helper playback access; cannot be mixed with get_playback_fd() on this
 * handle. Returns -1/EBUSY if direct playback access was already selected.
 */
int audio_io_process_write_frames(AudioIOProcess *process, const uint8_t *buffer,
                                  size_t frames);
/** @brief Idempotently request capture stop at a block boundary.
 * @return 0 on success or -1 with errno. Continue read/process/write until EOF to
 * preserve queued audio. Capture checks the stop channel while waiting for ALSA.
 */
int audio_io_process_request_capture_stop(AudioIOProcess *process);
/** @brief Close pipes, drain playback, collect status, reap, and free *process.
 * @return 0 if both workers succeeded; -1 with errno otherwise. Always sets
 * *process to NULL for a valid handle; result may be NULL.
 * @details Normally call after capture EOF and forwarding the final block.
 * Calling early discards capture data and may report EPIPE. A deadline bounds
 * waiting for both children; remaining children are killed and reaped.
 * Borrowed PCM descriptors are closed here; callers must not close them first.
 */
int audio_io_process_finish(AudioIOProcess **process, AudioIOProcessResult *result);
/** @brief Cancel without draining, kill/reap children, close all FDs, free handle.
 * @details NULL handles are accepted. Intended for transfer errors and exception
 * paths. Does not alter process-wide signal handlers or reap unrelated children.
 */
void audio_io_process_abort(AudioIOProcess **process);
#ifdef __cplusplus
}
#endif
#endif

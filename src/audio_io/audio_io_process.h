/**
 * @file audio_io_process.h
 * @brief Linux child-process transport for 96 kHz stereo S24_3LE audio.
 * @details Each frame is six bytes. PCM pipes contain no headers or control
 * messages. One caller thread owns a handle; do not call its functions concurrently.
 * The application must not reap library-owned children, set SIGCHLD to SIG_IGN,
 * or enable SA_NOCLDWAIT. Library functions are not async-signal-safe: signal
 * handlers should set a flag for the owning thread instead of calling this API.
 * The existing audio_io_run() API is independent and unchanged.
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
    int timeout_ms;             /**< Positive deadline per transfer/start/finish operation. */
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

/** @brief Read up to capacity complete capture frames into borrowed storage.
 * @return Frame count, zero at clean EOF, or -1 with errno (EPROTO for truncated
 * frames). Fills capacity unless EOF intervenes. capacity is 1..configured block.
 * @details A timed-out read retains accumulated bytes for the next call. Other
 * read errors require abort. EOF alone does not establish worker success: finish
 * must collect final status. No descriptor is transferred to the caller.
 */
ssize_t audio_io_process_read_frames(AudioIOProcess *process, uint8_t *buffer,
                                    size_t capacity);
/** @brief Write complete frames, retrying short writes and EINTR.
 * @return 0 on success; -1 with errno on failure. Any write failure is terminal:
 * some bytes may already have reached playback; abort rather than retrying a block.
 * @details frames is 1..configured block. SIGPIPE is blocked only in the calling
 * thread during writes; newly generated SIGPIPE is consumed before mask restore.
 * Existing signal dispositions and preexisting pending signals are preserved.
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

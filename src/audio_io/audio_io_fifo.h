/** @file audio_io_fifo.h
 * @brief Small Linux FIFO helpers for the worker and pass-through executables.
 * @details One reader and one writer per FIFO. All PCM is 96000 Hz, stereo,
 * signed packed 24-bit little-endian: six bytes per interleaved frame.
 * These helpers never create FIFOs, launch processes, or install signal handlers.
 * Callers own/close returned descriptors and choose their own SIGPIPE policy.
 * A stop pointer may be NULL, or point to a caller-owned signal-safe flag that
 * stays valid throughout the call. Workers use it for SIGINT/SIGTERM shutdown.
 */
#ifndef AUDIO_IO_FIFO_H
#define AUDIO_IO_FIFO_H
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { AUDIO_IO_RATE = 96000, AUDIO_IO_CHANNELS = 2, AUDIO_IO_FRAME_BYTES = 6,
       AUDIO_IO_MAX_BLOCK_FRAMES = 65536, AUDIO_IO_TIMEOUT_MS = 5000 };

/** @brief Open an existing FIFO without changing process signal policy.
 * @param path Existing FIFO path. Never created, truncated, or removed here.
 * @param writing Nonzero opens write-only; zero opens read-only.
 * @param stop Optional caller-owned stop flag; nonzero returns ECANCELED.
 * @return Owned nonblocking CLOEXEC FD, or -1 with errno.
 * @details A writer retries ENXIO for up to five seconds while waiting for its
 * reader. A reader opens immediately; use read_block() to wait for first data or
 * a writer's closure. A raw read before a writer connects can return zero.
 * Use these transfer helpers with nonblocking descriptors; they handle EAGAIN.
 */
int audio_fifo_open(const char *path, int writing,
                    const volatile sig_atomic_t *stop);
/** @brief Monotonic milliseconds, or -1 with errno. */
int64_t audio_fifo_now(void);
/** @brief Fill up to bytes, or return a short final block at EOF.
 * @return Bytes read, zero at EOF, -1 with errno on error. bytes must be a
 * positive multiple of six. EOF within a frame returns EPROTO. A nonzero
 * optional stop flag returns ECANCELED. A block has a five-second deadline; errors are terminal.
 * @details Also accepts regular raw input files. Retains partial reads within
 * this call; callers must not retry a failed block because bytes may be lost.
 */
ssize_t audio_fifo_read_block(int fd, uint8_t *buffer, size_t bytes,
                              const volatile sig_atomic_t *stop);
/** @brief Write all bytes, retrying partial writes, EINTR, and EAGAIN.
 * @return 0 on success, -1 with errno on error. Never retry a failed block.
 * @details Completes the current block despite a stop signal when possible;
 * a five-second deadline bounds a stalled peer. The executable must arrange
 * SIGPIPE handling (for example ignore it so a broken pipe returns EPIPE).
 * No handlers are installed here. A failed block may already be partly written.
 */
int audio_fifo_write_all(int fd, const uint8_t *buffer, size_t bytes);
#ifdef __cplusplus
}
#endif
#endif

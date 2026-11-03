/** @file audio_io_process_internal.h
 * @brief Private deadline and pipe helpers shared by the launcher and worker.
 */
#ifndef AUDIO_IO_PROCESS_INTERNAL_H
#define AUDIO_IO_PROCESS_INTERNAL_H
#include <stdint.h>
#include <sys/types.h>
/** @brief Monotonic milliseconds, or -1 with errno. */
int64_t audio_pipe_now(void);
/** @brief Wait for fd readiness until deadline; retries EINTR. */
int audio_pipe_wait(int fd, short events, int64_t deadline);
/** @brief Write with thread-local SIGPIPE protection. */
ssize_t audio_pipe_write(int fd, const void *data, size_t bytes);
/** @brief Write all bytes before deadline (-1 means no deadline). */
int audio_pipe_write_all(int fd, const void *data, size_t bytes, int64_t deadline);
#endif

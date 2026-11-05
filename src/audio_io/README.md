# ADC/DAC process transport

`audio_io_run()` remains the original blocking, single-process file/ALSA pipeline.
Its configuration, DSP callback, and existing PCM integrity test are unchanged.
The additional `audio_io_process.h` API lets the existing C++ orchestrator own
separate capture and playback processes, with DSP remaining in the orchestrator.
No source files in `audio_orchestrator` or `gs_socket_api` have been modified.

## Data and descriptor contract

```text
ALSA ADC -> capture worker -> PCM pipe -> orchestrator/DSP
                                         |
ALSA DAC <- playback worker <- PCM pipe <-+
```

The fixed format is 96,000 frames/second, two interleaved channels, signed packed
24-bit little-endian PCM (`S24_3LE`). A frame is six bytes: left sample followed by
right sample. There are no headers, sample conversions, resampling, or control
messages in either PCM pipe. The default block is 128 frames / 768 bytes; allowed
blocks are 1..65536 frames. Pipe capacity is independent of block size and is not
assumed to be a multiple of six. All transfer code handles short reads/writes.

The library exclusively owns the descriptors and child PIDs:

| Channel | Parent/orchestrator | Capture child | Playback child |
| --- | --- | --- | --- |
| Capture PCM | Read end | Write end, FD 3 | Closed |
| Playback PCM | Write end | Closed | Read end, FD 3 |
| Capture stop | Write end, closed to request stop | Read end, FD 5 | Closed |
| Worker status (one pipe per child) | Read end | Write end, FD 4 | Write end, FD 4 |

The public API does not expose these descriptors. Unused ends are closed. Spawn
file actions retain only standard descriptors and the child's assigned channels;
this includes closing the real controller's sockets, which currently lack
`CLOEXEC`. Standard input/output/error remain inherited for diagnostics, not PCM.
The worker executable is private to this API, not a peripheral control client.

Each worker sends one atomic integer status after opening/configuring its
endpoint, and another after runtime cleanup: zero means success, a positive
`errno` means failure. Runtime failures are also printed to stderr. The parent
waits for both startup acknowledgements before `start()` succeeds. PCM EOF alone
never proves successful completion: always check `finish()` and its per-worker
results, including `waitpid()` status. ALSA errors use their corresponding errno;
recovery can lose audio and does not guarantee uninterrupted samples.

## Public API and lifetime

- `audio_io_process_start(config, &process)`: create pipes, `posix_spawn()` both
  workers, and await readiness. On failure, close all owned descriptors and
  kill/reap launched children. Supply an explicit worker path; there is no PATH
  search or dependency on the current directory.
- `audio_io_process_read_frames(process, buffer, capacity)`: fill up to capacity
  frames, with a short final block at EOF. Return the frame count, zero for EOF,
  or -1 with errno. A read timeout retains accumulated bytes for a subsequent
  read. Other read errors require abort. `EPROTO` reports EOF inside a frame.
- `audio_io_process_write_frames(process, buffer, frames)`: transmit a complete
  block, returning zero or -1 with errno. Any failure is terminal; abort rather
  than retrying a possibly partly transmitted block. `EPIPE` means playback
  disconnected; `ETIMEDOUT` means it stopped consuming in time.
- `audio_io_process_request_capture_stop(process)`: idempotently close the stop
  channel. Keep reading, processing, and writing until capture EOF. Capture stops
  between complete blocks; it finishes an in-progress pipe write rather than
  truncating a frame. While waiting for ALSA readiness it checks stop at roughly
  50 ms intervals. This is not a real-time deadline.
- `audio_io_process_finish(&process, &result)`: after forwarding all capture
  frames, close playback input so it drains and exits; collect both statuses,
  reap children, free the handle, and set it to NULL, even on error. Calling it
  before consuming capture EOF discards capture data and may report `EPIPE`.
- `audio_io_process_abort(&process)`: close channels, kill both workers, reap
  them, and free the handle. NULL is accepted. Use on errors/exception paths;
  queued audio may be discarded.

`timeout_ms` is a positive monotonic deadline per public transfer, startup, or
finish operation, not per individual system call. A finish timeout kills and
reaps remaining workers and reports `ETIMEDOUT`. Kernel scheduling and reaping
can add delay; this is not a hard real-time shutdown guarantee. Abort sends
SIGKILL directly. Neither operation reaps unrelated children.

A single thread must own each handle. To stop from a UI, control thread, or signal
handler, set an application flag and let the audio-owning thread call stop/abort.
The functions are not async-signal-safe. Do not reap these child PIDs yourself or
configure automatic child reaping with `SIGCHLD=SIG_IGN` / `SA_NOCLDWAIT`.
Read and write storage remains caller-owned. Configuration strings only need to
survive `start()`. Raw fixtures must be regular files, must remain at stable paths
during startup, and input/output must differ (existing hard-link/symlink aliases
are rejected). Output is truncated at startup and may be partial on failure;
startup has no rollback and successful writes do not imply durable disk storage.

Pipe writes block SIGPIPE only in the calling thread, preserve the prior mask
and existing pending SIGPIPE, consume a newly generated SIGPIPE after EPIPE, then
restore the mask. They do not install a global handler or change dispositions.

## Build and hardware-free demonstration

Requires Linux, ALSA development files, pthreads, and glibc providing
`posix_spawn_file_actions_addclosefrom_np` (glibc 2.34 or later). CMake checks that
spawn facility explicitly; musl/older glibc need a close-from spawn alternative.
No changes to the root CMake file are necessary. Existing project targets remain.

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/bin/audio_io_orchestrator_example ./build/bin/audio_io_worker
./build/bin/audio_io_orchestrator_example ./build/bin/audio_io_worker 32768
```

The example generates a 32771-frame fixture, launches the real capture/playback
executables in raw-file mode, forwards PCM unchanged through the public C API,
and compares all 196626 output bytes. It compiles the existing `IO_Controller`
implementation unchanged, receives a real packed `gs_socket_msg` from a simulated
peripheral on `SOCK_SEQPACKET`, and verifies it separately from PCM. Temporary
files and the socket path are removed on completion.

`IO_Controller` accepts a connection in its constructor, so the example starts
a known simulated peer and completes that blocking startup **before** starting
audio. It calls the blocking `recv_msg()` on a dedicated control thread; the PCM
loop contains no control socket operations. The example receives exactly one
known message and joins that finite task. It is not a production control service:
with the current controller, absent peripherals can block startup indefinitely.
CTest bounds the entire demonstration with a timeout. A permanent control thread
requires a coordinated way to unblock accept/recv during application shutdown.

Tests cover the unchanged PCM integrity check, fragmented byte transfers,
incomplete frames, startup failures in either worker, crashes and runtime errors,
capture stop/drain, slow capture with buffered read-timeout recovery, stalled
playback/write timeout, startup/finish timeouts, broken pipes under the default
SIGPIPE disposition, preservation of a pending blocked SIGPIPE, interrupted poll,
FD inheritance, descriptor leaks, child reaping, and both C++ demonstrations.
ALSA `null` capture/playback also exercise `snd_pcm_readi()`/`snd_pcm_writei()`
and orderly stop without physical devices. Large-block demonstrations force
partial pipe transfers and include a short final frame block.

## Exact changes eventually needed in the teammate's application

These are instructions, not changes made to their files:

1. Add `target_link_libraries(audio_orchestrator PRIVATE audio_io)` and
   `add_dependencies(audio_orchestrator audio_io_worker)` after all subdirectories
   are declared in the root CMake file (or coordinate equivalent target changes).
   The executable must be deployed alongside the worker; supply its explicit path.
2. In `src/audio_orchestrator/main.cpp`, include `audio_io_process.h`. Replace the
   current receive-one-message-and-exit demo with application lifetime management.
3. Decide how control clients connect before audio startup. Keep `IO_Controller`
   construction and `recv_msg()` off the running PCM path. The finite example
   demonstrates startup handshake plus a dedicated receiving thread; production
   needs a cancellable control-thread lifecycle or a future nonblocking controller
   interface agreed with its owner. Do not put `recv_msg()` inside the audio loop.
4. Initialize `AudioIOProcessConfig` with `AUDIO_IO_PROCESS_CONFIG_INIT`, set
   worker_path, capture/playback ALSA device names, and block size. Leave
   input_file/output_file NULL for hardware. Call start and check its return.
5. Allocate `frames_per_buffer * AUDIO_IO_FRAME_BYTES` bytes. Read frames, invoke
   the team's DSP callback with `(buffer, frames, AUDIO_IO_CHANNELS, context)`, then
   write the same frame count. `AudioProcessCallback` from `audio_io.h` can be
   reused; samples are packed bytes, not float/int32 arrays. Publish control
   parameter updates with appropriate synchronization, without blocking audio.
6. When the owning loop observes a stop flag, call request_capture_stop once,
   then continue processing/forwarding capture until EOF. Call finish and inspect
   both worker results. On transfer/DSP failure or exceptions call abort instead.
   A read timeout may be retried with its retained bytes or treated as an abort
   according to application policy.
7. Coordinate control-thread cancellation and join it separately. The existing
   controller has no public shutdown/unblock API; that production change belongs
   to its owner. Also coordinate the existing Python example's 12-byte assertions:
   the actual packed C++ message and Python `<BBBq` representation are 11 bytes.
   The integration demonstration uses the real C++ definition as-is.

Physical ADC/DAC configuration, clock synchronization, measured latency, signal
quality, long-duration underrun/overrun behavior, and real DSP scheduling still
need hardware verification. Backpressure is bounded by pipe capacity; a stalled
DSP path can overrun capture. Independent ADC/DAC clocks may drift. Neither this
transport nor the original pipeline provides drift correction or real-time
scheduling guarantees.

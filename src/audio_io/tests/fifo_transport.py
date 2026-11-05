"""Hardware-free FIFO checks. Only this test harness creates pipes/launches peers."""
import errno
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import tempfile
import time

WORKER, PASSTHROUGH, MODE = sys.argv[1:]


class Session:
    """Keep test children and FDs together so failed assertions also clean up."""
    def __init__(self, directory):
        self.directory = Path(directory)
        self.children = []
        self.fds = []

    def start(self, *args):
        child = subprocess.Popen(args, stdout=subprocess.DEVNULL)
        self.children.append(child)
        return child

    def fifo(self, name):
        path = self.directory / name
        os.mkfifo(path)
        return str(path)

    def open(self, path, writing=False):
        deadline = time.monotonic() + 3
        while True:
            try:
                fd = os.open(path, (os.O_WRONLY if writing else os.O_RDONLY) | os.O_NONBLOCK)
                self.fds.append(fd)
                return fd
            except OSError as error:
                if error.errno != errno.ENXIO or time.monotonic() >= deadline:
                    raise
                time.sleep(0.01)

    def close(self, fd):
        os.close(fd)
        self.fds.remove(fd)

    def cleanup(self):
        for child in self.children:
            if child.poll() is None:
                child.terminate()
        for fd in self.fds:
            os.close(fd)
        for child in self.children:
            try:
                child.wait(timeout=6)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()


def read_to_eof(fd):
    data = bytearray()
    deadline = time.monotonic() + 4
    while True:
        ready, _, _ = select.select([fd], [], [], max(0, deadline - time.monotonic()))
        assert ready, "capture stalled"
        part = os.read(fd, 5)  # Deliberately split six-byte frames.
        if not part:
            return bytes(data)
        data.extend(part)


def integration(session, frames, data, reverse=False):
    """The middle process stands in for the orchestrator's future PCM loop."""
    name = str(len(session.children))
    capture = session.fifo("capture" + name)
    playback = session.fifo("playback" + name)
    source = session.directory / ("input" + name)
    output = session.directory / ("output" + name)
    source.write_bytes(data)
    commands = [
        [WORKER, "playback", playback, "file", str(output), str(frames)],
        [PASSTHROUGH, capture, playback, str(frames)],
        [WORKER, "capture", capture, "file", str(source), str(frames)],
    ]
    if reverse:
        commands.reverse()
    children = [session.start(*command) for command in commands]
    for child in children:
        assert child.wait(timeout=12) == 0, "pipeline process failed"
    assert output.read_bytes() == data, "PCM bytes changed"


def faults(session):
    fixture = bytes(range(252))  # 42 complete frames, including a short last block.
    integration(session, 17, fixture, reverse=True)
    integration(session, 128, b"")

    # Playback assembles deliberately fragmented writes into frames.
    fifo = session.fifo("fragmented")
    output = session.directory / "fragmented.raw"
    child = session.start(WORKER, "playback", fifo, "file", str(output), "17")
    fd = session.open(fifo, writing=True)
    for byte in fixture:
        assert os.write(fd, bytes([byte])) == 1
    session.close(fd)
    assert child.wait(timeout=3) == 0
    assert output.read_bytes() == fixture

    # Production playback must reject a final partial frame.
    fifo = session.fifo("truncated")
    child = session.start(WORKER, "playback", fifo, "file", str(output))
    fd = session.open(fifo, writing=True)
    os.write(fd, b"1234567")
    session.close(fd)
    assert child.wait(timeout=3) != 0

    # Malformed simulated capture input also fails, rather than padding samples.
    source = session.directory / "bad.raw"
    source.write_bytes(b"1234567")
    fifo = session.fifo("bad-capture")
    fd = session.open(fifo)
    child = session.start(WORKER, "capture", fifo, "file", str(source))
    assert read_to_eof(fd) == b""
    assert child.wait(timeout=3) != 0
    session.close(fd)

    # Missing and regular-file FIFO paths must fail without damaging the file.
    regular = session.directory / "not-a-fifo"
    regular.write_bytes(b"keep me")
    for path in (str(regular), str(session.directory / "missing")):
        child = session.start(WORKER, "capture", path, "file", str(source))
        assert child.wait(timeout=2) != 0
    assert regular.read_bytes() == b"keep me"

    # Both workers can be stopped while still waiting for a FIFO counterpart.
    for mode in ("capture", "playback"):
        fifo = session.fifo("waiting-" + mode)
        child = session.start(WORKER, mode, fifo, "file", str(source))
        time.sleep(0.1)  # Fault injection: give the executable time to reach open().
        child.send_signal(signal.SIGTERM)
        assert child.wait(timeout=2) == 0

    # Playback waiting for data must also honor SIGTERM (writer stays connected).
    fifo = session.fifo("idle")
    child = session.start(WORKER, "playback", fifo, "file", str(output))
    fd = session.open(fifo, writing=True)
    child.terminate()
    assert child.wait(timeout=2) == 0
    session.close(fd)

    # A disconnected capture reader yields EPIPE, not death from SIGPIPE.
    source.write_bytes(fixture * 10000)
    fifo = session.fifo("disconnected")
    fd = session.open(fifo)
    child = session.start(WORKER, "capture", fifo, "file", str(source), "32768")
    assert select.select([fd], [], [], 3)[0]
    assert os.read(fd, 6)
    session.close(fd)
    assert child.wait(timeout=3) == 1

    # Backpressure plus stop is bounded, even if the reader never drains.
    fifo = session.fifo("backpressure")
    fd = session.open(fifo)
    child = session.start(WORKER, "capture", fifo, "file", str(source), "32768")
    assert select.select([fd], [], [], 3)[0]
    child.terminate()
    assert child.wait(timeout=7) == 1  # Cannot complete the block: deadline expires.
    session.close(fd)

    # A silent connected writer times out instead of hanging playback forever.
    fifo = session.fifo("silent-writer")
    child = session.start(WORKER, "playback", fifo, "file", str(output))
    fd = session.open(fifo, writing=True)
    assert child.wait(timeout=7) == 1
    session.close(fd)

    # Missing peers time out without SIGALRM, for both FIFO directions.
    for mode in ("capture", "playback"):
        fifo = session.fifo("no-peer-" + mode)
        endpoint = source if mode == "capture" else output
        child = session.start(WORKER, mode, fifo, "file", str(endpoint))
        assert child.wait(timeout=7) == 1

    # The stand-in also stops cleanly while waiting for its downstream reader.
    capture = session.fifo("stopped-middle-capture")
    playback = session.fifo("stopped-middle-playback")
    child = session.start(PASSTHROUGH, capture, playback)
    time.sleep(0.1)
    child.terminate()
    assert child.wait(timeout=2) == 0

    # Existing ALSA null plugin exercises readi/writei without physical hardware.
    fifo = session.fifo("alsa-capture")
    fd = session.open(fifo)
    child = session.start(WORKER, "capture", fifo, "alsa", "null", "17")
    assert select.select([fd], [], [], 3)[0]
    assert os.read(fd, 6)
    child.terminate()
    read_to_eof(fd)
    assert child.wait(timeout=3) == 0
    session.close(fd)
    fifo = session.fifo("alsa-playback")
    child = session.start(WORKER, "playback", fifo, "alsa", "null", "17")
    fd = session.open(fifo, writing=True)
    os.write(fd, fixture)
    session.close(fd)
    assert child.wait(timeout=3) == 0


with tempfile.TemporaryDirectory(prefix="gatorstomp-fifo-test-") as directory:
    session = Session(directory)
    try:
        if MODE == "faults":
            faults(session)
        else:
            data = bytes((i * 37 + i // 3) % 256 for i in range(32771 * 6))
            integration(session, int(MODE), data)
        print("PASS: FIFO transfers, process exits, and PCM bytes verified")
    finally:
        session.cleanup()

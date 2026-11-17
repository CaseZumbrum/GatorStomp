# Pedal sender for the mock orchestrator listener.
#
# Runs on the PocketBeagle 2 with the GatorStomp controls overlay installed, and
# stands in for the input service with one foot switch and one rotary encoder.
#
# The foot switch and the encoder push are handled by the gpio-keys driver and read
# as input events. This kernel has no rotary-encoder driver, so the encoder's A and B
# pins are read with gpiod and decoded here.
#
# Foot switch presses are sent as BUTTON id 0, and each encoder detent as TWEAK id 0
# with value +1 or -1.
#
# Usage: python3 pedal_sender.py [socket_path]

import fcntl
import glob
import os
import select
import socket
import struct
import sys
import time

import gpiod
from gpiod.line import Direction, Edge, Value

# Mirrors gs_socket_msg in packet_structure.h: type, id, value (signed), time_ms
MSG_FORMAT = "<BBbq"

GS_MSG_BUTTON = 1
GS_MSG_TWEAK = 2

DEFAULT_SOCKET_PATH = "/run/gatorstomp/orchestrator.sock"

# The gpio-keys device takes its name from the overlay node
DEVICE_NAME = "gatorstomp-controls"

# struct input_event on 64 bit Linux: seconds, microseconds, type, code, value
EVENT_FORMAT = "llHHi"
EVENT_SIZE = struct.calcsize(EVENT_FORMAT)

EV_KEY = 1

# Foot switches 0 to 3 are BTN_0 to BTN_3
BTN_0 = 0x100
NUM_SWITCHES = 4

# The encoder push has no message type in the protocol yet, so it is only printed
ENCODER_PUSH = 0x2C0

# EVIOCGKEY asks the kernel which keys are held down right now, as a bitmap
KEY_BITMAP_SIZE = 96
EVIOCGKEY = (2 << 30) | (KEY_BITMAP_SIZE << 16) | (ord("E") << 8) | 0x18

# The main GPIO0 controller, found by label because its gpiochip number can change
GPIO0_LABEL = "600000.gpio"

# Encoder 0 pins on GPIO0
ENCODER_A = 53
ENCODER_B = 49
ENCODER_ID = 0

# Change to -1 if turning the knob clockwise sends -1
DIRECTION = 1

# Quadrature decoding: index is (previous state << 2) | new state, where a state is (A << 1) | B.
# A valid step is +1 or -1, and anything else (no change or a skipped state) is 0.
STEP = [0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0]

# With the pull ups, both contacts are open (A and B high) when the knob rests in a detent
REST_STATE = 0b11


def find_device(name):
    for path in sorted(glob.glob("/sys/class/input/event*/device/name")):
        with open(path) as f:
            if f.read().strip() == name:
                # /sys/class/input/eventN/device/name -> /dev/input/eventN
                return "/dev/input/" + path.split("/")[4]
    raise RuntimeError(f"No input device named {name}. Is the controls overlay loaded?")


def find_chip(label):
    for path in sorted(glob.glob("/dev/gpiochip*")):
        with gpiod.Chip(path) as chip:
            if chip.get_info().label == label:
                return path
    raise RuntimeError(f"No GPIO chip with label {label}")


def pack(msg_type, msg_id, value):
    # The header defines time_ms as time since epoch in ms
    time_ms = time.time_ns() // 1_000_000
    return struct.pack(MSG_FORMAT, msg_type, msg_id, value, time_ms)


def held_keys(fd):
    bitmap = fcntl.ioctl(fd, EVIOCGKEY, bytes(KEY_BITMAP_SIZE))
    return lambda code: 1 if bitmap[code // 8] & (1 << (code % 8)) else 0


def handle_key(sock, fd):
    data = os.read(fd, EVENT_SIZE)
    _, _, event_type, code, value = struct.unpack(EVENT_FORMAT, data)

    # Skip sync events, and key repeats (value 2)
    if event_type != EV_KEY or value > 1:
        return

    if BTN_0 <= code < BTN_0 + NUM_SWITCHES:
        switch_id = code - BTN_0
        sock.send(pack(GS_MSG_BUTTON, switch_id, value))
        print(f"Sent BUTTON id={switch_id} value={value} ({'pressed' if value else 'released'})")
    elif code == ENCODER_PUSH:
        print(f"Encoder {ENCODER_ID} push {'pressed' if value else 'released'} (not sent, no message type yet)")


class Encoder:
    def __init__(self, request):
        self.request = request
        self.a = 1 if request.get_value(ENCODER_A) == Value.ACTIVE else 0
        self.b = 1 if request.get_value(ENCODER_B) == Value.ACTIVE else 0
        self.count = 0

    def handle_edges(self, sock):
        for event in self.request.read_edge_events():
            previous = (self.a << 1) | self.b

            level = 1 if event.event_type == event.Type.RISING_EDGE else 0
            if event.line_offset == ENCODER_A:
                self.a = level
            else:
                self.b = level

            state = (self.a << 1) | self.b
            self.count += STEP[(previous << 2) | state]

            # Contact bounce moves back and forth between two states, so it cancels out in count.
            # One detent is a full cycle of 4 steps, and it is only reported once the knob
            # settles back in the detent.
            if state == REST_STATE:
                if abs(self.count) >= 2:
                    value = DIRECTION if self.count > 0 else -DIRECTION
                    sock.send(pack(GS_MSG_TWEAK, ENCODER_ID, value))
                    print(f"Sent TWEAK id={ENCODER_ID} value={value:+d}")
                self.count = 0


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SOCKET_PATH
    device = find_device(DEVICE_NAME)
    key_fd = os.open(device, os.O_RDONLY)

    settings = gpiod.LineSettings(direction=Direction.INPUT, edge_detection=Edge.BOTH)
    config = {(ENCODER_A, ENCODER_B): settings}

    try:
        with gpiod.request_lines(find_chip(GPIO0_LABEL), consumer="gatorstomp-encoder", config=config) as request:
            with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as sock:
                try:
                    sock.connect(path)
                except OSError as error:
                    print(f"Could not connect to {path}: {error}")
                    print("Is the mock listener running with the same path?")
                    return

                # Like the real input service, report the foot switch state on connect.
                # Only foot switch 0 is wired so far.
                is_held = held_keys(key_fd)
                sock.send(pack(GS_MSG_BUTTON, 0, is_held(BTN_0)))

                encoder = Encoder(request)
                print(f"Connected to {path}, press the foot switch or turn the knob (Ctrl+C to stop)")

                try:
                    while True:
                        # Wait until the foot switch or the encoder has something to read
                        ready, _, _ = select.select([key_fd, request.fd], [], [])
                        if key_fd in ready:
                            handle_key(sock, key_fd)
                        if request.fd in ready:
                            encoder.handle_edges(sock)
                except ConnectionError:
                    print("Listener disconnected")
    finally:
        os.close(key_fd)


if __name__ == "__main__":
    # Ctrl+C can land anywhere, including while the socket is closing, so it is caught out here
    try:
        main()
    except KeyboardInterrupt:
        pass

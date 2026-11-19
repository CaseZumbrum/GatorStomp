# Pedal sender for the mock orchestrator listener.
#
# Runs on the PocketBeagle 2 with the GatorStomp controls overlay installed, and
# stands in for the input service with one foot switch and one rotary encoder.
#
# The foot switch and the encoder push are handled by the gpio-keys driver and read
# as input events. The encoder is read one of two ways, chosen at startup.
# If the kernel has the rotary-encoder driver, the encoder shows up as its own input
# device and its clicks are read as input events. If it does not (the BeagleBoard
# Debian image), the encoder's A and B pins are read with gpiod and decoded here.
#
# Foot switch presses are sent as BUTTON id 0, and each encoder click as TWEAK id 0
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

# Mirrors gs_socket_msg in packet_structure.h: type, id, value (signed), time_ms
MSG_FORMAT = "<BBbq"

GS_MSG_BUTTON = 1
GS_MSG_TWEAK = 2

DEFAULT_SOCKET_PATH = "/run/gatorstomp/orchestrator.sock"

# Input devices take their names from the overlay nodes
CONTROLS_DEVICE = "gatorstomp-controls"
ENCODER_DEVICE = "gatorstomp-encoder-0"

# struct input_event on 64 bit Linux: seconds, microseconds, type, code, value
EVENT_FORMAT = "llHHi"
EVENT_SIZE = struct.calcsize(EVENT_FORMAT)

EV_KEY = 1
EV_REL = 2

# The overlay reports encoder clicks on the REL_DIAL axis
REL_DIAL = 0x07

# Foot switches 0 to 3 are BTN_0 to BTN_3
BTN_0 = 0x100
NUM_SWITCHES = 4

# The encoder push has no message type in the protocol yet, so it is only printed
ENCODER_PUSH = 0x2C0

# EVIOCGKEY asks the kernel which keys are held down right now, as a bitmap
KEY_BITMAP_SIZE = 96
EVIOCGKEY = (2 << 30) | (KEY_BITMAP_SIZE << 16) | (ord("E") << 8) | 0x18

ENCODER_ID = 0

# Only used when decoding with gpiod.
# The main GPIO0 controller, found by label because its gpiochip number can change.
GPIO0_LABEL = "600000.gpio"

# Encoder 0 pins on GPIO0
ENCODER_A = 53
ENCODER_B = 49

# Change to -1 if turning the knob clockwise sends -1 when decoding with gpiod
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
    return None


def pack(msg_type, msg_id, value):
    # The header defines time_ms as time since epoch in ms
    time_ms = time.time_ns() // 1_000_000
    return struct.pack(MSG_FORMAT, msg_type, msg_id, value, time_ms)


def read_event(fd):
    _, _, event_type, code, value = struct.unpack(EVENT_FORMAT, os.read(fd, EVENT_SIZE))
    return event_type, code, value


def held_keys(fd):
    bitmap = fcntl.ioctl(fd, EVIOCGKEY, bytes(KEY_BITMAP_SIZE))
    return lambda code: 1 if bitmap[code // 8] & (1 << (code % 8)) else 0


def send_tweak(sock, value):
    sock.send(pack(GS_MSG_TWEAK, ENCODER_ID, value))
    print(f"Sent TWEAK id={ENCODER_ID} value={value:+d}")


def handle_key(sock, fd):
    event_type, code, value = read_event(fd)

    # Skip sync events, and key repeats (value 2)
    if event_type != EV_KEY or value > 1:
        return

    if BTN_0 <= code < BTN_0 + NUM_SWITCHES:
        switch_id = code - BTN_0
        sock.send(pack(GS_MSG_BUTTON, switch_id, value))
        print(f"Sent BUTTON id={switch_id} value={value} ({'pressed' if value else 'released'})")
    elif code == ENCODER_PUSH:
        print(f"Encoder {ENCODER_ID} push {'pressed' if value else 'released'} (not sent, no message type yet)")


class KernelEncoder:
    # Reads encoder clicks from the rotary-encoder driver's input device

    def __init__(self, device):
        self.fd = os.open(device, os.O_RDONLY)
        self.description = f"read from the rotary-encoder driver ({device})"

    def handle(self, sock):
        event_type, code, value = read_event(self.fd)

        # Skip sync events. The driver reports one event of +1 or -1 per click.
        if event_type == EV_REL and code == REL_DIAL:
            send_tweak(sock, value)

    def close(self):
        os.close(self.fd)


class GpiodEncoder:
    # Reads the encoder's A and B pins with gpiod and decodes the clicks in Python

    def __init__(self):
        # Imported here so gpiod is only needed on kernels without the rotary-encoder driver
        import gpiod
        from gpiod.line import Direction, Edge, Value

        settings = gpiod.LineSettings(direction=Direction.INPUT, edge_detection=Edge.BOTH)
        config = {(ENCODER_A, ENCODER_B): settings}
        self.request = gpiod.request_lines(self.find_chip(gpiod), consumer="gatorstomp-encoder", config=config)

        self.fd = self.request.fd
        self.description = "no rotary-encoder driver, decoding A and B with gpiod"

        self.a = 1 if self.request.get_value(ENCODER_A) == Value.ACTIVE else 0
        self.b = 1 if self.request.get_value(ENCODER_B) == Value.ACTIVE else 0
        self.count = 0

    @staticmethod
    def find_chip(gpiod):
        for path in sorted(glob.glob("/dev/gpiochip*")):
            with gpiod.Chip(path) as chip:
                if chip.get_info().label == GPIO0_LABEL:
                    return path
        raise RuntimeError(f"No GPIO chip with label {GPIO0_LABEL}")

    def handle(self, sock):
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
            # One click is a full cycle of 4 steps, and it is only reported once the knob
            # settles back in the click.
            if state == REST_STATE:
                if abs(self.count) >= 2:
                    send_tweak(sock, DIRECTION if self.count > 0 else -DIRECTION)
                self.count = 0

    def close(self):
        self.request.release()


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SOCKET_PATH

    controls = find_device(CONTROLS_DEVICE)
    if controls is None:
        print(f"No input device named {CONTROLS_DEVICE}. Is the controls overlay loaded?")
        return

    # Use the kernel driver when the encoder has its own input device, otherwise fall back to gpiod
    encoder_device = find_device(ENCODER_DEVICE)
    encoder = KernelEncoder(encoder_device) if encoder_device else GpiodEncoder()

    key_fd = os.open(controls, os.O_RDONLY)

    try:
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

            print(f"Encoder {ENCODER_ID}: {encoder.description}")
            print(f"Connected to {path}, press the foot switch or turn the knob (Ctrl+C to stop)")

            try:
                while True:
                    # Wait until the foot switch or the encoder has something to read
                    ready, _, _ = select.select([key_fd, encoder.fd], [], [])
                    if key_fd in ready:
                        handle_key(sock, key_fd)
                    if encoder.fd in ready:
                        encoder.handle(sock)
            except ConnectionError:
                print("Listener disconnected")
    finally:
        os.close(key_fd)
        encoder.close()


if __name__ == "__main__":
    # Ctrl+C can land anywhere, including while the socket is closing, so it is caught out here
    try:
        main()
    except KeyboardInterrupt:
        pass

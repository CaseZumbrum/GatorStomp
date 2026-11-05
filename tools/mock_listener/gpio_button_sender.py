# GPIO button sender for the mock orchestrator listener.
#
# Runs on the PocketBeagle 2 and turns its USER button into a foot switch.
# Every press and release is sent to the listener as a BUTTON message with id 0.
#
# The USER button drives the BOOTMODE11 and BOOTMODE13 pins through a transistor,
# so it reads high while pressed. BOOTMODE11 is GPMC0_AD11, which is GPIO0_26.
#
# Usage: python3 gpio_button_sender.py [socket_path]

import glob
import socket
import struct
import sys
import time
from datetime import timedelta

import gpiod
from gpiod.line import Direction, Edge, Value

# Mirrors gs_socket_msg in packet_structure.h: type, id, value (signed), time_ms
MSG_FORMAT = "<BBbq"

GS_MSG_BUTTON = 1

DEFAULT_SOCKET_PATH = "/run/gatorstomp/orchestrator.sock"

# The main GPIO0 controller. Its gpiochip number can change between kernels, so it is found by label.
GPIO0_LABEL = "600000.gpio"

# GPIO0_26 is wired to the USER button
USER_BUTTON_LINE = 26

# The foot switch id this button pretends to be
SWITCH_ID = 0


def find_chip(label):
    for path in sorted(glob.glob("/dev/gpiochip*")):
        with gpiod.Chip(path) as chip:
            if chip.get_info().label == label:
                return path
    raise RuntimeError(f"No GPIO chip with label {label}")


def pack(value):
    # The header defines time_ms as time since epoch in ms
    time_ms = time.time_ns() // 1_000_000
    return struct.pack(MSG_FORMAT, GS_MSG_BUTTON, SWITCH_ID, value, time_ms)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SOCKET_PATH

    # Watch both edges, and ignore any bounce shorter than 10 ms
    settings = gpiod.LineSettings(
        direction=Direction.INPUT,
        edge_detection=Edge.BOTH,
        debounce_period=timedelta(milliseconds=10),
    )

    with gpiod.request_lines(find_chip(GPIO0_LABEL), consumer="gatorstomp-button", config={USER_BUTTON_LINE: settings}) as request:
        with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as sock:
            try:
                sock.connect(path)
            except OSError as error:
                print(f"Could not connect to {path}: {error}")
                print("Is the mock listener running with the same path?")
                return

            # Like the real input service, report the button's current state on connect
            pressed = request.get_value(USER_BUTTON_LINE) == Value.ACTIVE
            sock.send(pack(1 if pressed else 0))
            print(f"Connected to {path}, press the USER button (Ctrl+C to stop)")

            try:
                while True:
                    # Blocks until the button changes
                    for event in request.read_edge_events():
                        value = 1 if event.event_type == event.Type.RISING_EDGE else 0
                        sock.send(pack(value))
                        print(f"Sent BUTTON id={SWITCH_ID} value={value} ({'pressed' if value else 'released'})")
            except ConnectionError:
                print("Listener disconnected")


if __name__ == "__main__":
    # Ctrl+C can land anywhere, including while the socket is closing, so it is caught out here
    try:
        main()
    except KeyboardInterrupt:
        pass

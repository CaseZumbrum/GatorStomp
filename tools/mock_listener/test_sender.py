# Test sender for the mock orchestrator listener.
#
# Pretends to be the input service. Connects to the listener's socket, sends a few
# valid encoder and foot switch messages, then sends messages that break the
# protocol so each check in the listener can be seen.
#
# Usage: python3 test_sender.py [socket_path]

import socket
import struct
import sys
import time

# Mirrors gs_socket_msg in packet_structure.h: type, id, value (signed), time_ms
MSG_FORMAT = "<BBbq"

GS_MSG_ACK = 0
GS_MSG_BUTTON = 1
GS_MSG_TWEAK = 2
GS_MSG_VOLUME = 3

DEFAULT_SOCKET_PATH = "/run/gatorstomp/orchestrator.sock"


def pack(msg_type, msg_id, value):
    # The header defines time_ms as time since epoch in ms
    time_ms = time.time_ns() // 1_000_000
    return struct.pack(MSG_FORMAT, msg_type, msg_id, value, time_ms)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SOCKET_PATH

    # Each message is built inside a lambda so its timestamp is taken when it is sent
    messages = [
        ("encoder 2 turned down one step", lambda: pack(GS_MSG_TWEAK, 2, -1)),
        ("encoder 0 turned up five steps", lambda: pack(GS_MSG_TWEAK, 0, 5)),
        ("foot switch 1 pressed", lambda: pack(GS_MSG_BUTTON, 1, 1)),
        ("foot switch 1 released", lambda: pack(GS_MSG_BUTTON, 1, 0)),
        ("VOLUME, not expected from the input service", lambda: pack(GS_MSG_VOLUME, 0, 10)),
        ("ACK, not expected from the input service", lambda: pack(GS_MSG_ACK, 0, 0)),
        ("unknown type 9", lambda: pack(9, 0, 0)),
        ("foot switch with id 7", lambda: pack(GS_MSG_BUTTON, 7, 1)),
        ("foot switch with value 2", lambda: pack(GS_MSG_BUTTON, 1, 2)),
        ("message that is too short", lambda: b"\x01\x02\x03"),
        ("message that is too long", lambda: pack(GS_MSG_TWEAK, 1, 1) + b"\x00"),
    ]

    with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as sock:
        sock.connect(path)

        for description, build in messages:
            data = build()
            print(f"Sending {description} ({len(data)} bytes)")
            sock.send(data)

            # Short pause so the listener output is easy to follow
            time.sleep(0.2)


if __name__ == "__main__":
    main()

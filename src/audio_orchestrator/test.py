# Simple example for sending data to a socket using the gs_socket_api byte structure.

import socket
import struct
import time

SOCKET_PATH = "/tmp/test.sock"

GS_MSG_ACK = 0
GS_MSG_BUTTON = 1
GS_MSG_TWEAK = 2
GS_MSG_VOLUME = 3
GS_MSG_UNUSED = 4

MSG_FORMAT = "<BBhq"
MSG_SIZE = struct.calcsize(MSG_FORMAT)

assert MSG_SIZE == 12


def send_message(sock, msg_type, msg_id, value, time_ms):

    data = struct.pack(
        MSG_FORMAT,
        msg_type,
        msg_id,
        value,
        time_ms,
    )

    assert len(data) == 12
    sock.sendall(data)

    print(f"Sent {len(data)} bytes: {data.hex()}")


def main():
    with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as sock:
        sock.connect(SOCKET_PATH)

        send_message(
            sock, GS_MSG_BUTTON, msg_id=4, value=2, time_ms=time.time_ns() // 1_000_000
        )


if __name__ == "__main__":
    main()

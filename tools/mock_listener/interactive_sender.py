# Interactive sender for the mock orchestrator listener.
#
# Turns the keyboard into a pretend pedal for live demos. The arrow keys turn the
# encoders and the number keys press and release the foot switches. Every action
# is sent to the listener as a gs_socket_msg.
#
# Usage: python3 interactive_sender.py [socket_path]

import curses
import socket
import struct
import sys
import time

# Mirrors gs_socket_msg in packet_structure.h: type, id, value (signed), time_ms
MSG_FORMAT = "<BBbq"

GS_MSG_BUTTON = 1
GS_MSG_TWEAK = 2

DEFAULT_SOCKET_PATH = "/run/gatorstomp/orchestrator.sock"

# The pedal has 4 encoders and 4 foot switches, numbered 0 to 3
NUM_CONTROLS = 4

HELP = [
    "Left and Right: turn the selected encoder",
    "Up and Down: choose which encoder is selected",
    "1 to 4: press or release foot switches 0 to 3",
    "b: send a broken message",
    "q: quit",
]


def pack(msg_type, msg_id, value):
    # The header defines time_ms as time since epoch in ms
    time_ms = time.time_ns() // 1_000_000
    return struct.pack(MSG_FORMAT, msg_type, msg_id, value, time_ms)


def draw(screen, path, encoder, switches, last_sent):
    screen.erase()
    screen.addstr(0, 0, "GatorStomp interactive sender")
    screen.addstr(1, 0, f"Connected to {path}")

    screen.addstr(3, 0, f"Selected encoder: {encoder}")
    for switch_id, state in enumerate(switches):
        name = "pressed" if state == 1 else "released"
        screen.addstr(4 + switch_id, 0, f"Foot switch {switch_id} (key {switch_id + 1}): {name}")

    screen.addstr(9, 0, f"Last sent: {last_sent}")

    for line_number, line in enumerate(HELP):
        screen.addstr(11 + line_number, 0, line)

    screen.refresh()


def run(screen, sock, path):
    # Hide the blinking cursor if the terminal allows it
    try:
        curses.curs_set(0)
    except curses.error:
        pass

    encoder = 0
    switches = [0] * NUM_CONTROLS

    # Like the real input service, report the state of every foot switch on connect
    for switch_id in range(NUM_CONTROLS):
        sock.send(pack(GS_MSG_BUTTON, switch_id, 0))
    last_sent = "all 4 foot switches released (initial state)"

    while True:
        draw(screen, path, encoder, switches, last_sent)
        key = screen.getch()

        if key == ord("q"):
            return

        # Up and Down only change the selection, so nothing is sent
        if key == curses.KEY_UP:
            encoder = (encoder + 1) % NUM_CONTROLS
            continue
        if key == curses.KEY_DOWN:
            encoder = (encoder - 1) % NUM_CONTROLS
            continue

        if key == curses.KEY_LEFT:
            data = pack(GS_MSG_TWEAK, encoder, -1)
            last_sent = f"TWEAK id={encoder} value=-1"
        elif key == curses.KEY_RIGHT:
            data = pack(GS_MSG_TWEAK, encoder, 1)
            last_sent = f"TWEAK id={encoder} value=1"
        elif ord("1") <= key <= ord("4"):
            # A terminal can't tell when a key is let go, so each press toggles the switch
            switch_id = key - ord("1")
            switches[switch_id] = 1 - switches[switch_id]
            data = pack(GS_MSG_BUTTON, switch_id, switches[switch_id])
            last_sent = f"BUTTON id={switch_id} value={switches[switch_id]}"
        elif key == ord("b"):
            data = b"\x01\x02\x03"
            last_sent = "a broken 3 byte message"
        else:
            continue

        sock.send(data)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SOCKET_PATH

    with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as sock:
        try:
            sock.connect(path)
        except OSError as error:
            print(f"Could not connect to {path}: {error}")
            print("Is the mock listener running with the same path?")
            return

        # curses.wrapper sets up the terminal and puts it back to normal when run returns or fails
        try:
            curses.wrapper(run, sock, path)
        except ConnectionError:
            print("Listener disconnected")
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()

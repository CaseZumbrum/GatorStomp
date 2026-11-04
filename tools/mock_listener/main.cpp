/**
 * @file main.cpp
 * @brief Mock orchestrator listener. Stands in for the audio orchestrator's I/O handler so the input service can be tested before the real one exists.
 * @details Accepts one client at a time on a Unix socket and prints every gs_socket_msg it receives. Messages that break the protocol are logged and the listener keeps running.
 * @author Shan Sundal
 */

#include <packet_structure.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>

/** @brief Folder that holds the default socket path, created at startup if it is missing */
static const char* DEFAULT_SOCKET_DIR = "/run/gatorstomp";

/** @brief Path of the socket file, global so the Ctrl+C handler can remove it */
static const char* socket_path = GS_SOCKET_PATH;

/**
 * @brief Runs on Ctrl+C. Removes the socket file and exits.
 * @details unlink and _exit are used because they are safe to call from inside a signal handler.
 */
static void handle_sigint(int)
{
    unlink(socket_path);
    _exit(0);
}

/**
 * @brief Returns a readable name for a message type.
 * @param type the type field of a message
 */
static const char* type_name(int type)
{
    switch (type) {
        case GS_MSG_ACK: return "ACK";
        case GS_MSG_BUTTON: return "BUTTON";
        case GS_MSG_TWEAK: return "TWEAK";
        case GS_MSG_VOLUME: return "VOLUME";
        case GS_MSG_UNUSED: return "UNUSED";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Checks a message against the protocol and prints one line describing it.
 * @param msg the message that was received
 */
static void print_msg(const gs_socket_msg& msg)
{
    long long time = msg.time_ms;
    int type = msg.type;
    int id = msg.id;
    int value = msg.value;

    // The input service should only ever send BUTTON and TWEAK messages
    if (type != GS_MSG_BUTTON && type != GS_MSG_TWEAK) {
        printf("[%lld ms] PROTOCOL ERROR: type %d (%s) is not expected from the input service\n", time, type, type_name(type));
        return;
    }

    // Foot switches and encoders are numbered 0 to 3
    if (id > 3) {
        printf("[%lld ms] PROTOCOL ERROR: %s id %d is outside 0 to 3\n", time, type_name(type), id);
        return;
    }

    // TWEAK values are relative encoder steps, so any int8_t value is allowed
    if (type == GS_MSG_TWEAK) {
        printf("[%lld ms] TWEAK   id=%d  value=%d\n", time, id, value);
        return;
    }

    // BUTTON values are 1 for pressed and 0 for released
    if (value == 1) {
        printf("[%lld ms] BUTTON  id=%d  value=1 (pressed)\n", time, id);
    } else if (value == 0) {
        printf("[%lld ms] BUTTON  id=%d  value=0 (released)\n", time, id);
    } else {
        printf("[%lld ms] PROTOCOL ERROR: BUTTON value %d should be 0 or 1\n", time, value);
    }
}

int main(int argc, char* argv[])
{
    // Print each line right away, even when the output is piped to a file
    setvbuf(stdout, nullptr, _IOLBF, 0);

    // An optional argument overrides the socket path, for example ./mock_listener /tmp/gs.sock
    if (argc > 1) {
        socket_path = argv[1];
    } else if (mkdir(DEFAULT_SOCKET_DIR, 0755) == -1 && errno != EEXIST) {
        // The default folder usually needs root on a dev machine
        fprintf(stderr, "Could not create %s: %s\n", DEFAULT_SOCKET_DIR, strerror(errno));
        fprintf(stderr, "Try passing a path instead: %s /tmp/gs.sock\n", argv[0]);
        return 1;
    }

    // A Unix socket address is just a file path, which has a fixed maximum length
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (strlen(socket_path) >= sizeof(addr.sun_path)) {
        fprintf(stderr, "Socket path is too long: %s\n", socket_path);
        return 1;
    }
    strcpy(addr.sun_path, socket_path);

    // Remove a socket file left behind by an earlier run, otherwise bind fails
    // ENOENT only means there was no old file to remove
    if (unlink(socket_path) == -1 && errno != ENOENT) {
        fprintf(stderr, "Could not remove old socket %s: %s\n", socket_path, strerror(errno));
        return 1;
    }

    // SOCK_SEQPACKET keeps message boundaries, so one send on the client is exactly one recv here
    int listen_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (listen_fd == -1) {
        fprintf(stderr, "socket failed: %s\n", strerror(errno));
        return 1;
    }

    if (bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        fprintf(stderr, "bind failed: %s\n", strerror(errno));
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 1) == -1) {
        fprintf(stderr, "listen failed: %s\n", strerror(errno));
        close(listen_fd);
        unlink(socket_path);
        return 1;
    }

    // Clean up the socket file on Ctrl+C now that it exists
    signal(SIGINT, handle_sigint);

    printf("Mock orchestrator listening on %s (Ctrl+C to stop)\n", socket_path);
    printf("Expecting %zu byte messages\n", sizeof(gs_socket_msg));

    // Serve one client at a time, and go back to waiting whenever a client leaves
    while (true) {
        printf("Waiting for a client...\n");

        int client_fd = accept(listen_fd, nullptr, nullptr);
        if (client_fd == -1) {
            fprintf(stderr, "accept failed: %s\n", strerror(errno));
            continue;
        }
        printf("Client connected\n");

        while (true) {
            gs_socket_msg msg;

            // MSG_TRUNC makes recv return the real size of the message even when it is bigger than msg
            // Without it, an oversized message would be cut down to fit and look valid
            ssize_t size = recv(client_fd, &msg, sizeof(msg), MSG_TRUNC);

            if (size == -1) {
                fprintf(stderr, "recv failed: %s\n", strerror(errno));
                break;
            }

            // recv returns 0 when the client closes its end of the connection
            if (size == 0) {
                printf("Client disconnected\n");
                break;
            }

            if (size != sizeof(msg)) {
                printf("PROTOCOL ERROR: got a %zd byte message, expected %zu bytes\n", size, sizeof(msg));
                continue;
            }

            print_msg(msg);
        }

        close(client_fd);
    }
}

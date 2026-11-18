/**
 * @file io_controller.cpp
 * @brief Implementation of the IO_Controller class
 * @todo add a thread manager
 * @author Case Zumbrum
 */

#include "io_controller.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <cerrno>
#include <stdexcept>
#include <algorithm>

IO_Controller::IO_Controller(std::string socket_path, message_handler_t message_handler=nullptr) {
    this->socket_path = socket_path;

    this->message_handler = message_handler;

    fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);

    if (fd == -1) {
        throw std::runtime_error("Failed to open socket");
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;

    if (socket_path.size() >= sizeof(addr.sun_path)) {
        close(fd);
        throw std::runtime_error("Socket path too long");
    }

    std::strncpy(addr.sun_path, socket_path.c_str(),
                 sizeof(addr.sun_path) - 1);

    unlink(socket_path.c_str());

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr)) == -1) {
        close(fd);
        throw std::runtime_error("Failed to bind socket");
    }

    if (listen(fd, 10) == -1) {
        close(fd);
        unlink(socket_path.c_str());
        throw std::runtime_error("Failed to listen");
    }
}

void IO_Controller::start() {
    if (running.exchange(true)) {
        return;
    }

    accept_thread = std::thread(
        &IO_Controller::accept_clients, this
    );
}

void IO_Controller::accept_clients() {

    while (running) {

        int client_fd = accept(fd, nullptr, nullptr);

        if (client_fd == -1) {
            if (!running) break;

            if (errno == EINTR) continue;

            perror("accept");
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(clients_mutex);

            clients.push_back(client_fd);

            client_threads.emplace_back(
                &IO_Controller::handle_client,
                this,
                client_fd
            );
        }

        std::cout << "Client connected: "
                  << client_fd << std::endl;
    }
}

void IO_Controller::handle_client(int client_fd) {

    gs_socket_msg msg{};

    while (running) {

        ssize_t bytes = recv(
            client_fd,
            &msg,
            sizeof(msg),
            0
        );

        if (bytes <= 0) {
            if (bytes == -1 && errno == EINTR) {
                continue;
            }

            break;
        }

        if (bytes != sizeof(gs_socket_msg)) {
            std::cerr << "Invalid packet size from client "
                      << client_fd << std::endl;
            continue;
        }

        std::cout << "Received packet from client " << client_fd << " at " << msg.time_ms << std::endl;
        // Process received message here
        if(this->message_handler){
            this->message_handler(msg);
        }

        
    }

    close(client_fd);

    {
        std::lock_guard<std::mutex> lock(clients_mutex);

        clients.erase(
            std::remove(clients.begin(), clients.end(), client_fd),
            clients.end()
        );
    }

    std::cout << "Client disconnected: " << client_fd << std::endl;
}

bool IO_Controller::send_msg(
    gs_socket_msg* msg,
    int client_index
) {
    std::lock_guard<std::mutex> lock(clients_mutex);

    if (client_index < 0 ||
        client_index >= static_cast<int>(clients.size())) {
        return false;
    }

    ssize_t bytes = send(
        clients[client_index],
        msg,
        sizeof(gs_socket_msg),
        MSG_NOSIGNAL
    );

    return bytes == sizeof(gs_socket_msg);
}

void IO_Controller::stop() {

    if (!running.exchange(false)) {
        return;
    }

    // Unblock accept()
    shutdown(fd, SHUT_RDWR);
    close(fd);
    fd = -1;

    if (accept_thread.joinable()) {
        accept_thread.join();
    }

    {
        std::lock_guard<std::mutex> lock(clients_mutex);

        for (int client : clients) {
            shutdown(client, SHUT_RDWR);
        }
    }

    for (auto& thread : client_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    unlink(socket_path.c_str());
}

IO_Controller::~IO_Controller() {
    stop();
}
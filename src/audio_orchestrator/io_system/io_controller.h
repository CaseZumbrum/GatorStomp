/**
 * @file io_controller.h
 * @brief Definition of the IO_Controller class, which is used by the audio orchestrator to communicate with peripheral device processes across a UNIX socket.
 * @todo currently only works on a single client connection, needs to be updated to accept connections during operation.
 * @author Case Zumbrum
 */

#pragma once

#include <packet_structure.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

typedef std::function<bool(gs_socket_msg)> message_handler_t;

class IO_Controller {
public:
    explicit IO_Controller(std::string socket_path, message_handler_t message_handler);
    ~IO_Controller();

    bool send_msg(gs_socket_msg* msg, int client_index);

    void start();
    void stop();

private:
    std::string socket_path;

    int fd = -1;

    std::vector<int> clients;
    std::vector<std::thread> client_threads;

    std::thread accept_thread;

    std::mutex clients_mutex;
    std::atomic<bool> running{false};

    message_handler_t message_handler;

    void accept_clients();
    void handle_client(int client_fd);
};
/**
 * @file io_controller.h
 * @brief Definition of the IO_Controller class, which is used by the audio orchestrator to communicate with peripheral device processes across a UNIX socket.
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

/** @brief class for a gs_socket_message handler */
typedef std::function<bool(gs_socket_msg)> message_handler_t;

/** @brief Class for the socket API of the audio orchestrator. Does not handle messages, but accepts a callback for them*/
class IO_Controller {
public:
    /**
     * @brief constructor
     * @param socket_path path of the socket to open
     * @param message_handler callback to call when a successful message is received
     */
    explicit IO_Controller(std::string socket_path, message_handler_t message_handler);
    /**
     * @brief destructor, kills the socket and all threads
     */
    ~IO_Controller();

    /**
     * @brief sends a message to a specific client
     * @param msg the message to send
     * @param client_index the index of the client
     * @todo rework this to not use indices
     */
    bool send_msg(gs_socket_msg* msg, int client_index);

    /** @brief starts the socket api */
    void start();
    /** @brief stops the socket api, killing all active connections */
    void stop();

private:
    /** @brief path of the socket */
    std::string socket_path;

    /** @brief file descriptor for the socket */
    int fd = -1;

    /** @brief vector of client file_descriptors */
    std::vector<int> clients;
    /** @brief vector of client threads (each client get's its own thread for recieving) */
    std::vector<std::thread> client_threads;

    /** @brief thread used for accepting new clients */
    std::thread accept_thread;

    /** @brief mutex used when accepting or removing clients */
    std::mutex clients_mutex;
    /** @brief used to stop running threads */
    std::atomic<bool> running{false};

    /** @brief handler to be called when a message is recieved */
    message_handler_t message_handler;

    /** @brief thread entry point for accepting clients */
    void accept_clients();
    /** 
     * @brief thread entry point for each individual client
     * @param client_fd the file descriptor for the specific client being handled
     */
    void handle_client(int client_fd);
    /** @brief thread that removes old client threads
     * @todo implement me
     */
    void thread_garbage_collector();
};
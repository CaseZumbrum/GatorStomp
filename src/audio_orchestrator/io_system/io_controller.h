/**
 * @file io_controller.h
 * @brief Definition of the IO_Controller class, which is used by the audio orchestrator to communicate with peripheral device processes across a UNIX socket.
 * @todo currently only works on a single client connection, needs to be updated to accept connections during operation.
 * @author Case Zumbrum
 */

#pragma once
#include <packet_structure.h>

#include <string>
#include <array>
#include <vector>
#include <sys/socket.h>
#include <sys/un.h>

#define BUFFER_SIZE 128

/**
 * @brief Class for the IO_Controller, creates and binds to a unix socket
 */
class IO_Controller{
    private:
        /** @brief the path to the socket being used, defaults to the GS_SOCKET_PATH */
        std::string socket_path = GS_SOCKET_PATH;
        /** @brief buffer for input messages */
        std::array<gs_socket_msg, BUFFER_SIZE> input_buffer;
        /** @brief buffer for output messages */
        std::array<gs_socket_msg, BUFFER_SIZE> output_buffer;

        /** @brief vector of connected clients */
        std::vector<int> clients;

        /** @brief main file descriptor for the socket */
        int fd;
        /** @brief address for the socket */
        sockaddr_un addr{};

    public:
        /** 
         * @brief instantiates an IO_Controller and binds to the socket
         * @details currently awaits a single client connection
         * @param socket_path path to the socket
         */
        IO_Controller(std::string socket_path);
        /**
         * @brief destructor, disconnects all clients and un-binds the socket
         */
        ~IO_Controller();
        /**
         * @brief sends a message to a specific client index
         * @param msg pointer to the message being sent
         * @param client_index index of the client in the clients vector
         * @return true on success, false on failure
         */
        bool send_msg(gs_socket_msg* msg, int client_index);
        /**
         * @brief receive a message from a specific client index
         * @param msg pointer to the message being received (will be overwritten)
         * @param client_index index of the client in the clients vector
         * @return true on success, false on failure
         */
        bool recv_msg(gs_socket_msg* msg, int client_index);
};
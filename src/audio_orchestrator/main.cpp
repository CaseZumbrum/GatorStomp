/**
 * @file main.cpp
 * @brief A simple calculator example for testing the audio orchestrator I/O structure. Sets up and receives a single message.
 * @author Case Zumbrum
 */

#include <iostream>
#include <packet_structure.h>
#include "io_system/io_controller.h"

int main(){
    IO_Controller c("/tmp/test.sock");
    gs_socket_msg msg;
    if(c.recv_msg(&msg, 0)){
        std::cout << "Type: " << +msg.type << std::endl << "ID: " << +msg.id << std::endl << "Value: " << msg.value << std::endl << "Timestamp " << std::hex << msg.time_ms << std::endl;
    }
    else{
        std::cout << "Error on recv" << std::endl;
    }
    return 0;
}
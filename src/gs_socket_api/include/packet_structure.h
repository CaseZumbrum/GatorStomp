/**
 * @file packet_structure.h
 * @brief Definition of the gs_socket_msg struct, which is sent across a Unix socket for communication between the audio orchestrator and peripherals.
 * @author Case Zumbrum
 */

#pragma once
#include <stdint.h>

#define GS_SOCKET_PATH "/run/gatorstomp/orchestrator.sock"   /* proposed */

/**
 * @brief Enum for the message type of a socket message, used to control which kind of message is being sent.
 */
enum gs_msg_type : uint8_t {
    /** @brief acknowledgement message */
    GS_MSG_ACK    = 0,
    /** @brief button press related message */
    GS_MSG_BUTTON = 1,
    /** @brief tweaking an effect parameter related message */
    GS_MSG_TWEAK = 2,
    /** @brief system volume related message message */
    GS_MSG_VOLUME = 3,
    /** @brief additional message type, not currently in use */
    GS_MSG_UNUSED = 4
};

/**
 *  @brief Struct for messages being sent to/from the audio orchestrator api
 */
struct [[gnu::packed]] gs_socket_msg {
    /** @brief the kind of message being sent */
    gs_msg_type type;
    /** @brief which peripheral is sending this. 0->3 for GS_MSG_BUTTON and GS_MSG_TWEAK, unused for others */
    uint8_t id;
    /** @brief Value for the event, 0->1 for GS_MSG_BUTTON, -254->255 for GS_MSG_TWEAK and GS_MSG_VOLUME, unused for others*/
    int16_t value;
    /** @brief time since epoch in ms */
    int64_t time_ms;
};

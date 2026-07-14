#pragma once

#include "lora_receiver.h"
#include "lora_config.h"
#include "lora_custom_event.h"
#include "bt_transmitter.h"
#include <furi.h>

#define MAX_DATA_SIZE (1 << 8) // 256 bytes

#define MIN_SF (7)
#define MAX_SF (12)

#define MIN_POWER (0)
#define MAX_POWER (22)

#define MIN_PREAMBLE (6) // For RX and TX preamble
#define MAX_PREAMBLE (20) // For RX and TX preamble

#define BANDWIDTH_LIST_SIZE (3) // Number of bandwidth options (125, 250, 500 kHz)
#define CANAL_LIST_SIZE     (3) // Number of canal options (868.1, 868.3, 868.5 MHz)

typedef struct {
    uint8_t margin; // Link margin in dB (0-254) from the last LinkCheckReq.
    uint16_t gateway_count; // Number of gateways that received the last transmitted frame.
    uint8_t rx_window; // RX window number where the last frame was received.
    int8_t rssi; // RSSI (Received Signal Strength Indicator) of the last frame.
    int8_t snr; // SNR (Signal-to-Noise Ratio) of the last frame.
    uint8_t port; // Port number used in the last transmission.
    char data[MAX_DATA_SIZE]; // Payload data of the last transmitted frame.
    char decoded_data[MAX_DATA_SIZE]; // Decoded data of the last transmitted frame.
    bool is_multicast; // True if the frame was received in a multicast group.
    bool is_pending; // True if the server has pending data for the device.
    bool is_ack; // True if the last frame was acknowledged by the server.
} LoraMsgResponseModel;

/**
 * @brief LoRa Receiver Model
 */
typedef struct {
    LoraMsgResponseModel msg_response; // Model for the LoRa message response
    LoraConfigModel config; // Model for the LoRa configuration
} LoraReceiverModel;

/**
 * @brief LoRa receiver Object
 */
struct LoraReceiver {
    View* view;
    void* context;
    LoraReceiverViewCallbak view_callback; // Callback function to be called on view events
    LoraReceiverProcessCallback process_callback; // Callback function for received messages
    LoraStateManager* state_manager;
};

typedef enum {
    LoraPacketFpending, // +{MSG, CMSG}: FPENDING
    LoraPacketLinkInfo, // +{MSG, CMSG}: Link <margin>,<gateway_count>
    LoraPacketRxwinInfo, // +{MSG, CMSG, TEST}: RXWIN<rx_window>, RSSI <rssi>, SNR <snr>
    LoraPacketAck, // +{MSG, CMSG}: ACK Received
    LoraPacketMulticast, // +{MSG, CMSG}: MULTICAST
    LoraPacketRxPacket, // +{MSG, TEST} PORT: <port>; RX: "<hex data>"
} LoraPacketType;

// DEBUG MACRO
#define DEBUG_LORA_MSG_RESPONSE(lora_receiver)                                                   \
    FURI_LOG_D("DebugMsgResponse", "LoRaMsgResponseModel Debug:");                               \
    FURI_LOG_D("DebugMsgResponse", "  Port: %hhu", (lora_receiver->model->msg_response).port);   \
    FURI_LOG_D("DebugMsgResponse", "  Data: %s", (lora_receiver->model->msg_response).data);     \
    FURI_LOG_D(                                                                                  \
        "DebugMsgResponse", "  Margin: %hhu", (lora_receiver->model->msg_response).margin);      \
    FURI_LOG_D(                                                                                  \
        "DebugMsgResponse",                                                                      \
        "  Gateway Count: %u",                                                                   \
        (lora_receiver->model->msg_response).gateway_count);                                     \
    FURI_LOG_D(                                                                                  \
        "DebugMsgResponse", "  RX Window: %d", (lora_receiver->model->msg_response).rx_window);  \
    FURI_LOG_D("DebugMsgResponse", "  RSSI: %d dBm", (lora_receiver->model->msg_response).rssi); \
    FURI_LOG_D("DebugMsgResponse", "  SNR: %d dB", (lora_receiver->model->msg_response).snr);    \
    FURI_LOG_D(                                                                                  \
        "DebugMsgResponse",                                                                      \
        "  Multicast: %s",                                                                       \
        (lora_receiver->model->msg_response).is_multicast ? "Yes" : "No");                       \
    FURI_LOG_D(                                                                                  \
        "DebugMsgResponse",                                                                      \
        "  Pending: %s",                                                                         \
        (lora_receiver->model->msg_response).is_pending ? "Yes" : "No");                         \
    FURI_LOG_D(                                                                                  \
        "DebugMsgResponse",                                                                      \
        "  ACK Received: %s",                                                                    \
        (lora_receiver->model->msg_response).is_ack ? "Yes" : "No");

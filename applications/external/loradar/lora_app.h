#ifndef UART_DEMO_H
#define UART_DEMO_H

#include <expansion/expansion.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/variable_item_list.h>

#include <gui/canvas.h>
#include <input/input.h>

#include "lora_state_manager.h"
#include "helpers/uart_helper.h"
#include "lora_receiver.h"
#include "lora_transmitter.h"
#include "bt_transmitter.h"
#include "lora_custom_event.h"

#define DEVICE_BAUDRATE 9600

// Comment out the following line to process data as it is received.
#define DEMO_PROCESS_LINE
#define LINE_DELIMITER         '\n'
#define INCLUDE_LINE_DELIMITER false

typedef enum {
    DEFAULT_CMD = 0,
    MSG_CMD = 1,
    CMSG_CMD = 2,
    JOIN_CMD = 3,
    CONFIG_CMD = 4,
    CMD_TYPE_COUNT,
} LoraCmdType;

typedef struct {
    Gui* gui;
    FuriTimer* timer;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    LoraStateManager* state_manager;
    Submenu* submenu;
    VariableItemList* var_item_list;
    uint32_t index;
    LoraReceiver* receiver;
    LoraTransmitter* transmitter;
    BtTransmitter* bt_transmitter;
    LoraState current_state;
} LoraApp;

typedef enum {
    LoraAppSubMenuView,
    LoraAppReceiverView,
    LoraAppReceiverCfgView,
} LoraAppView;

// Callback to handle UART responses
// TODO: remove this
void handle_default_response(FuriString* line, void* context);
void lora_receiver_default_response_callback(FuriString* line, void* context);
void handle_join_response(FuriString* line, void* context);

/**
 * @brief Handles the reception of a LoRa packet in TEST mode (P2P LoRa).
 *
 * This function is triggered on the receiver side when a LoRa packet is 
 * received from the transmitter during TEST mode operation. It decodes the
 * data and updates the LoRaMsgResponseModel structure with the received data.
 */
void lora_receiver_rx_response_callback(FuriString* line, void* context);

// TODO: is this needed?
extern LoraState lora_app_state;

// TODO: remove this
// void lora_enter_receive_mode(void *context);
// void send_cmsg(LoraApp * app, const char *msg);
// void lora_transmitter_setup_lorawan(void *context);
// void otaa_join_procedure(void *context);

#endif

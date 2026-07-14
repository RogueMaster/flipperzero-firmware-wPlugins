#pragma once

#include <gui/view.h>

#include "lora_state_manager.h"
#include "lora_custom_event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LoraReceiver LoraReceiver;

/**
 * @brief Callback function called when the LoRa receiver receives a message.
 * @param receiver Pointer to the LoraReceiver object.
 * @param line Pointer to the received data.
 */
typedef void (*LoraReceiverProcessCallback)(FuriString* line, void* context);

typedef void (*LoraReceiverViewCallbak)(LoraCustomEvent event, void* context);

/**
 * @brief Allocate and initialize a LoraReceiver object.
 * @return Pointer to the allocated LoraReceiver object.
 */
LoraReceiver* lora_receiver_alloc(void);

/**
 * @brief Free the memory allocated for a LoraReceiver object.
 * @param receiver Pointer to the LoraReceiver object to be freed.
 */
void lora_receiver_free(LoraReceiver* receiver);

/**
 * @brief Get the view associated with the LoraReceiver object.
 * @param receiver Pointer to the LoraReceiver object.
 * @return Pointer to the View associated with the LoraReceiver object.
 */
View* lora_receiver_get_view(LoraReceiver* receiver);

// METHODS
void lora_receiver_set_data_msg_response(LoraReceiver* receiver, uint8_t* data, uint32_t size);

/**
 * @brief Set the data message response and update the view.
 * @param receiver Pointer to the LoraReceiver object.
 * @param line string to be decoded.
 */
void lora_receiver_decode_msg_response(void* context, FuriString* line);

/**
 * @brief Update process callback that will be called when we receive a frame on serial port.
 * @param receiver Pointer to the LoraReceiver object.
 * @param state The current state of the LoRa receiver.
 * @param context Pointer to the context object.
 * @return void 
 */
void lora_receiver_update_process_callback(LoraReceiver* receiver, LoraState state);

LoraReceiverProcessCallback lora_receiver_get_callback(LoraReceiver* receiver);

void lora_receiver_set_view_callback(
    LoraReceiver* receiver,
    LoraReceiverViewCallbak callback,
    void* context);

void lora_receiver_set_state_manager(LoraReceiver* receiver, LoraStateManager* state_manager);
#ifdef __cplusplus
}
#endif

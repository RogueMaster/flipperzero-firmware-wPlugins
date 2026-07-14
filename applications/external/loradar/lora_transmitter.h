#pragma once

#include "lora_state_manager.h"
#include "lora_config.h"
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    TransmitterEventEnterReceiveMode = 1 << 0,
    TransmitterEventSetRFTestConfig = 1 << 1,
    TransmitterEventExciting = 1 << 2,
    TransmitterEventResponseReceived = 1 << 3,

} TransmitterEventFlags;

typedef struct LoraTransmitter LoraTransmitter;

/**
 * @brief Callback function for sending data
 * @param context Pointer to the object that will send msg (e.g. UART helper).
 * @param data Pointer to the data to be sent.
 * @param length Length of the data to be sent.
 */
typedef void (
    *LoraTransmitterMethod)(void* context, const char* data, size_t length, bool wait_response);

/**
 * @brief Context Destructor
 * @param context Pointer to the object that will be freed.
 */
typedef void (*LoraTransmitterContextDestructor)(void* context);

typedef void (*SetTransmitterThreadIdMethod)(void* context, FuriThreadId thread_id);
/**
 * @brief LoRa Transmitter Constructor
 * @param context Pointer to the object that will send msg (e.g. UART helper).
 * @param send_method Pointer to the function that will send the data.
 * @param context_destructor Pointer to the function that will free the context.
 * 
 * @return 
 */
LoraTransmitter* lora_transmitter_alloc(
    void* context,
    LoraTransmitterMethod send_method,
    LoraTransmitterContextDestructor context_destructor,
    SetTransmitterThreadIdMethod set_thread_id_method);

/**
 * @brief LoRa Transmitter Destructor
 * @param transmitter Pointer to the LoraTransmitter object.
 */
void lora_transmitter_free(LoraTransmitter* transmitter);

/**
 * @brief Enter in receive mode (LoRa P2P)
 * @param transmitter Pointer to the LoraTransmitter object.
 */
void lora_transmitter_enter_receive_mode(LoraTransmitter* transmitter);

/**
 * @brief Do the OTAA join procedure (LoRaWAN)
 * @param transmitter Pointer to the LoraTransmitter object.
 */
void lora_transmitter_otaa_join_procedure(LoraTransmitter* transmitter);

/**
 * @brief Setup configuration for LoRaWAN
 * @param transmitter Pointer to the LoraTransmitter object.
 */
void lora_transmitter_setup_lorawan(LoraTransmitter* transmitter);

/**
 * @brief Send an Uplink message
 * @param transmitter Pointer to the LoraTransmitter object.
 * @param msg Pointer to the message to be sent.
 */
void lora_transmitter_send_cmsg(LoraTransmitter* transmitter, const char* msg);

/**
 * @brief Set the state manager for the LoRa Transmitter
 * @param transmitter Pointer to the LoraTransmitter object.
 * @param state_manager Pointer to the LoraStateManager object.
 */
void lora_transmitter_set_state_manager(
    LoraTransmitter* transmitter,
    LoraStateManager* state_manager);

/**
 * @brief Set RF configuration in test mode by sending AT command (eg: AT+TEST=RFCFG,866,SF12,125,12,15,14,ON,OFF, OFF)
 * @param transmitter Pointer to the LoraTransmitter object.
 * @param config Pointer to the configuration object.
 */
void lora_transmitter_set_rf_test_config(LoraTransmitter* transmitter, LoraConfigModel* config);

#ifdef __cplusplus
}
#endif

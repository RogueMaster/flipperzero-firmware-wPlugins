#pragma once

typedef enum {
    INIT, // Being on this state means the device is not configured yet
    CONFIG, // Being on this state means the device has been configured (e.g. AppKey, DR, TxPower)
    JOINED, // Being on this state means the device has joined the network
    SENDING, // Being on this state means the device is sending data
    RX, // Being on this state means the device is receiving data
} LoraState;
typedef struct LoraStateManager LoraStateManager;

LoraStateManager* lora_state_manager_alloc(void* context);
void lora_state_manager_set_state(LoraStateManager* state_manager, LoraState state);
LoraState lora_state_manager_get_state(LoraStateManager* state_manager);
void lora_state_manager_free(LoraStateManager* state_manager);

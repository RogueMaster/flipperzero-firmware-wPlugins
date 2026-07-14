#include "lora_state_manager.h"
#include "lora_receiver.h"

struct LoraStateManager {
    LoraState state;
    LoraReceiver* receiver;
};

LoraStateManager* lora_state_manager_alloc(void* context) {
    LoraReceiver* receiver = context;
    furi_assert(receiver);
    LoraStateManager* state_manager = malloc(sizeof(LoraStateManager));
    state_manager->receiver = receiver;
    state_manager->state = INIT;
    return state_manager;
}

void lora_state_manager_free(LoraStateManager* state_manager) {
    furi_assert(state_manager);
    free(state_manager);
}

void lora_state_manager_set_state(LoraStateManager* state_manager, LoraState new_state) {
    if(state_manager->state == new_state) {
        return; // No state change
    }
    lora_receiver_update_process_callback(state_manager->receiver, new_state);
    state_manager->state = new_state;
}

LoraState lora_state_manager_get_state(LoraStateManager* state_manager) {
    furi_assert(state_manager);
    return state_manager->state;
}

#pragma once

#include <furi.h>

typedef struct {
    void* (*alloc)(FuriMessageQueue* link_queue);
    void (*free)(void* instance);
    void (*press)(void* instance, uint16_t key);
    void (*release)(void* instance, uint16_t key);
    void (*offer)(void* instance);
    void (*withdraw)(void* instance);
} TransportLink;

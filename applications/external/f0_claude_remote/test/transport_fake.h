#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "transport.h"

typedef struct {
    TransportKey key;
    bool pressed;
} TransportFakeEvent;

Transport* transport_fake_alloc(void);

void transport_fake_free(Transport* transport);

size_t transport_fake_count(const Transport* transport);

TransportFakeEvent transport_fake_event(const Transport* transport, size_t index);

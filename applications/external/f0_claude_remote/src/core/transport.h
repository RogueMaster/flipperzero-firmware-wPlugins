#pragma once

typedef enum {
    TransportKeyArrowDown,
    TransportKeyArrowUp,
    TransportKeyControlF,
    TransportKeyControlL,
    TransportKeyControlQ,
    TransportKeyControlX,
    TransportKeyEnter,
    TransportKeyEsc,
    TransportKeyMAX,
} TransportKey;

typedef struct Transport Transport;

void transport_press(Transport* transport, TransportKey key);

void transport_release(Transport* transport, TransportKey key);

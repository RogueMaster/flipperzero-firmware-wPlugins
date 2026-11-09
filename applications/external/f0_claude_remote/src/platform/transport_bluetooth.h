#pragma once

#include <furi.h>

typedef struct TransportBluetooth TransportBluetooth;

TransportBluetooth* transport_bluetooth_alloc(FuriMessageQueue* link_queue);

void transport_bluetooth_free(TransportBluetooth* bluetooth);

void transport_bluetooth_press(TransportBluetooth* bluetooth, uint16_t key);

void transport_bluetooth_release(TransportBluetooth* bluetooth, uint16_t key);

void transport_bluetooth_offer(TransportBluetooth* bluetooth);

void transport_bluetooth_withdraw(TransportBluetooth* bluetooth);

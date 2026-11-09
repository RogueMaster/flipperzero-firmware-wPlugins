#pragma once

#include <furi.h>

typedef struct TransportUsb TransportUsb;

TransportUsb* transport_usb_alloc(FuriMessageQueue* link_queue);

void transport_usb_free(TransportUsb* usb);

void transport_usb_press(TransportUsb* usb, uint16_t key);

void transport_usb_release(TransportUsb* usb, uint16_t key);

void transport_usb_offer(TransportUsb* usb);

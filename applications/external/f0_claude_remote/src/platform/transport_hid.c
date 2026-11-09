#include "transport_hid.h"

#include <furi_hal_usb_hid.h>

#include "transport_bluetooth.h"
#include "transport_usb.h"

/* One of the two links at a time: the other is NULL. */
struct Transport {
    FuriMessageQueue* link_queue;
    TransportBluetooth* bluetooth;
    TransportUsb* usb;
};

static const uint16_t transport_hid_keys[TransportKeyMAX] = {
    [TransportKeyArrowDown] = HID_KEYBOARD_DOWN_ARROW,
    [TransportKeyArrowUp] = HID_KEYBOARD_UP_ARROW,
    [TransportKeyControlF] = KEY_MOD_LEFT_CTRL | HID_KEYBOARD_F,
    [TransportKeyControlL] = KEY_MOD_LEFT_CTRL | HID_KEYBOARD_L,
    [TransportKeyControlQ] = KEY_MOD_LEFT_CTRL | HID_KEYBOARD_Q,
    [TransportKeyControlX] = KEY_MOD_LEFT_CTRL | HID_KEYBOARD_X,
    [TransportKeyEnter] = HID_KEYBOARD_RETURN,
    [TransportKeyEsc] = HID_KEYBOARD_ESCAPE,
};

Transport* transport_hid_alloc(FuriMessageQueue* link_queue) {
    Transport* transport = malloc(sizeof(Transport));
    transport->link_queue = link_queue;
    transport->bluetooth = transport_bluetooth_alloc(link_queue);
    transport->usb = NULL;
    return transport;
}

void transport_hid_free(Transport* transport) {
    if(transport->usb) {
        transport_usb_free(transport->usb);
    } else {
        transport_bluetooth_free(transport->bluetooth);
    }
    free(transport);
}

void transport_press(Transport* transport, TransportKey key) {
    if(transport->usb) {
        transport_usb_press(transport->usb, transport_hid_keys[key]);
    } else {
        transport_bluetooth_press(transport->bluetooth, transport_hid_keys[key]);
    }
}

void transport_release(Transport* transport, TransportKey key) {
    if(transport->usb) {
        transport_usb_release(transport->usb, transport_hid_keys[key]);
    } else {
        transport_bluetooth_release(transport->bluetooth, transport_hid_keys[key]);
    }
}

void transport_offer(Transport* transport) {
    if(transport->usb) {
        transport_usb_offer(transport->usb);
    } else {
        transport_bluetooth_offer(transport->bluetooth);
    }
}

void transport_withdraw(Transport* transport) {
    if(transport->bluetooth) {
        transport_bluetooth_withdraw(transport->bluetooth);
    }
}

/* A Connected that one link left queued would read as the other's. */
void transport_use_usb(Transport* transport) {
    transport_bluetooth_free(transport->bluetooth);
    transport->bluetooth = NULL;
    furi_message_queue_reset(transport->link_queue);
    transport->usb = transport_usb_alloc(transport->link_queue);
}

void transport_use_bluetooth(Transport* transport) {
    transport_usb_free(transport->usb);
    transport->usb = NULL;
    furi_message_queue_reset(transport->link_queue);
    transport->bluetooth = transport_bluetooth_alloc(transport->link_queue);
}

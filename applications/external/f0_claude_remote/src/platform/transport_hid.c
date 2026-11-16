#include "transport_hid.h"

#include <furi_hal_usb_hid.h>

#include "transport_bluetooth.h"
#include "transport_usb.h"

struct Transport {
    FuriMessageQueue* link_queue;
    const TransportLink* link;
    void* instance;
};

static const uint16_t transport_hid_keys[TransportKeyMAX] = {
    [TransportKeyArrowDown] = HID_KEYBOARD_DOWN_ARROW,
    [TransportKeyArrowLeft] = HID_KEYBOARD_LEFT_ARROW,
    [TransportKeyArrowRight] = HID_KEYBOARD_RIGHT_ARROW,
    [TransportKeyArrowUp] = HID_KEYBOARD_UP_ARROW,
    [TransportKeyControlF] = KEY_MOD_LEFT_CTRL | HID_KEYBOARD_F,
    [TransportKeyControlL] = KEY_MOD_LEFT_CTRL | HID_KEYBOARD_L,
    [TransportKeyControlQ] = KEY_MOD_LEFT_CTRL | HID_KEYBOARD_Q,
    [TransportKeyControlX] = KEY_MOD_LEFT_CTRL | HID_KEYBOARD_X,
    [TransportKeyEnter] = HID_KEYBOARD_RETURN,
    [TransportKeyEsc] = HID_KEYBOARD_ESCAPE,
};

static const TransportLink* const transport_hid_links[LinkMAX] = {
    [LinkBluetooth] = &transport_bluetooth,
    [LinkUsb] = &transport_usb,
};

static void transport_hid_open(Transport* transport, Link link) {
    transport->link = transport_hid_links[link];
    transport->instance = transport->link->alloc(transport->link_queue);
}

static void transport_hid_close(Transport* transport) {
    transport->link->free(transport->instance);
}

Transport* transport_hid_alloc(FuriMessageQueue* link_queue, Link link) {
    Transport* transport = malloc(sizeof(Transport));
    transport->link_queue = link_queue;
    transport_hid_open(transport, link);
    return transport;
}

void transport_hid_free(Transport* transport) {
    transport_hid_close(transport);
    free(transport);
}

/* A Connected that one link left queued would read as the other's. */
void transport_hid_set_link(Transport* transport, Link link) {
    if(transport_hid_links[link] == transport->link) {
        return;
    }
    transport_hid_close(transport);
    furi_message_queue_reset(transport->link_queue);
    transport_hid_open(transport, link);
}

void transport_press(Transport* transport, TransportKey key) {
    transport->link->press(transport->instance, transport_hid_keys[key]);
}

void transport_release(Transport* transport, TransportKey key) {
    transport->link->release(transport->instance, transport_hid_keys[key]);
}

void transport_offer(Transport* transport) {
    transport->link->offer(transport->instance);
}

void transport_withdraw(Transport* transport) {
    transport->link->withdraw(transport->instance);
}

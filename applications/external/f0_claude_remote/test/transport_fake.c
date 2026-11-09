#include "transport_fake.h"

#include <assert.h>
#include <stdlib.h>

#define TRANSPORT_FAKE_EVENTS 8

struct Transport {
    TransportFakeEvent events[TRANSPORT_FAKE_EVENTS];
    size_t count;
    bool offered;
    bool on_usb;
};

static void transport_fake_record(Transport* transport, TransportKey key, bool pressed) {
    assert(transport->count < TRANSPORT_FAKE_EVENTS);
    transport->events[transport->count].key = key;
    transport->events[transport->count].pressed = pressed;
    transport->count++;
}

Transport* transport_fake_alloc(void) {
    Transport* transport = malloc(sizeof(Transport));
    assert(transport);
    transport->count = 0;
    transport->offered = true;
    transport->on_usb = false;
    return transport;
}

void transport_fake_free(Transport* transport) {
    free(transport);
}

size_t transport_fake_count(const Transport* transport) {
    return transport->count;
}

TransportFakeEvent transport_fake_event(const Transport* transport, size_t index) {
    assert(index < transport->count);
    return transport->events[index];
}

bool transport_fake_offered(const Transport* transport) {
    return transport->offered;
}

bool transport_fake_on_usb(const Transport* transport) {
    return transport->on_usb;
}

void transport_press(Transport* transport, TransportKey key) {
    transport_fake_record(transport, key, true);
}

void transport_release(Transport* transport, TransportKey key) {
    transport_fake_record(transport, key, false);
}

void transport_offer(Transport* transport) {
    transport->offered = true;
}

void transport_withdraw(Transport* transport) {
    transport->offered = false;
}

void transport_use_usb(Transport* transport) {
    transport->on_usb = true;
}

void transport_use_bluetooth(Transport* transport) {
    transport->on_usb = false;
}

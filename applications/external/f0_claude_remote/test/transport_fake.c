#include "transport_fake.h"

#include <assert.h>
#include <stdlib.h>

#define TRANSPORT_FAKE_EVENTS 8

struct Transport {
    TransportFakeEvent events[TRANSPORT_FAKE_EVENTS];
    size_t count;
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

void transport_press(Transport* transport, TransportKey key) {
    transport_fake_record(transport, key, true);
}

void transport_release(Transport* transport, TransportKey key) {
    transport_fake_record(transport, key, false);
}

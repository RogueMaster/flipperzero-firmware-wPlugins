#include "link_select.h"

static const LinkOutput nothing = {0, false, false, false};

void link_select_init(LinkSelect* ls) {
    for(uint8_t i = 0; i < LINK_SLOTS; i++) {
        host_link_init(&ls->link[i]);
        ls->open[i] = false;
    }
    ls->active = LINK_NONE;
}

static uint8_t bit(uint8_t slot) {
    return (uint8_t)(1u << slot);
}

static LinkOutput greet(LinkSelect* ls, uint8_t slot, uint32_t now_ms) {
    LinkOutput out = nothing;
    if(host_link_port_opened(&ls->link[slot], now_ms).send_hello) out.send_hello = bit(slot);
    return out;
}

LinkOutput link_select_port_opened(LinkSelect* ls, uint8_t slot, uint32_t now_ms) {
    if(slot >= LINK_SLOTS || ls->open[slot]) return nothing;
    ls->open[slot] = true;
    if(ls->active != LINK_NONE) return nothing; /* another host is up */
    return greet(ls, slot, now_ms);
}

/* The port stays open, its handshake ends: a later hello there is ignored. */
static void park_others(LinkSelect* ls, uint8_t slot) {
    for(uint8_t i = 0; i < LINK_SLOTS; i++)
        if(i != slot) host_link_port_closed(&ls->link[i]);
}

LinkOutput link_select_port_closed(LinkSelect* ls, uint8_t slot, uint32_t now_ms) {
    if(slot >= LINK_SLOTS) return nothing;
    LinkOutput out = nothing;
    ls->open[slot] = false;
    out.host_lost = host_link_port_closed(&ls->link[slot]).host_lost;
    if(ls->active != slot) return out;
    ls->active = LINK_NONE;
    /* a host left waiting on another port answers a fresh greeting */
    for(uint8_t i = 0; i < LINK_SLOTS; i++) {
        if(i == slot || !ls->open[i]) continue;
        host_link_port_closed(&ls->link[i]);
        out.send_hello |= greet(ls, i, now_ms).send_hello;
    }
    return out;
}

LinkOutput link_select_host_hello(LinkSelect* ls, uint8_t slot, uint32_t now_ms) {
    if(slot >= LINK_SLOTS) return nothing;
    if(ls->active != LINK_NONE && ls->active != slot) return nothing;
    if(ls->link[slot].state == HostLinkStateClosed) return nothing; /* stale or parked */
    HostLinkOutput o = host_link_host_hello(&ls->link[slot], now_ms);
    LinkOutput out = nothing;
    out.accepted = true;
    out.host_connected = o.host_connected;
    if(o.send_hello) out.send_hello = bit(slot);
    if(ls->active == LINK_NONE) {
        ls->active = slot;
        park_others(ls, slot);
    }
    return out;
}

LinkOutput link_select_tick(LinkSelect* ls, uint32_t now_ms) {
    for(uint8_t i = 0; i < LINK_SLOTS; i++)
        host_link_tick(&ls->link[i], now_ms);
    return nothing;
}

uint8_t link_select_active(const LinkSelect* ls) {
    return ls->active;
}

bool link_select_listening(const LinkSelect* ls, uint8_t slot) {
    return slot < LINK_SLOTS && ls->link[slot].state != HostLinkStateClosed;
}

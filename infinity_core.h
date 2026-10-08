/*
 * Disney Infinity USB base protocol core (no platform dependencies).
 *
 * The behaviour follows RPCS3's Infinity base emulation (rpcs3/Emu/Io/Infinity.cpp)
 * command for command; tests/ compares this code against that source.
 *
 * Threading: nothing here locks. On the Flipper the USB interrupt calls
 * inf_base_handle_out()/inf_base_next_in(), and the UI thread must wrap its own
 * calls in a critical section.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INF_PACKET_SIZE   32
#define INF_BLOCK_SIZE    16
#define INF_FIGURE_BLOCKS 0x14
#define INF_FIGURE_SIZE   (INF_FIGURE_BLOCKS * INF_BLOCK_SIZE) /* 320 bytes */
#define INF_SLOT_COUNT    9
#define INF_ORDER_UNSET   0xFF
#define INF_EVENT_QUEUE   16
#define INF_REPLY_QUEUE   16
#define INF_LOG_ENTRIES   48

typedef struct {
    uint8_t data[INF_FIGURE_SIZE];
    bool present;
    bool dirty; /* data changed by the game and not yet saved */
    uint8_t order_added;
} InfFigure;

typedef struct {
    uint8_t command;
    uint8_t sequence;
    uint8_t arg0;
    uint8_t arg1;
} InfLogEntry;

typedef struct {
    uint32_t rand_a, rand_b, rand_c, rand_d;
    uint8_t next_order;
    InfFigure figures[INF_SLOT_COUNT];

    uint8_t events[INF_EVENT_QUEUE][INF_PACKET_SIZE];
    uint8_t event_head;
    uint8_t event_count;
    uint8_t replies[INF_REPLY_QUEUE][INF_PACKET_SIZE];
    uint8_t reply_head;
    uint8_t reply_count;

    /* Diagnostics, read by the UI. */
    uint32_t commands_seen;
    uint32_t dropped_packets;
    uint8_t last_command;
    bool handshake_seen;
    InfLogEntry log[INF_LOG_ENTRIES];
    uint8_t log_next;
    uint32_t log_total;
} InfBase;

/* Reset everything, including figures. */
void inf_base_init(InfBase* base);

/* The USB link went away or was reconfigured: drop queued packets and the
 * handshake state, keep the figures. */
void inf_base_reset_link(InfBase* base);

/* A 32-byte packet arrived on the OUT endpoint. Queues exactly one reply. */
void inf_base_handle_out(InfBase* base, const uint8_t packet[INF_PACKET_SIZE]);

/* Fetch the next packet for the IN endpoint (figure events first, then replies).
 * Returns false when nothing is waiting. */
bool inf_base_next_in(InfBase* base, uint8_t out[INF_PACKET_SIZE]);

/* Same packet as inf_base_next_in would return, without removing it. Used by the
 * USB layer: peek, try to write to the endpoint, drop only if the write took. */
bool inf_base_peek_in(const InfBase* base, uint8_t out[INF_PACKET_SIZE]);
void inf_base_drop_in(InfBase* base);

/* Put a figure (320-byte image) on a slot (0..8) and queue the "added" event. */
bool inf_base_load_figure(InfBase* base, uint8_t slot, const uint8_t data[INF_FIGURE_SIZE]);

/* Take the figure off a slot and queue the "removed" event. The data stays in
 * place until the next load so it can still be saved. */
bool inf_base_remove_figure(InfBase* base, uint8_t slot);

bool inf_base_slot_present(const InfBase* base, uint8_t slot);

/* If the game wrote to this slot since the last call, copy the image out,
 * clear the flag and return true. */
bool inf_base_take_dirty(InfBase* base, uint8_t slot, uint8_t out[INF_FIGURE_SIZE]);

#ifdef __cplusplus
}
#endif

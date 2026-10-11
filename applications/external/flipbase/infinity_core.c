/*
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "infinity_core.h"

#include <string.h>

static uint8_t checksum(const uint8_t* data, int count) {
    unsigned sum = 0;
    for(int i = 0; i < count; i++) sum += data[i];
    return (uint8_t)(sum & 0xFF);
}

static uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

/* ----------------------------------------------------- challenge / PRNG */

static const uint64_t SCRAMBLE_MASK = 0x8E55AA1B3999E8AAull;

static uint32_t descramble(uint64_t value) {
    uint64_t mask = SCRAMBLE_MASK;
    uint32_t ret = 0;
    for(int i = 0; i < 64; i++) {
        if(mask & 0x8000000000000000ull) {
            ret = (ret << 1) | (uint32_t)(value & 0x01);
        }
        value >>= 1;
        mask <<= 1;
    }
    return ret;
}

static uint64_t scramble(uint32_t value, uint32_t garbage) {
    uint64_t mask = SCRAMBLE_MASK;
    uint64_t ret = 0;
    for(int i = 0; i < 64; i++) {
        ret <<= 1;
        if((mask & 1) != 0) {
            ret |= (value & 1);
            value >>= 1;
        } else {
            ret |= (garbage & 1);
            garbage >>= 1;
        }
        mask >>= 1;
    }
    return ret;
}

static uint32_t get_next(InfBase* base) {
    uint32_t a = base->rand_a;
    uint32_t b = base->rand_b;
    uint32_t c = base->rand_c;
    uint32_t ret = rotl32(base->rand_b, 27);

    const uint32_t temp = a + ((ret ^ 0xFFFFFFFFu) + 1u);
    b ^= rotl32(c, 17);
    a = base->rand_d;
    c += a;
    ret = b + temp;
    a += temp;

    base->rand_c = a;
    base->rand_a = b;
    base->rand_b = c;
    base->rand_d = ret;
    return ret;
}

static void generate_seed(InfBase* base, uint32_t seed) {
    base->rand_a = 0xF1EA5EEDu;
    base->rand_b = seed;
    base->rand_c = seed;
    base->rand_d = seed;
    for(int i = 0; i < 23; i++) get_next(base);
}

/* ------------------------------------------------------------ queues */

static void push_packet(
    uint8_t queue[][INF_PACKET_SIZE],
    uint8_t* head,
    uint8_t* count,
    uint8_t capacity,
    const uint8_t packet[INF_PACKET_SIZE],
    uint32_t* dropped) {
    if(*count == capacity) {
        /* Full: forget the oldest packet so the newest still gets through. */
        *head = (uint8_t)((*head + 1) % capacity);
        (*count)--;
        (*dropped)++;
    }
    const uint8_t tail = (uint8_t)((*head + *count) % capacity);
    memcpy(queue[tail], packet, INF_PACKET_SIZE);
    (*count)++;
}

static bool pop_packet(
    uint8_t queue[][INF_PACKET_SIZE],
    uint8_t* head,
    uint8_t* count,
    uint8_t capacity,
    uint8_t out[INF_PACKET_SIZE]) {
    if(*count == 0) return false;
    memcpy(out, queue[*head], INF_PACKET_SIZE);
    *head = (uint8_t)((*head + 1) % capacity);
    (*count)--;
    return true;
}

/* ----------------------------------------------------------- replies */

static void blank_response(uint8_t sequence, uint8_t reply[INF_PACKET_SIZE]) {
    reply[0] = 0xAA;
    reply[1] = 0x01;
    reply[2] = sequence;
    reply[3] = checksum(reply, 3);
}

static void present_figures(const InfBase* base, uint8_t sequence, uint8_t reply[INF_PACKET_SIZE]) {
    int x = 3;
    for(uint8_t i = 0; i < INF_SLOT_COUNT; i++) {
        const uint8_t slot = (i == 0) ? 0x10 : (i < 4) ? 0x20 : 0x30;
        if(base->figures[i].present) {
            reply[x] = (uint8_t)(slot + base->figures[i].order_added);
            reply[x + 1] = 0x09;
            x += 2;
        }
    }
    reply[0] = 0xAA;
    reply[1] = (uint8_t)(x - 2);
    reply[2] = sequence;
    reply[x] = checksum(reply, x);
}

/* The game addresses a figure by the order in which it was first added. An
 * unknown number falls back to slot 0, exactly like the reference. */
static InfFigure* figure_by_order(InfBase* base, uint8_t order_added) {
    for(uint8_t i = 0; i < INF_SLOT_COUNT; i++) {
        if(base->figures[i].order_added == order_added) return &base->figures[i];
    }
    return &base->figures[0];
}

static uint8_t file_block_for(uint8_t block) {
    return (uint8_t)((block == 0) ? 1 : (block * 4));
}

static void query_block(
    InfBase* base,
    uint8_t fig_num,
    uint8_t block,
    uint8_t reply[INF_PACKET_SIZE],
    uint8_t sequence) {
    InfFigure* figure = figure_by_order(base, fig_num);
    reply[0] = 0xAA;
    reply[1] = 0x12;
    reply[2] = sequence;
    reply[3] = 0x00;
    const uint8_t file_block = file_block_for(block);
    if(figure->present && file_block < INF_FIGURE_BLOCKS) {
        memcpy(&reply[4], figure->data + (INF_BLOCK_SIZE * file_block), INF_BLOCK_SIZE);
    }
    reply[20] = checksum(reply, 20);
}

static void write_block(
    InfBase* base,
    uint8_t fig_num,
    uint8_t block,
    const uint8_t* to_write,
    uint8_t reply[INF_PACKET_SIZE],
    uint8_t sequence) {
    InfFigure* figure = figure_by_order(base, fig_num);
    reply[0] = 0xAA;
    reply[1] = 0x02;
    reply[2] = sequence;
    reply[3] = 0x00;
    const uint8_t file_block = file_block_for(block);
    if(figure->present && file_block < INF_FIGURE_BLOCKS) {
        memcpy(figure->data + (file_block * INF_BLOCK_SIZE), to_write, INF_BLOCK_SIZE);
        figure->dirty = true;
    }
    reply[4] = checksum(reply, 4);
}

static void figure_identifier(
    InfBase* base,
    uint8_t fig_num,
    uint8_t sequence,
    uint8_t reply[INF_PACKET_SIZE]) {
    InfFigure* figure = figure_by_order(base, fig_num);
    reply[0] = 0xAA;
    reply[1] = 0x09;
    reply[2] = sequence;
    reply[3] = 0x00;
    if(figure->present) {
        memcpy(&reply[4], figure->data, 7);
    }
    reply[11] = checksum(reply, 11);
}

static void next_and_scramble(InfBase* base, uint8_t sequence, uint8_t reply[INF_PACKET_SIZE]) {
    const uint32_t next_random = get_next(base);
    const uint64_t scrambled = scramble(next_random, 0);
    memset(reply, 0, INF_PACKET_SIZE);
    reply[0] = 0xAA;
    reply[1] = 0x09;
    reply[2] = sequence;
    for(int i = 0; i < 8; i++) {
        reply[3 + i] = (uint8_t)((scrambled >> (56 - 8 * i)) & 0xFF);
    }
    reply[11] = checksum(reply, 11);
}

static void descramble_and_seed(InfBase* base, const uint8_t* buf, uint8_t sequence, uint8_t reply[INF_PACKET_SIZE]) {
    uint64_t value = 0;
    for(int i = 0; i < 8; i++) {
        value = (value << 8) | buf[4 + i];
    }
    generate_seed(base, descramble(value));
    blank_response(sequence, reply);
}

/* ----------------------------------------------------------- public */

void inf_base_init(InfBase* base) {
    memset(base, 0, sizeof(*base));
    for(int i = 0; i < INF_SLOT_COUNT; i++) {
        base->figures[i].order_added = INF_ORDER_UNSET;
    }
}

void inf_base_reset_link(InfBase* base) {
    base->event_head = 0;
    base->event_count = 0;
    base->reply_head = 0;
    base->reply_count = 0;
    base->handshake_seen = false;
}

void inf_base_handle_out(InfBase* base, const uint8_t buf[INF_PACKET_SIZE]) {
    const uint8_t command = buf[2];
    const uint8_t sequence = buf[3];
    uint8_t reply[INF_PACKET_SIZE];
    memset(reply, 0, sizeof(reply));

    base->commands_seen++;
    base->last_command = command;
    base->log[base->log_next].command = command;
    base->log[base->log_next].sequence = sequence;
    base->log[base->log_next].arg0 = buf[4];
    base->log[base->log_next].arg1 = buf[5];
    base->log_next = (uint8_t)((base->log_next + 1) % INF_LOG_ENTRIES);
    base->log_total++;

    switch(command) {
    case 0x80: {
        static const uint8_t info[24] = {0xaa, 0x15, 0x00, 0x00, 0x0f, 0x01, 0x00, 0x03,
                                         0x02, 0x09, 0x09, 0x43, 0x20, 0x32, 0x62, 0x36,
                                         0x36, 0x4b, 0x34, 0x99, 0x67, 0x31, 0x93, 0x8c};
        memcpy(reply, info, sizeof(info));
        break;
    }
    case 0x81: /* initiate challenge */
        base->handshake_seen = true;
        descramble_and_seed(base, buf, sequence, reply);
        break;
    case 0x83: /* challenge response */
        next_and_scramble(base, sequence, reply);
        break;
    case 0x90:
    case 0x92:
    case 0x93:
    case 0x95:
    case 0x96: /* light commands */
        blank_response(sequence, reply);
        break;
    case 0xA1:
        present_figures(base, sequence, reply);
        break;
    case 0xA2:
        query_block(base, buf[4], buf[5], reply, sequence);
        break;
    case 0xA3:
        write_block(base, buf[4], buf[5], &buf[7], reply, sequence);
        break;
    case 0xB4:
        figure_identifier(base, buf[4], sequence, reply);
        break;
    case 0xB5:
        blank_response(sequence, reply);
        break;
    default:
        break; /* unknown command: the reference answers with an all-zero packet */
    }

    push_packet(
        base->replies, &base->reply_head, &base->reply_count, INF_REPLY_QUEUE, reply, &base->dropped_packets);
}

bool inf_base_peek_in(const InfBase* base, uint8_t out[INF_PACKET_SIZE]) {
    if(base->event_count) {
        memcpy(out, base->events[base->event_head], INF_PACKET_SIZE);
        return true;
    }
    if(base->reply_count) {
        memcpy(out, base->replies[base->reply_head], INF_PACKET_SIZE);
        return true;
    }
    return false;
}

void inf_base_drop_in(InfBase* base) {
    uint8_t scratch[INF_PACKET_SIZE];
    if(!pop_packet(base->events, &base->event_head, &base->event_count, INF_EVENT_QUEUE, scratch)) {
        pop_packet(base->replies, &base->reply_head, &base->reply_count, INF_REPLY_QUEUE, scratch);
    }
}

bool inf_base_next_in(InfBase* base, uint8_t out[INF_PACKET_SIZE]) {
    if(!inf_base_peek_in(base, out)) return false;
    inf_base_drop_in(base);
    return true;
}

/* Slots 0-2 report as position 1, 3-5 as 2, 6-8 as 3. */
static uint8_t derive_position(uint8_t slot) {
    if(slot <= 2) return 1;
    if(slot <= 5) return 2;
    if(slot <= 8) return 3;
    return 0;
}

static void queue_event(InfBase* base, uint8_t position, uint8_t order_added, uint8_t status) {
    uint8_t event[INF_PACKET_SIZE];
    memset(event, 0, sizeof(event));
    event[0] = 0xAB;
    event[1] = 0x04;
    event[2] = position;
    event[3] = 0x09;
    event[4] = order_added;
    event[5] = status;
    event[6] = checksum(event, 6);
    push_packet(
        base->events, &base->event_head, &base->event_count, INF_EVENT_QUEUE, event, &base->dropped_packets);
}

bool inf_base_load_figure(InfBase* base, uint8_t slot, const uint8_t data[INF_FIGURE_SIZE]) {
    if(slot >= INF_SLOT_COUNT) return false;
    InfFigure* figure = &base->figures[slot];

    memcpy(figure->data, data, INF_FIGURE_SIZE);
    figure->present = true;
    figure->dirty = false;
    if(figure->order_added == INF_ORDER_UNSET) {
        figure->order_added = base->next_order;
        base->next_order++;
    }
    queue_event(base, derive_position(slot), figure->order_added, 0x00);
    return true;
}

bool inf_base_remove_figure(InfBase* base, uint8_t slot) {
    if(slot >= INF_SLOT_COUNT) return false;
    InfFigure* figure = &base->figures[slot];
    if(!figure->present) return false;

    figure->present = false;
    queue_event(base, derive_position(slot), figure->order_added, 0x01);
    return true;
}

bool inf_base_slot_present(const InfBase* base, uint8_t slot) {
    return slot < INF_SLOT_COUNT && base->figures[slot].present;
}

bool inf_base_take_dirty(InfBase* base, uint8_t slot, uint8_t out[INF_FIGURE_SIZE]) {
    if(slot >= INF_SLOT_COUNT) return false;
    InfFigure* figure = &base->figures[slot];
    if(!figure->dirty) return false;
    memcpy(out, figure->data, INF_FIGURE_SIZE);
    figure->dirty = false;
    return true;
}

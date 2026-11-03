/* Keyboard key table (see kbd_layout.h). SPDX-License-Identifier: GPL-3.0-only
 *
 * Key table after UART Terminal's uart_text_input.c (github.com/cool4uma/UART_Terminal),
 * which extends the Flipper firmware's text_input.c (GPL-3.0). */
#include "kbd_layout.h"

#include <stddef.h>

static const KbdKey row0[] = {
    {'{', 1, 0},
    {'(', 9, 0},
    {'[', 17, 0},
    {'|', 25, 0},
    {'@', 33, 0},
    {'&', 41, 0},
    {'#', 49, 0},
    {';', 57, 0},
    {'^', 65, 0},
    {'*', 73, 0},
    {'`', 81, 0},
    {'"', 89, 0},
    {'~', 97, 0},
    {'\'', 105, 0},
    {'.', 113, 0},
    {'/', 120, 0},
};

static const KbdKey row1[] = {
    {'q', 1, 10},
    {'w', 9, 10},
    {'e', 17, 10},
    {'r', 25, 10},
    {'t', 33, 10},
    {'y', 41, 10},
    {'u', 49, 10},
    {'i', 57, 10},
    {'o', 65, 10},
    {'p', 73, 10},
    {'0', 81, 10},
    {'1', 89, 10},
    {'2', 97, 10},
    {'3', 105, 10},
    {'=', 113, 10},
    {'-', 120, 10},
};

static const KbdKey row2[] = {
    {'a', 1, 21},
    {'s', 9, 21},
    {'d', 17, 21},
    {'f', 25, 21},
    {'g', 33, 21},
    {'h', 41, 21},
    {'j', 49, 21},
    {'k', 57, 21},
    {'l', 65, 21},
    {KBD_BS, 72, 13},
    {'4', 89, 21},
    {'5', 97, 21},
    {'6', 105, 21},
    {'$', 113, 21},
    {KBD_SHIFT, 120, 21},
};

static const KbdKey row3[] = {
    {'z', 1, 33},
    {'x', 9, 33},
    {'c', 17, 33},
    {'v', 25, 33},
    {'b', 33, 33},
    {'n', 41, 33},
    {'m', 49, 33},
    {'_', 57, 33},
    {KBD_ENTER, 64, 24},
    {'7', 89, 33},
    {'8', 97, 33},
    {'9', 105, 33},
    {'!', 113, 33},
    {'+', 120, 33},
};

#define N(a) ((uint8_t)(sizeof(a) / sizeof((a)[0])))

const KbdKey* kbd_row(uint8_t row, uint8_t* count) {
    switch(row) {
    case 0:
        *count = N(row0);
        return row0;
    case 1:
        *count = N(row1);
        return row1;
    case 2:
        *count = N(row2);
        return row2;
    default:
        *count = N(row3);
        return row3;
    }
}

uint8_t kbd_row_len(uint8_t row) {
    uint8_t n;
    kbd_row(row, &n);
    return n;
}

char kbd_shift(char c) {
    if(c >= 'a' && c <= 'z') return (char)(c - 0x20);
    switch(c) {
    case '_':
        return ' ';
    case '(':
        return ')';
    case '{':
        return '}';
    case '[':
        return ']';
    case '/':
        return '\\';
    case ';':
        return ':';
    case '.':
        return ',';
    case '!':
        return '?';
    case '&':
        return '<';
    case '|':
        return '>';
    case '5':
        return '%';
    default:
        return c;
    }
}

KbdRect kbd_key_rect(const KbdKey* k) {
    KbdRect r;
    uint8_t x = (uint8_t)(KBD_ORIGIN_X + k->x), y = (uint8_t)(KBD_ORIGIN_Y + k->y);
    if(k->key == KBD_BS) {
        r = (KbdRect){x, y, 16, 9};
    } else if(k->key == KBD_ENTER) {
        r = (KbdRect){x, y, 24, 11};
    } else {
        r = (KbdRect){(uint8_t)(x - 1), (uint8_t)(y - 8), 7, 10}; /* the stock selection box */
    }
    return r;
}

static uint8_t centre(const KbdKey* k) {
    KbdRect r = kbd_key_rect(k);
    return (uint8_t)(r.x + r.w / 2);
}

uint8_t kbd_nearest(uint8_t row, uint8_t col, uint8_t to_row) {
    uint8_t n;
    const KbdKey* from = kbd_row(row, &n);
    uint8_t cx = centre(&from[col < n ? col : n - 1]);
    const KbdKey* to = kbd_row(to_row, &n);
    uint8_t best = 0, best_d = 255;
    for(uint8_t i = 0; i < n; i++) {
        uint8_t c = centre(&to[i]);
        uint8_t d = (uint8_t)(c > cx ? c - cx : cx - c);
        if(d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

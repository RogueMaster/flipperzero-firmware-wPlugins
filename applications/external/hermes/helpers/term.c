#include "term.h"

#include <string.h>

#define TERM_TAB_STOP (8u)

/* A boot log is full of colour codes. Without this the screen fills with
 * "[0;32m" noise instead of text, so the cooker is a small VT parser. */
typedef enum {
    TermStateNormal,
    TermStateEsc, // saw ESC, waiting to find out what kind
    TermStateCsi, // ESC [ ... final byte in 0x40..0x7E
    TermStateOsc, // ESC ] ... terminated by BEL or ST
    TermStateOscEsc, // inside OSC, saw ESC, a '\' ends the string
    TermStateSwallow, // charset selects and friends: eat exactly one byte
} TermState;

struct Term {
    char lines[TERM_LINES][TERM_COLS + 1];
    uint16_t head; // newest line
    uint16_t count; // lines in use
    uint8_t col;
    TermState state;

    uint8_t raw[TERM_RAW_BYTES];
    uint16_t raw_head;
    uint16_t raw_count;

    uint32_t total_rx;
};

Term* term_alloc(void) {
    Term* t = malloc(sizeof(Term));
    term_reset(t);
    return t;
}

void term_free(Term* t) {
    furi_assert(t);
    free(t);
}

void term_reset(Term* t) {
    furi_assert(t);
    memset(t, 0, sizeof(Term));
    t->count = 1; // the cursor always has a line to sit on
    t->state = TermStateNormal;
}

/* -------------------------------------------------------------- writing --- */

static void term_newline(Term* t) {
    t->head = (uint16_t)((t->head + 1u) % TERM_LINES);
    memset(t->lines[t->head], 0, TERM_COLS + 1);
    t->col = 0;
    if(t->count < TERM_LINES) t->count++;
}

static void term_putc(Term* t, char c) {
    if(t->col >= TERM_COLS) term_newline(t); // wrap
    t->lines[t->head][t->col] = c;
    t->col++;
}

static void term_control(Term* t, uint8_t b) {
    switch(b) {
    case '\n':
        /* Devices that send a bare LF still expect the next line to start at
         * the left, so this does the carriage return too. */
        term_newline(t);
        break;
    case '\r':
        t->col = 0;
        break;
    case '\b':
        if(t->col > 0) {
            t->col--;
            t->lines[t->head][t->col] = ' ';
        }
        break;
    case '\t': {
        uint8_t next = (uint8_t)((t->col + TERM_TAB_STOP) & ~(TERM_TAB_STOP - 1u));
        if(next > TERM_COLS) next = TERM_COLS;
        while(t->col < next) {
            t->lines[t->head][t->col] = ' ';
            t->col++;
        }
        break;
    }
    default:
        break; // BEL and the rest: nothing sensible to draw
    }
}

static void term_cook(Term* t, uint8_t b) {
    switch(t->state) {
    case TermStateNormal:
        if(b == 0x1B) {
            t->state = TermStateEsc;
        } else if(b >= 0x20 && b <= 0x7E) {
            term_putc(t, (char)b);
        } else {
            term_control(t, b);
        }
        break;

    case TermStateEsc:
        if(b == '[') {
            t->state = TermStateCsi;
        } else if(b == ']') {
            t->state = TermStateOsc;
        } else if(b == '(' || b == ')' || b == '#') {
            t->state = TermStateSwallow;
        } else {
            t->state = TermStateNormal; // two-byte escape, already consumed
        }
        break;

    case TermStateCsi:
        /* Parameters and intermediates run 0x20..0x3F; the first byte in
         * 0x40..0x7E ends the sequence. */
        if(b >= 0x40 && b <= 0x7E) t->state = TermStateNormal;
        break;

    case TermStateOsc:
        if(b == 0x07) {
            t->state = TermStateNormal; // BEL terminator
        } else if(b == 0x1B) {
            t->state = TermStateOscEsc;
        }
        break;

    case TermStateOscEsc:
        t->state = (b == '\\') ? TermStateNormal : TermStateOsc;
        break;

    case TermStateSwallow:
    default:
        t->state = TermStateNormal;
        break;
    }
}

void term_feed(Term* t, const uint8_t* data, size_t len) {
    furi_assert(t);
    furi_assert(data);

    for(size_t i = 0; i < len; i++) {
        const uint8_t b = data[i];

        t->raw[t->raw_head] = b;
        t->raw_head = (uint16_t)((t->raw_head + 1u) % TERM_RAW_BYTES);
        if(t->raw_count < TERM_RAW_BYTES) t->raw_count++;

        t->total_rx++;
        term_cook(t, b);
    }
}

void term_feed_echo(Term* t, uint8_t byte) {
    furi_assert(t);
    term_cook(t, byte); // deliberately not counted as received, and not in raw
}

/* -------------------------------------------------------------- reading --- */

size_t term_line_count(const Term* t) {
    furi_assert(t);
    return t->count;
}

const char* term_line(const Term* t, size_t i) {
    furi_assert(t);
    if(i >= t->count) return "";
    const size_t oldest = (t->head + TERM_LINES - (t->count - 1u)) % TERM_LINES;
    return t->lines[(oldest + i) % TERM_LINES];
}

uint8_t term_cursor_col(const Term* t) {
    furi_assert(t);
    return t->col;
}

size_t term_raw_count(const Term* t) {
    furi_assert(t);
    return t->raw_count;
}

uint8_t term_raw_at(const Term* t, size_t i) {
    furi_assert(t);
    if(i >= t->raw_count) return 0;
    const size_t oldest = (t->raw_head + TERM_RAW_BYTES - t->raw_count) % TERM_RAW_BYTES;
    return t->raw[(oldest + i) % TERM_RAW_BYTES];
}

uint32_t term_total_rx(const Term* t) {
    furi_assert(t);
    return t->total_rx;
}

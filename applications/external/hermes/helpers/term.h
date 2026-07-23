#pragma once

#include <furi.h>

/* 21 columns is what FontKeyboard fits across 128 px. Real consoles assume 80,
 * so long lines wrap - honest, and better than truncating. */
#define TERM_COLS      (21u)
#define TERM_LINES     (96u)
#define TERM_RAW_BYTES (512u)

typedef struct Term Term;

Term* term_alloc(void);
void term_free(Term* t);
void term_reset(Term* t);

/** Feed received bytes through the cooker. Call from one thread only. */
void term_feed(Term* t, const uint8_t* data, size_t len);

/** Feed a locally-typed byte so the user sees their own keystrokes when the
 *  target does not echo them back. */
void term_feed_echo(Term* t, uint8_t byte);

size_t term_line_count(const Term* t);

/** Line `i`, where 0 is the oldest still in scrollback. */
const char* term_line(const Term* t, size_t i);

uint8_t term_cursor_col(const Term* t);

/* Raw byte ring, for the hex view - the cooked lines have already dropped
 * escape sequences and control bytes, which is exactly what hex mode is for. */
size_t term_raw_count(const Term* t);
uint8_t term_raw_at(const Term* t, size_t i);

uint32_t term_total_rx(const Term* t);

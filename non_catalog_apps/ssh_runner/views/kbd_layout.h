/* Keyboard key table and geometry (pure C, host-tested; tools/flipper/kbd.py mirrors it).
 *
 * The layout is the community extended keyboard (UART Terminal's uart_text_input, itself
 * derived from the firmware's text_input.c): four rows, one page, every symbol a shell
 * command or a host needs, the firmware's own Backspace/Save key images. Changes: 'd' and
 * 'c' moved to x=17 so columns line up; long-press alternates for '&' ('<') and '|' ('>');
 * a Shift key in place of '%', which is long-press '5' (Shift+5 on a US keyboard).
 * See THIRD_PARTY.md.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define KBD_ROWS     4
#define KBD_MAXKEYS  16
#define KBD_ORIGIN_X 1
#define KBD_ORIGIN_Y 29
#define KBD_SCREEN_W 128
#define KBD_SCREEN_H 64

#define KBD_BS    '\b'
#define KBD_ENTER '\r'
#define KBD_SHIFT '\x0e' /* Shift: once for the next key, twice for caps lock */

typedef struct {
    char key;
    uint8_t x; /* relative to the origin: glyph keys give the glyph's left/baseline, */
    uint8_t y; /* image keys (Backspace, Save) their top-left corner */
} KbdKey;

typedef struct {
    uint8_t x, y, w, h; /* absolute screen rectangle (selection box or image) */
} KbdRect;

const KbdKey* kbd_row(uint8_t row, uint8_t* count);
uint8_t kbd_row_len(uint8_t row);
/* Long-press (and Shift) alternate: upper case, '_' -> space, '(' -> ')', ';' -> ':',
 * '5' -> '%' and so on. */
char kbd_shift(char c);
KbdRect kbd_key_rect(const KbdKey* k);
/* Column in `to_row` whose key centre is nearest to key (`row`, `col`): Up/Down. */
uint8_t kbd_nearest(uint8_t row, uint8_t col, uint8_t to_row);

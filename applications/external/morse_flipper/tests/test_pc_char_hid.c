#include "pc_keys.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    bool shift;

    /* Letters map to the a-z usage range, no shift, case-insensitive. */
    assert(morse_pc_hid_usage_for_char('A', &shift) == 0x04U && !shift);
    assert(morse_pc_hid_usage_for_char('a', &shift) == 0x04U && !shift);
    assert(morse_pc_hid_usage_for_char('E', &shift) == 0x08U && !shift);
    assert(morse_pc_hid_usage_for_char('Z', &shift) == 0x1DU && !shift);

    /* Digits: 1-9 are contiguous, 0 is separate, none need shift. */
    assert(morse_pc_hid_usage_for_char('1', &shift) == 0x1EU && !shift);
    assert(morse_pc_hid_usage_for_char('9', &shift) == 0x26U && !shift);
    assert(morse_pc_hid_usage_for_char('0', &shift) == 0x27U && !shift);

    /* Space. */
    assert(morse_pc_hid_usage_for_char(' ', &shift) == 0x2CU && !shift);

    /* Unshifted punctuation the decoder can emit. */
    assert(morse_pc_hid_usage_for_char('.', &shift) == 0x37U && !shift);
    assert(morse_pc_hid_usage_for_char(',', &shift) == 0x36U && !shift);
    assert(morse_pc_hid_usage_for_char('/', &shift) == 0x38U && !shift);
    assert(morse_pc_hid_usage_for_char('-', &shift) == 0x2DU && !shift);
    assert(morse_pc_hid_usage_for_char('=', &shift) == 0x2EU && !shift);
    assert(morse_pc_hid_usage_for_char(';', &shift) == 0x33U && !shift);
    assert(morse_pc_hid_usage_for_char('\'', &shift) == 0x34U && !shift);

    /* Shifted punctuation: usage is the base key, shift flag set. */
    assert(morse_pc_hid_usage_for_char('?', &shift) == 0x38U && shift); /* shift + / */
    assert(morse_pc_hid_usage_for_char(':', &shift) == 0x33U && shift); /* shift + ; */
    assert(morse_pc_hid_usage_for_char('(', &shift) == 0x26U && shift); /* shift + 9 */
    assert(morse_pc_hid_usage_for_char(')', &shift) == 0x27U && shift); /* shift + 0 */
    assert(morse_pc_hid_usage_for_char('+', &shift) == 0x2EU && shift); /* shift + = */
    assert(morse_pc_hid_usage_for_char('@', &shift) == 0x1FU && shift); /* shift + 2 */
    assert(morse_pc_hid_usage_for_char('!', &shift) == 0x1EU && shift); /* shift + 1 */
    assert(morse_pc_hid_usage_for_char('"', &shift) == 0x34U && shift); /* shift + ' */
    assert(morse_pc_hid_usage_for_char('&', &shift) == 0x24U && shift); /* shift + 7 */
    assert(morse_pc_hid_usage_for_char('_', &shift) == 0x2DU && shift); /* shift + - */

    /* Decoder control markers and prosign tokens must not type anything. */
    assert(morse_pc_hid_usage_for_char('|', &shift) == 0U && !shift);
    assert(morse_pc_hid_usage_for_char('#', &shift) == 0U && !shift);
    assert(morse_pc_hid_usage_for_char((char)0x80, &shift) == 0U && !shift);
    assert(morse_pc_hid_usage_for_char((char)0x85, &shift) == 0U && !shift);
    assert(morse_pc_hid_usage_for_char('\0', &shift) == 0U && !shift);

    /* A NULL shift pointer must be tolerated. */
    assert(morse_pc_hid_usage_for_char('e', NULL) == 0x08U);

    puts("test_pc_char_hid: passed");
    return 0;
}

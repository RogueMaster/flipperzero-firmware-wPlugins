/*
 * Purpose: Define private CW prosign tokens shared by trainer and UI code.
 * Owns: token identifiers and token label/conversion declarations.
 * Depends on: host-safe integer types only.
 * Tests: tests/test_cw_token.c and tests/test_prompt_font.c.
 */

#ifndef MORSE_FLIPPER_CW_TOKEN_H
#define MORSE_FLIPPER_CW_TOKEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MORSE_FLIPPER_CW_TOKEN_SK    0x80U
#define MORSE_FLIPPER_CW_TOKEN_BK    0x81U
#define MORSE_FLIPPER_CW_TOKEN_CT_KA 0x82U
#define MORSE_FLIPPER_CW_TOKEN_VE_SN 0x83U
#define MORSE_FLIPPER_CW_TOKEN_AA    0x84U
#define MORSE_FLIPPER_CW_TOKEN_SOS   0x85U

bool morse_flipper_cw_token_is_private(uint8_t ch);
const char* morse_flipper_cw_token_label(uint8_t ch);
const char* morse_flipper_cw_token_text(uint8_t ch);
uint16_t morse_flipper_cw_token_code(uint8_t ch);
bool morse_flipper_cw_token_parse(const char* text, uint8_t* token, size_t* consumed);
size_t morse_flipper_cw_token_expand_text(char* out, size_t out_sz, const char* text);

#endif

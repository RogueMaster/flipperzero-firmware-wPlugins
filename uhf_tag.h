#pragma once

#include "uhf_types.h"

#include <stdbool.h>
#include <stddef.h>

/* Captured from a tag response before later reader commands reuse the RX buffer. */
typedef struct {
    char epc[UHF_EPC_HEX_MAX + 1U];
    char tid[UHF_TID_HEX_MAX + 1U];
    uint16_t pc;
    bool valid;
} UhfTidReadResult;

bool uhf_tag_parse_custom_tid(
    const uint8_t* data, size_t data_len, uint8_t words, UhfTidReadResult* result);
bool uhf_tag_parse_tid_reply(
    const uint8_t* data, size_t data_len, uint8_t words, UhfTidReadResult* result);
bool uhf_tag_epc_hex_to_ascii(const char* epc_hex, char* out, size_t out_size);
bool uhf_tag_ascii_to_epc_hex(const char* ascii, char* out, size_t out_size);
/* Zero is a TID-only tag. Other lengths are whole words from 4 through 48 bytes. */
bool uhf_tag_epc_length_ok(size_t epc_len);
/* Bytes 4-5 are the XTID word. 12 means a 48-bit serial and no extended
   header, including UCODE 8 (0x2000). Zero means keep the bytes the chip
   returned. */
size_t uhf_tag_tid_length(const uint8_t* tid, size_t len);
const char* uhf_tag_bank_name(UhfTagBank bank);
const char* uhf_tag_bank_button_name(UhfTagBank bank);

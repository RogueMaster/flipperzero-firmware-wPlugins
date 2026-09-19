#pragma once

#include "uhf_types.h"

#include <stdbool.h>
#include <stddef.h>

bool uhf_tag_epc_hex_to_ascii(const char* epc_hex, char* out, size_t out_size);
bool uhf_tag_ascii_to_epc_hex(const char* ascii, char* out, size_t out_size);
const char* uhf_tag_bank_name(UhfTagBank bank);
const char* uhf_tag_bank_button_name(UhfTagBank bank);

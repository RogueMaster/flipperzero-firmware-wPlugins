#include "uhf_tag.h"
#include "uhf_protocol.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

bool uhf_tag_epc_hex_to_ascii(const char* epc_hex, char* out, size_t out_size) {
    uint8_t bytes[UHF_EPC_HEX_MAX / 2U];
    size_t length = 0U;
    if(!out || out_size < 2U ||
       !uhf_protocol_hex_to_bytes(epc_hex, bytes, sizeof(bytes), &length) || length == 0U ||
       length >= out_size) {
        return false;
    }
    for(size_t i = 0U; i < length; i++) {
        if(bytes[i] < 0x20U || bytes[i] > 0x7EU) return false;
    }
    memcpy(out, bytes, length);
    out[length] = '\0';
    while(length > 0U && out[length - 1U] == ' ')
        out[--length] = '\0';
    return out[0] != '\0';
}

bool uhf_tag_ascii_to_epc_hex(const char* ascii, char* out, size_t out_size) {
    if(!ascii || !out) return false;
    const size_t length = strlen(ascii);
    if(length == 0U || length > 12U) return false;
    uint8_t bytes[12U];
    memset(bytes, ' ', sizeof(bytes));
    for(size_t i = 0U; i < length; i++) {
        const unsigned char c = (unsigned char)ascii[i];
        if(c < 0x20U || c > 0x7EU) return false;
        bytes[i] = c;
    }
    return uhf_protocol_bytes_to_hex(bytes, sizeof(bytes), out, out_size);
}

const char* uhf_tag_bank_name(UhfTagBank bank) {
    if(bank == UhfTagBankReserved) return "Reserved";
    if(bank == UhfTagBankTid) return "TID";
    if(bank == UhfTagBankUser) return "User Data";
    return "EPC";
}

const char* uhf_tag_bank_button_name(UhfTagBank bank) {
    if(bank == UhfTagBankReserved) return "RSVD";
    if(bank == UhfTagBankTid) return "TID";
    if(bank == UhfTagBankUser) return "User";
    return "EPC";
}

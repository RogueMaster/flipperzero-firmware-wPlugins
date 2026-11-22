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

bool uhf_tag_epc_length_ok(size_t epc_len) {
    return epc_len == 0U ||
           (epc_len >= 4U && epc_len <= (UHF_EPC_HEX_MAX / 2U) && (epc_len % 2U) == 0U);
}

size_t uhf_tag_tid_length(const uint8_t* tid, size_t len) {
    if(!tid || len < 6U) return 0U;
    const uint16_t xtid = (uint16_t)(((uint16_t)tid[4] << 8U) | tid[5]);
    /* Bit 13 set and bit 15 clear: serial present, no extended header. */
    if((xtid & 0xA000U) == 0x2000U) return 12U;
    return 0U;
}

static bool uhf_tag_store_tid_result(
    uint16_t pc,
    const uint8_t* epc,
    size_t epc_len,
    const uint8_t* tid,
    size_t tid_len,
    UhfTidReadResult* result) {
    if(!uhf_tag_epc_length_ok(epc_len) || epc_len != ((size_t)(pc >> 11U) * 2U) ||
       tid_len < 4U || (tid_len & 1U)) return false;
    if(!uhf_protocol_bytes_to_hex(epc, epc_len, result->epc, sizeof(result->epc)) ||
       !uhf_protocol_bytes_to_hex(tid, tid_len, result->tid, sizeof(result->tid))) return false;
    result->pc = pc;
    result->valid = true;
    return true;
}

bool uhf_tag_parse_custom_tid(
    const uint8_t* data, size_t data_len, uint8_t words, UhfTidReadResult* result) {
    if(!result) return false;
    result->valid = false;
    const size_t wanted = (size_t)words * 2U;
    if(!data || data_len < 6U || wanted < 4U || wanted > 16U) return false;
    const uint16_t pc = (uint16_t)(((uint16_t)data[1] << 8U) | data[2]);
    const size_t epc_len = (size_t)(pc >> 11U) * 2U;
    const size_t after_crc = 3U + epc_len + 2U;
    /* A truncated EPC is malformed, not evidence of an empty EPC. */
    if(!uhf_tag_epc_length_ok(epc_len) || after_crc >= data_len) return false;
    const size_t remain = data_len - after_crc;
    size_t offset = after_crc;
    size_t tid_len = wanted;
    const size_t declared = data[after_crc];
    if(declared >= 4U && (declared % 2U) == 0U && declared <= wanted &&
       1U + declared <= remain) {
        offset++;
        tid_len = declared;
    } else if(remain >= 1U + wanted + 7U && declared == 0U) {
        offset++;
    } else if(remain != wanted && remain < wanted + 7U) {
        return false;
    }
    return uhf_tag_store_tid_result(pc, &data[3], epc_len, &data[offset], tid_len, result);
}

bool uhf_tag_parse_tid_reply(
    const uint8_t* data, size_t data_len, uint8_t words, UhfTidReadResult* result) {
    if(!result) return false;
    result->valid = false;
    if(!data || data_len < 8U) return false;
    const size_t record_len = data[2];
    const size_t record_end = 3U + record_len;
    if(record_end + 3U > data_len) return false;
    size_t read_len = data[record_end];
    if(record_end + 4U <= data_len) {
        const size_t extended = ((size_t)data[record_end] << 8U) | data[record_end + 1U];
        if(extended > 0U && extended <= record_len) read_len = extended;
    }
    if(read_len < 4U || (read_len & 1U) || read_len > (size_t)words * 2U ||
       record_len < read_len + 4U) return false;
    const size_t epc_len = record_len - read_len - 4U;
    const uint16_t pc = (uint16_t)(((uint16_t)data[3] << 8U) | data[4]);
    return uhf_tag_store_tid_result(
        pc, &data[5], epc_len, &data[record_end - read_len], read_len, result);
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

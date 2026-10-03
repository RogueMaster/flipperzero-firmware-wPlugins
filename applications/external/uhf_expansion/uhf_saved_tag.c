#include "uhf_saved_tag.h"
#include "uhf_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool uhf_saved_hex(const char* value, size_t maximum, bool required) {
    const size_t len = strlen(value);
    uint8_t bytes[UHF_USER_HEX_MAX / 2U];
    size_t byte_count;
    return (!required || len > 0U) && len <= maximum && len % 4U == 0U &&
           uhf_protocol_hex_to_bytes(value, bytes, sizeof(bytes), &byte_count);
}

static bool uhf_saved_number(const char* value, int base, uint32_t maximum, uint32_t* out) {
    if(!value[0]) return false;
    for(const char* p = value; *p; p++) {
        if(!((*p >= '0' && *p <= '9') ||
             (base == 16 && ((*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f')))))
            return false;
    }
    if(strlen(value) > (base == 16 ? 4U : 10U)) return false;
    uint32_t number = 0U;
    for(const char* p = value; *p; p++) {
        const uint32_t digit = *p <= '9' ? (uint32_t)(*p - '0') :
                               *p <= 'F' ? (uint32_t)(*p - 'A') + 10U :
                                           (uint32_t)(*p - 'a') + 10U;
        if(number > (maximum - digit) / (uint32_t)base) return false;
        number = number * (uint32_t)base + digit;
    }
    *out = number;
    return true;
}

static bool uhf_saved_timestamp(const char* value) {
    if(!value[0]) return true;
    if(strlen(value) != 19U) return false;
    for(size_t i = 0; i < 19U; i++) {
        const char separator = i == 4U || i == 7U   ? '-' :
                               i == 10U             ? ' ' :
                               i == 13U || i == 16U ? ':' :
                                                      '\0';
        if(separator ? value[i] != separator : value[i] < '0' || value[i] > '9') return false;
    }
    return true;
}

bool uhf_saved_tag_parse(const char* text, UhfSavedTag* out) {
    if(!text || !out || strlen(text) >= UHF_SAVED_TAG_TEXT_MAX) return false;
    char buffer[UHF_SAVED_TAG_TEXT_MAX];
    strcpy(buffer, text);
    UhfSavedTag tag = {0};
    unsigned seen = 0U;
    char* line = buffer;
    while(line && *line) {
        char* next = strchr(line, '\n');
        if(next) *next++ = '\0';
        const size_t length = strlen(line);
        if(length && line[length - 1U] == '\r') line[length - 1U] = '\0';
        char* colon = strchr(line, ':');
        if(!colon) {
            if(*line) return false;
            line = next;
            continue;
        }
        *colon++ = '\0';
        if(*colon == ' ') colon++;
        static const char* const keys[] = {
            "Filetype", "Version", "EPC", "PC", "RSSI", "TID", "USER", "Timestamp"};
        unsigned field = 0U;
        while(field < 8U && strcmp(line, keys[field]))
            field++;
        if(field == 8U) return false;
        if(seen & (1U << field)) return false;
        seen |= 1U << field;
        uint32_t number;
        switch(field) {
        case 0:
            if(strcmp(colon, "UHF Tag")) return false;
            break;
        case 1:
            if(strcmp(colon, "1")) return false;
            break;
        case 2:
            if(!uhf_saved_hex(colon, UHF_EPC_HEX_MAX, true)) return false;
            strcpy(tag.epc, colon);
            break;
        case 3:
            if(strlen(colon) != 4U || !uhf_saved_number(colon, 16, UINT16_MAX, &number))
                return false;
            tag.pc = (uint16_t)number;
            break;
        case 4:
            if(!uhf_saved_number(colon, 10, UINT32_MAX, &tag.rssi)) return false;
            break;
        case 5:
            if(!uhf_saved_hex(colon, UHF_TID_HEX_MAX, false)) return false;
            strcpy(tag.tid, colon);
            break;
        case 6:
            if(!uhf_saved_hex(colon, UHF_USER_HEX_MAX, false)) return false;
            strcpy(tag.user, colon);
            break;
        case 7:
            if(!uhf_saved_timestamp(colon)) return false;
            strcpy(tag.timestamp, colon);
            break;
        }
        line = next;
    }
    if((seen & 0x0FU) != 0x0FU) return false;
    *out = tag;
    return true;
}

bool uhf_saved_tag_format(const UhfSavedTag* tag, char* out, size_t size) {
    if(!tag || !out || !uhf_saved_hex(tag->epc, UHF_EPC_HEX_MAX, true) ||
       !uhf_saved_hex(tag->tid, UHF_TID_HEX_MAX, false) ||
       !uhf_saved_hex(tag->user, UHF_USER_HEX_MAX, false) || !uhf_saved_timestamp(tag->timestamp))
        return false;
    const int len = snprintf(
        out,
        size,
        "Filetype: UHF Tag\nVersion: 1\nEPC: %s\nPC: %04X\nRSSI: %lu\n"
        "TID: %s\nUSER: %s\nTimestamp: %s\n",
        tag->epc,
        tag->pc,
        (unsigned long)tag->rssi,
        tag->tid,
        tag->user,
        tag->timestamp);
    return len > 0 && (size_t)len < size;
}

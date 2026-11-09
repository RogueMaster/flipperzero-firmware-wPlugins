#include "saved_program.h"
#include "transfer_protocol.h"
#include "keyboard_layout.h"
#include <string.h>

ClipboardSaved clipboard_saved_for_keyboard(
    const ClipboardSaved* saved,
    const ClipboardKeyboardSettings* settings) {
    ClipboardSaved run = *saved;
    if(settings->override_saved) {
        run.layout = settings->layout;
        run.target = settings->target;
    }
    return run;
}

static bool valid_name(const char* name, size_t length) {
    if(!length || length >= CLIPBOARD_SAVED_NAME) return false;
    for(size_t i = 0; i < length;) {
        uint8_t byte = (uint8_t)name[i++];
        if(byte < 32 || byte == 127) return false;
        if(byte < 128) continue;
        unsigned extra;
        uint32_t value, minimum;
        if(byte >= 0xC2 && byte <= 0xDF) {
            extra = 1;
            value = byte & 31;
            minimum = 128;
        } else if(byte >= 0xE0 && byte <= 0xEF) {
            extra = 2;
            value = byte & 15;
            minimum = 2048;
        } else if(byte >= 0xF0 && byte <= 0xF4) {
            extra = 3;
            value = byte & 7;
            minimum = 65536;
        } else
            return false;
        if(extra > length - i) return false;
        for(unsigned n = 0; n < extra; n++) {
            byte = (uint8_t)name[i++];
            if((byte & 0xC0) != 0x80) return false;
            value = (value << 6) | (byte & 63);
        }
        if(value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF) ||
           (value >= 0x80 && value <= 0x9F))
            return false;
    }
    return true;
}

bool clipboard_saved_encode(const ClipboardSaved* saved, uint8_t output[CLIPBOARD_SAVED_HEADER]) {
    size_t length = 0;
    while(length < sizeof(saved->name) && saved->name[length])
        length++;
    bool nonzero = false;
    for(unsigned i = 0; i < 16; i++)
        nonzero |= saved->id[i] != 0;
    if(!nonzero || saved->layout >= CLIPBOARD_LAYOUT_COUNT || saved->target > 2 ||
       saved->size < 6 || saved->size > CLIPBOARD_PROGRAM_MAX || !valid_name(saved->name, length))
        return false;
    memset(output, 0, CLIPBOARD_SAVED_HEADER);
    memcpy(output, "BCS4", 4);
    output[4] = saved->layout;
    output[5] = saved->target;
    output[6] = (uint8_t)length;
    clipboard_write_u32(output + 8, saved->size);
    clipboard_write_u32(output + 12, saved->crc);
    memcpy(output + 16, saved->id, 16);
    memcpy(output + 32, saved->name, length);
    clipboard_write_u32(
        output + 124, clipboard_crc_update(0xFFFFFFFFU, output, 124) ^ 0xFFFFFFFFU);
    return true;
}

bool clipboard_saved_decode(const uint8_t input[CLIPBOARD_SAVED_HEADER], ClipboardSaved* saved) {
    if(memcmp(input, "BCS4", 4) != 0 || input[6] == 0 || input[6] >= CLIPBOARD_SAVED_NAME ||
       clipboard_read_u32(input + 124) !=
           (clipboard_crc_update(0xFFFFFFFFU, input, 124) ^ 0xFFFFFFFFU))
        return false;
    memset(saved, 0, sizeof(*saved));
    saved->layout = input[4];
    saved->target = input[5];
    saved->size = clipboard_read_u32(input + 8);
    saved->crc = clipboard_read_u32(input + 12);
    memcpy(saved->id, input + 16, 16);
    memcpy(saved->name, input + 32, input[6]);
    uint8_t canonical[CLIPBOARD_SAVED_HEADER];
    return clipboard_saved_encode(saved, canonical) &&
           memcmp(canonical, input, sizeof(canonical)) == 0;
}

bool clipboard_saved_verify(const ClipboardSaved* saved, const ClipboardReader* program) {
    if(program->size != saved->size) return false;
    uint32_t crc = 0xFFFFFFFFU;
    uint8_t bytes[256];
    for(size_t offset = 0; offset < program->size;) {
        size_t size = program->size - offset;
        if(size > sizeof(bytes)) size = sizeof(bytes);
        if(!program->read(program->context, offset, bytes, size)) return false;
        crc = clipboard_crc_update(crc, bytes, size);
        offset += size;
    }
    return (crc ^ 0xFFFFFFFFU) == saved->crc;
}

void clipboard_saved_id(const uint8_t id[16], char output[33]) {
    static const char hex[] = "0123456789abcdef";
    for(unsigned i = 0; i < 16; i++) {
        output[i * 2] = hex[id[i] >> 4];
        output[i * 2 + 1] = hex[id[i] & 15];
    }
    output[32] = 0;
}

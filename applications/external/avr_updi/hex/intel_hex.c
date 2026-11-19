#include "intel_hex.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct IntelHexParser {
    uint32_t max_address;
    uint16_t page_size;
    IntelHexPageCallback on_page;
    void* context;
    uint32_t base;
    bool have_page;
    bool eof;
    uint32_t page_address;
    uint8_t page[INTEL_HEX_MAX_PAGE_SIZE];
};

struct IntelHexWriter {
    IntelHexWriteFn write;
    void* context;
    bool have_ela;
    uint16_t ela;
    uint32_t cursor;
    bool have_cursor;
};

static void hex_set_error(char* error, size_t error_size, const char* message) {
    if(error == NULL || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
}

static int hex_nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_byte(const char* text) {
    int hi = hex_nibble(text[0]);
    int lo = hex_nibble(text[1]);
    if(hi < 0 || lo < 0) return -1;
    return (hi << 4) | lo;
}

static char hex_digit(uint8_t value) {
    return (char)(value < 10 ? '0' + value : 'A' + (value - 10));
}

static void hex_put_byte(char* dest, uint8_t value) {
    dest[0] = hex_digit((uint8_t)(value >> 4));
    dest[1] = hex_digit((uint8_t)(value & 0x0F));
}

static bool hex_emit_page(IntelHexParser* parser) {
    if(!parser->have_page) return true;
    parser->have_page = false;
    if(parser->on_page == NULL) return true;
    return parser->on_page(parser->page_address, parser->page, parser->page_size, parser->context);
}

static bool hex_place(
    IntelHexParser* parser,
    uint32_t address,
    uint8_t value,
    char* error,
    size_t error_size) {
    if(parser->eof) {
        hex_set_error(error, error_size, "Data after EOF");
        return false;
    }
    if(address >= parser->max_address) {
        hex_set_error(error, error_size, "Address past end of flash");
        return false;
    }

    uint32_t page = address & ~(uint32_t)(parser->page_size - 1u);
    if(!parser->have_page) {
        parser->have_page = true;
        parser->page_address = page;
        memset(parser->page, 0xFF, parser->page_size);
    } else if(page != parser->page_address) {
        if(page < parser->page_address) {
            hex_set_error(error, error_size, "HEX addresses go backwards");
            return false;
        }
        if(!hex_emit_page(parser)) {
            hex_set_error(error, error_size, "Page callback failed");
            return false;
        }
        parser->have_page = true;
        parser->page_address = page;
        memset(parser->page, 0xFF, parser->page_size);
    }

    parser->page[address - parser->page_address] = value;
    return true;
}

IntelHexParser* intel_hex_parser_alloc(
    uint32_t max_address,
    uint16_t page_size,
    IntelHexPageCallback on_page,
    void* context) {
    if(page_size == 0 || page_size > INTEL_HEX_MAX_PAGE_SIZE ||
       (page_size & (page_size - 1u)) != 0) {
        return NULL;
    }
    IntelHexParser* parser = calloc(1, sizeof(IntelHexParser));
    if(parser == NULL) return NULL;
    parser->max_address = max_address;
    parser->page_size = page_size;
    parser->on_page = on_page;
    parser->context = context;
    return parser;
}

void intel_hex_parser_free(IntelHexParser* parser) {
    free(parser);
}

bool intel_hex_parser_feed(
    IntelHexParser* parser,
    const char* line,
    char* error,
    size_t error_size) {
    if(parser == NULL || line == NULL) {
        hex_set_error(error, error_size, "Bad HEX parser state");
        return false;
    }

    while(*line == ' ' || *line == '\t')
        line++;
    if(*line == '\0' || *line == '#' || *line == ';') return true;
    if(*line != ':') {
        hex_set_error(error, error_size, "HEX line missing ':'");
        return false;
    }
    line++;

    uint8_t raw[255 + 5];
    size_t raw_len = 0;
    while(line[0] != '\0' && line[1] != '\0') {
        if(line[0] == ' ' || line[0] == '\t') break;
        int byte = hex_byte(line);
        if(byte < 0) {
            hex_set_error(error, error_size, "HEX line has a bad digit");
            return false;
        }
        if(raw_len >= sizeof(raw)) {
            hex_set_error(error, error_size, "HEX record too long");
            return false;
        }
        raw[raw_len++] = (uint8_t)byte;
        line += 2;
    }
    if(raw_len < 5) {
        hex_set_error(error, error_size, "HEX record too short");
        return false;
    }

    uint8_t count = raw[0];
    if(raw_len != (size_t)count + 5u) {
        hex_set_error(error, error_size, "HEX record length mismatch");
        return false;
    }

    uint8_t sum = 0;
    for(size_t i = 0; i < raw_len; i++)
        sum = (uint8_t)(sum + raw[i]);
    if(sum != 0) {
        hex_set_error(error, error_size, "HEX checksum mismatch");
        return false;
    }

    uint16_t offset = (uint16_t)((raw[1] << 8) | raw[2]);
    uint8_t type = raw[3];
    const uint8_t* data = &raw[4];

    if(type == 0x00) {
        if(parser->base > UINT32_MAX - offset) {
            hex_set_error(error, error_size, "HEX address overflow");
            return false;
        }
        uint32_t address = parser->base + offset;
        for(uint8_t i = 0; i < count; i++) {
            if(!hex_place(parser, address, data[i], error, error_size)) return false;
            if(address == UINT32_MAX) {
                hex_set_error(error, error_size, "HEX address overflow");
                return false;
            }
            address++;
        }
        return true;
    }

    if(type == 0x01) {
        if(count != 0) {
            hex_set_error(error, error_size, "Bad EOF record");
            return false;
        }
        if(!hex_emit_page(parser)) {
            hex_set_error(error, error_size, "Page callback failed");
            return false;
        }
        parser->eof = true;
        return true;
    }

    if(type == 0x02 || type == 0x04) {
        if(count != 2) {
            hex_set_error(error, error_size, "Bad extended address record");
            return false;
        }
        uint16_t value = (uint16_t)((data[0] << 8) | data[1]);
        if(type == 0x04) {
            parser->base = (uint32_t)value << 16;
        } else {
            parser->base = (uint32_t)value << 4;
        }
        return true;
    }

    if(type == 0x03 || type == 0x05) return true;

    hex_set_error(error, error_size, "Unsupported HEX record type");
    return false;
}

bool intel_hex_parser_finish(IntelHexParser* parser, char* error, size_t error_size) {
    if(parser == NULL) {
        hex_set_error(error, error_size, "Bad HEX parser state");
        return false;
    }
    if(!hex_emit_page(parser)) {
        hex_set_error(error, error_size, "Page callback failed");
        return false;
    }
    return true;
}

IntelHexWriter* intel_hex_writer_alloc(IntelHexWriteFn write, void* context) {
    IntelHexWriter* writer = calloc(1, sizeof(IntelHexWriter));
    if(writer == NULL) return NULL;
    writer->write = write;
    writer->context = context;
    return writer;
}

void intel_hex_writer_free(IntelHexWriter* writer) {
    free(writer);
}

static bool
    hex_write_line(IntelHexWriter* writer, const char* line, char* error, size_t error_size) {
    if(writer->write == NULL || !writer->write(line, writer->context)) {
        hex_set_error(error, error_size, "HEX write failed");
        return false;
    }
    return true;
}

static bool hex_write_record(
    IntelHexWriter* writer,
    uint16_t offset,
    uint8_t type,
    const uint8_t* data,
    uint8_t count,
    char* error,
    size_t error_size) {
    char line[1 + 2 + 4 + 2 + 16 * 2 + 2 + 1];
    size_t pos = 0;
    line[pos++] = ':';
    uint8_t sum = count;
    hex_put_byte(&line[pos], count);
    pos += 2;
    uint8_t offset_hi = (uint8_t)(offset >> 8);
    uint8_t offset_lo = (uint8_t)offset;
    sum = (uint8_t)(sum + offset_hi + offset_lo + type);
    hex_put_byte(&line[pos], offset_hi);
    pos += 2;
    hex_put_byte(&line[pos], offset_lo);
    pos += 2;
    hex_put_byte(&line[pos], type);
    pos += 2;
    for(uint8_t i = 0; i < count; i++) {
        sum = (uint8_t)(sum + data[i]);
        hex_put_byte(&line[pos], data[i]);
        pos += 2;
    }
    hex_put_byte(&line[pos], (uint8_t)(0u - sum));
    pos += 2;
    line[pos] = '\0';
    return hex_write_line(writer, line, error, error_size);
}

static bool
    hex_ensure_ela(IntelHexWriter* writer, uint32_t address, char* error, size_t error_size) {
    uint16_t ela = (uint16_t)(address >> 16);
    if(!writer->have_ela) {
        writer->have_ela = true;
        writer->ela = 0;
    }
    if(writer->ela == ela) return true;
    uint8_t bytes[2] = {(uint8_t)(ela >> 8), (uint8_t)ela};
    if(!hex_write_record(writer, 0, 0x04, bytes, 2, error, error_size)) return false;
    writer->ela = ela;
    return true;
}

bool intel_hex_writer_feed(
    IntelHexWriter* writer,
    uint32_t address,
    const uint8_t* data,
    size_t length,
    char* error,
    size_t error_size) {
    if(writer == NULL || (data == NULL && length != 0)) {
        hex_set_error(error, error_size, "Bad HEX writer state");
        return false;
    }
    if(writer->have_cursor && address < writer->cursor) {
        hex_set_error(error, error_size, "HEX addresses go backwards");
        return false;
    }

    size_t offset = 0;
    while(offset < length) {
        uint32_t at = address + (uint32_t)offset;
        if(!hex_ensure_ela(writer, at, error, error_size)) return false;

        uint32_t segment_end = (at & 0xFFFF0000u) + 0x10000u;
        size_t room = (size_t)(segment_end - at);
        if(room > 16u) room = 16u;
        if(room > length - offset) room = length - offset;

        if(!hex_write_record(
               writer, (uint16_t)at, 0x00, data + offset, (uint8_t)room, error, error_size)) {
            return false;
        }
        offset += room;
    }

    writer->have_cursor = true;
    writer->cursor = address + (uint32_t)length;
    return true;
}

bool intel_hex_writer_finish(IntelHexWriter* writer, char* error, size_t error_size) {
    if(writer == NULL) {
        hex_set_error(error, error_size, "Bad HEX writer state");
        return false;
    }
    return hex_write_line(writer, ":00000001FF", error, error_size);
}

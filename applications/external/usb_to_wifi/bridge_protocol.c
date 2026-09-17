#include "bridge_protocol.h"

#include <string.h>

static const uint8_t fib_magic[FIB_MAGIC_SIZE] = {'F', 'I', 'B', 'P'};

static uint32_t fib_crc32_begin(void) {
    return 0xFFFFFFFFU;
}

static uint32_t fib_crc32_update(uint32_t crc, const uint8_t* data, size_t length) {
    for(size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for(uint8_t bit = 0; bit < 8U; ++bit) {
            const uint32_t mask = (uint32_t) - (int32_t)(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return crc;
}

static uint32_t fib_crc32_finish(uint32_t crc) {
    return crc ^ 0xFFFFFFFFU;
}

uint32_t fib_crc32(const uint8_t* data, size_t length) {
    if(!data && length != 0U) return 0U;
    return fib_crc32_finish(fib_crc32_update(fib_crc32_begin(), data, length));
}

uint16_t fib_read_u16_le(const uint8_t* bytes) {
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

uint32_t fib_read_u32_le(const uint8_t* bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) | ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

uint64_t fib_read_u64_le(const uint8_t* bytes) {
    return (uint64_t)fib_read_u32_le(bytes) | ((uint64_t)fib_read_u32_le(bytes + 4U) << 32U);
}

void fib_write_u16_le(uint8_t* bytes, uint16_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
}

void fib_write_u32_le(uint8_t* bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

void fib_write_u64_le(uint8_t* bytes, uint64_t value) {
    fib_write_u32_le(bytes, (uint32_t)value);
    fib_write_u32_le(bytes + 4U, (uint32_t)(value >> 32U));
}

size_t fib_frame_encoded_size(uint32_t payload_length) {
    if(payload_length > FIB_MAX_FRAME_PAYLOAD) return 0U;
    return FIB_HEADER_SIZE + (size_t)payload_length + FIB_FRAME_CRC_SIZE;
}

bool fib_frame_encode(const FibFrame* frame, uint8_t* output, size_t capacity, size_t* written) {
    if(written) *written = 0U;
    if(!frame || !output || !written || frame->payload_length > FIB_MAX_FRAME_PAYLOAD) {
        return false;
    }

    const size_t encoded_size = fib_frame_encoded_size(frame->payload_length);
    if(capacity < encoded_size) return false;

    memcpy(output, fib_magic, FIB_MAGIC_SIZE);
    output[4] = frame->major;
    output[5] = frame->minor;
    output[6] = FIB_HEADER_SIZE;
    output[7] = (uint8_t)frame->type;
    fib_write_u16_le(output + 8U, frame->flags);
    fib_write_u16_le(output + 10U, 0U);
    fib_write_u32_le(output + 12U, frame->request_id);
    fib_write_u32_le(output + 16U, frame->sequence);
    fib_write_u32_le(output + 20U, frame->payload_length);

    const uint32_t header_crc = fib_crc32(output, FIB_HEADER_CRC_OFFSET);
    fib_write_u32_le(output + FIB_HEADER_CRC_OFFSET, header_crc);

    if(frame->payload_length != 0U) {
        memcpy(output + FIB_HEADER_SIZE, frame->payload, frame->payload_length);
    }

    uint32_t frame_crc = fib_crc32_begin();
    frame_crc = fib_crc32_update(frame_crc, output, FIB_HEADER_CRC_OFFSET);
    frame_crc =
        fib_crc32_update(frame_crc, output + FIB_HEADER_SIZE, (size_t)frame->payload_length);
    frame_crc = fib_crc32_finish(frame_crc);
    fib_write_u32_le(output + FIB_HEADER_SIZE + frame->payload_length, frame_crc);
    *written = encoded_size;
    return true;
}

static void fib_parser_seek_byte(FibParser* parser, uint8_t byte) {
    if(byte == fib_magic[parser->magic_matched]) {
        ++parser->magic_matched;
        if(parser->magic_matched == FIB_MAGIC_SIZE) {
            memcpy(parser->frame_bytes, fib_magic, FIB_MAGIC_SIZE);
            parser->used = FIB_MAGIC_SIZE;
            parser->expected = FIB_HEADER_SIZE;
            parser->magic_matched = 0U;
            parser->reading_frame = true;
        }
    } else {
        parser->magic_matched = (byte == fib_magic[0]) ? 1U : 0U;
    }
}

static void fib_parser_report(FibParser* parser, FibParseError error) {
    if(parser->error_callback) parser->error_callback(error, parser->callback_context);
}

void fib_parser_reset(FibParser* parser) {
    if(!parser) return;
    parser->used = 0U;
    parser->expected = 0U;
    parser->magic_matched = 0U;
    parser->reading_frame = false;
    memset(&parser->frame, 0, sizeof(parser->frame));
}

void fib_parser_init(
    FibParser* parser,
    FibFrameCallback frame_callback,
    FibParseErrorCallback error_callback,
    void* context) {
    if(!parser) return;
    memset(parser, 0, sizeof(*parser));
    parser->frame_callback = frame_callback;
    parser->error_callback = error_callback;
    parser->callback_context = context;
}

static bool fib_parser_validate_header(FibParser* parser) {
    const uint8_t* header = parser->frame_bytes;
    if(header[6] != FIB_HEADER_SIZE || fib_read_u16_le(header + 10U) != 0U) {
        fib_parser_report(parser, FibParseErrorInvalidHeader);
        return false;
    }

    const uint32_t expected_crc = fib_read_u32_le(header + FIB_HEADER_CRC_OFFSET);
    if(fib_crc32(header, FIB_HEADER_CRC_OFFSET) != expected_crc) {
        fib_parser_report(parser, FibParseErrorBadHeaderCrc);
        return false;
    }

    const uint32_t payload_length = fib_read_u32_le(header + 20U);
    if(payload_length > FIB_MAX_FRAME_PAYLOAD) {
        fib_parser_report(parser, FibParseErrorPayloadTooLarge);
        return false;
    }

    parser->frame.major = header[4];
    parser->frame.minor = header[5];
    parser->frame.type = (FibMessageType)header[7];
    parser->frame.flags = fib_read_u16_le(header + 8U);
    parser->frame.request_id = fib_read_u32_le(header + 12U);
    parser->frame.sequence = fib_read_u32_le(header + 16U);
    parser->frame.payload_length = payload_length;
    parser->expected = FIB_HEADER_SIZE + (size_t)payload_length + FIB_FRAME_CRC_SIZE;
    return true;
}

static bool fib_parser_validate_frame(FibParser* parser) {
    const uint32_t payload_length = parser->frame.payload_length;
    const uint32_t expected_crc =
        fib_read_u32_le(parser->frame_bytes + FIB_HEADER_SIZE + payload_length);
    uint32_t actual_crc = fib_crc32_begin();
    actual_crc = fib_crc32_update(actual_crc, parser->frame_bytes, FIB_HEADER_CRC_OFFSET);
    actual_crc = fib_crc32_update(
        actual_crc, parser->frame_bytes + FIB_HEADER_SIZE, (size_t)payload_length);
    actual_crc = fib_crc32_finish(actual_crc);
    if(actual_crc != expected_crc) {
        fib_parser_report(parser, FibParseErrorBadFrameCrc);
        return false;
    }

    if(payload_length != 0U) {
        memcpy(
            parser->frame.payload, parser->frame_bytes + FIB_HEADER_SIZE, (size_t)payload_length);
    }
    return true;
}

static size_t fib_parser_find_magic(const uint8_t* bytes, size_t length, size_t start) {
    for(size_t index = start; index + FIB_MAGIC_SIZE <= length; ++index) {
        if(memcmp(bytes + index, fib_magic, FIB_MAGIC_SIZE) == 0) return index;
    }
    return length;
}

static uint8_t fib_parser_magic_suffix(const uint8_t* bytes, size_t length) {
    size_t suffix_length = (length < FIB_MAGIC_SIZE - 1U) ? length : FIB_MAGIC_SIZE - 1U;
    while(suffix_length != 0U) {
        if(memcmp(bytes + length - suffix_length, fib_magic, suffix_length) == 0) {
            return (uint8_t)suffix_length;
        }
        --suffix_length;
    }
    return 0U;
}

/*
 * An invalid candidate can contain the beginning (or all) of the next frame.
 * Re-scan in-place so recovery does not add a frame-sized worker stack buffer.
 */
static void fib_parser_recover_buffered(FibParser* parser, size_t available, size_t search_start) {
    while(available != 0U) {
        const size_t magic_offset =
            fib_parser_find_magic(parser->frame_bytes, available, search_start);
        if(magic_offset == available) {
            const uint8_t matched = fib_parser_magic_suffix(parser->frame_bytes, available);
            fib_parser_reset(parser);
            parser->magic_matched = matched;
            return;
        }

        if(magic_offset != 0U) {
            available -= magic_offset;
            memmove(parser->frame_bytes, parser->frame_bytes + magic_offset, available);
        }

        fib_parser_reset(parser);
        parser->used = available;
        parser->expected = FIB_HEADER_SIZE;
        parser->reading_frame = true;

        if(available < FIB_HEADER_SIZE) return;
        if(!fib_parser_validate_header(parser)) {
            search_start = 1U;
            continue;
        }
        if(available < parser->expected) return;

        const size_t candidate_size = parser->expected;
        if(!fib_parser_validate_frame(parser)) {
            search_start = 1U;
            continue;
        }
        if(parser->frame_callback) {
            parser->frame_callback(&parser->frame, parser->callback_context);
        }

        const size_t trailing = available - candidate_size;
        if(trailing == 0U) {
            fib_parser_reset(parser);
            return;
        }
        memmove(parser->frame_bytes, parser->frame_bytes + candidate_size, trailing);
        available = trailing;
        search_start = 0U;
    }

    fib_parser_reset(parser);
}

void fib_parser_feed(FibParser* parser, const uint8_t* data, size_t length) {
    if(!parser || (!data && length != 0U)) return;

    for(size_t index = 0; index < length; ++index) {
        const uint8_t byte = data[index];
        if(!parser->reading_frame) {
            fib_parser_seek_byte(parser, byte);
            continue;
        }

        if(parser->used >= sizeof(parser->frame_bytes)) {
            fib_parser_report(parser, FibParseErrorInvalidHeader);
            fib_parser_reset(parser);
            fib_parser_seek_byte(parser, byte);
            continue;
        }

        parser->frame_bytes[parser->used++] = byte;
        if(parser->used == FIB_HEADER_SIZE && parser->expected == FIB_HEADER_SIZE) {
            if(!fib_parser_validate_header(parser)) {
                fib_parser_recover_buffered(parser, parser->used, 1U);
                continue;
            }
        }

        if(parser->used == parser->expected) {
            const bool valid = fib_parser_validate_frame(parser);
            if(valid && parser->frame_callback) {
                parser->frame_callback(&parser->frame, parser->callback_context);
            }
            if(valid) {
                fib_parser_reset(parser);
            } else {
                fib_parser_recover_buffered(parser, parser->used, 1U);
            }
        }
    }
}

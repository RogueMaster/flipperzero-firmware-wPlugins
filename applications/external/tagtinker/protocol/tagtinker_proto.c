/*
 * ESL protocol helpers.
 *
 * This file covers three jobs:
 * 1. Decode a barcode into the tag address and known display profile.
 * 2. Pack pixels into the tag's raw or RLE bitmap format.
 * 3. Wrap those bytes into the IR frames that the tag understands.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "tagtinker_proto.h"
#include "../tagtinker_app.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    uint16_t type_code;
    uint16_t width;
    uint16_t height;
    TagTinkerTagKind kind;
    TagTinkerTagColor color;
} TagTinkerProfileEntry;

/* Known type codes seen in ESL barcodes. */
static const TagTinkerProfileEntry profile_table[] = {
    {1206, 0,   0,   TagTinkerTagKindSegment,   TagTinkerTagColorMono},
    {1207, 0,   0,   TagTinkerTagKindSegment,   TagTinkerTagColorMono},
    {1217, 0,   0,   TagTinkerTagKindSegment,   TagTinkerTagColorMono},
    {1219, 0,   0,   TagTinkerTagKindSegment,   TagTinkerTagColorMono},
    {1240, 0,   0,   TagTinkerTagKindSegment,   TagTinkerTagColorMono},
    {1241, 0,   0,   TagTinkerTagKindSegment,   TagTinkerTagColorMono},
    {1242, 0,   0,   TagTinkerTagKindSegment,   TagTinkerTagColorMono},
    {1243, 0,   0,   TagTinkerTagKindSegment,   TagTinkerTagColorMono},
    {1265, 0,   0,   TagTinkerTagKindSegment,   TagTinkerTagColorMono},
    {1275, 320, 192, TagTinkerTagKindDotMatrix, TagTinkerTagColorMono},
    {1276, 320, 140, TagTinkerTagKindDotMatrix, TagTinkerTagColorMono},
    {1291, 0,   0,   TagTinkerTagKindSegment,   TagTinkerTagColorMono},
    {1300, 172, 72,  TagTinkerTagKindDotMatrix, TagTinkerTagColorMono},
    {1314, 400, 300, TagTinkerTagKindDotMatrix, TagTinkerTagColorMono},
    {1315, 296, 128, TagTinkerTagKindDotMatrix, TagTinkerTagColorMono},
    {1317, 152, 152, TagTinkerTagKindDotMatrix, TagTinkerTagColorMono},
    {1318, 208, 112, TagTinkerTagKindDotMatrix, TagTinkerTagColorMono},
    {1319, 800, 480, TagTinkerTagKindDotMatrix, TagTinkerTagColorMono},
    {1322, 152, 152, TagTinkerTagKindDotMatrix, TagTinkerTagColorMono},
    {1324, 208, 112, TagTinkerTagKindDotMatrix, TagTinkerTagColorMono},
    {1327, 208, 112, TagTinkerTagKindDotMatrix, TagTinkerTagColorRed},
    {1328, 296, 128, TagTinkerTagKindDotMatrix, TagTinkerTagColorRed},
    {1336, 400, 300, TagTinkerTagKindDotMatrix, TagTinkerTagColorRed},
    {1339, 152, 152, TagTinkerTagKindDotMatrix, TagTinkerTagColorRed},
    {1340, 800, 480, TagTinkerTagKindDotMatrix, TagTinkerTagColorRed},
    {1344, 296, 128, TagTinkerTagKindDotMatrix, TagTinkerTagColorYellow},
    {1346, 800, 480, TagTinkerTagKindDotMatrix, TagTinkerTagColorYellow},
    {1348, 264, 176, TagTinkerTagKindDotMatrix, TagTinkerTagColorRed},
    {1349, 264, 176, TagTinkerTagKindDotMatrix, TagTinkerTagColorYellow},
    {1351, 648, 480, TagTinkerTagKindDotMatrix, TagTinkerTagColorMono},
    {1353, 648, 480, TagTinkerTagKindDotMatrix, TagTinkerTagColorRed},
    {1354, 648, 480, TagTinkerTagKindDotMatrix, TagTinkerTagColorRed},
    {1370, 296, 128, TagTinkerTagKindDotMatrix, TagTinkerTagColorRed},
    {1371, 648, 480, TagTinkerTagKindDotMatrix, TagTinkerTagColorRed},
    {1510, 0,   0,   TagTinkerTagKindSegment,   TagTinkerTagColorMono},
    {1627, 296, 128, TagTinkerTagKindDotMatrix, TagTinkerTagColorRed},
    {1628, 296, 128, TagTinkerTagKindDotMatrix, TagTinkerTagColorRed},
    {1639, 152, 152, TagTinkerTagKindDotMatrix, TagTinkerTagColorRed},
};

static const TagTinkerProfileEntry* find_profile_entry(uint16_t type_code) {
    for(size_t i = 0; i < COUNT_OF(profile_table); i++) {
        if(profile_table[i].type_code == type_code) {
            return &profile_table[i];
        }
    }

    return NULL;
}

static void append_word(uint8_t* buf, size_t* pos, uint16_t value) {
    buf[(*pos)++] = (value >> 8) & 0xFF;
    buf[(*pos)++] = value & 0xFF;
}

static size_t terminate(uint8_t* buf, size_t len) {
    uint16_t crc = tagtinker_crc16(buf, len);
    buf[len]     = crc & 0xFF;
    buf[len + 1] = (crc >> 8) & 0xFF;
    return len + 2;
}

static size_t raw_frame(uint8_t* buf, uint8_t proto,
                        const uint8_t plid[4], uint8_t cmd) {
    /* Every addressed frame starts with protocol byte, PLID, then command. */
    buf[0] = proto;
    buf[1] = plid[3]; buf[2] = plid[2];
    buf[3] = plid[1]; buf[4] = plid[0];
    buf[5] = cmd;
    return 6;
}

static size_t mcu_frame(uint8_t* buf, const uint8_t plid[4], uint8_t cmd) {
    /* Image upload is tunneled through command 0x34 with an inner MCU opcode. */
    size_t p = raw_frame(buf, TAGTINKER_PROTO_DM, plid, 0x34);
    buf[p++] = 0x00;
    buf[p++] = 0x00;
    buf[p++] = 0x00;
    buf[p++] = cmd;
    return p;
}

uint16_t tagtinker_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0x8408;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(int b = 0; b < 8; b++)
            crc = (crc & 1) ? (crc >> 1) ^ 0x8408 : crc >> 1;
    }
    return crc;
}

bool tagtinker_barcode_to_plid(const char* barcode, uint8_t plid[4]) {
    if(!barcode || strlen(barcode) != 17) return false;
    for(int i = 2; i < 12; i++)
        if(barcode[i] < '0' || barcode[i] > '9') return false;

    uint32_t lo = 0, hi = 0;
    for(int i = 2; i < 7; i++)  lo = lo * 10 + (barcode[i] - '0');
    for(int i = 7; i < 12; i++) hi = hi * 10 + (barcode[i] - '0');

    uint32_t id = lo + (hi << 16);
    plid[0] = (id >> 8)  & 0xFF;
    plid[1] = id & 0xFF;
    plid[2] = (id >> 24) & 0xFF;
    plid[3] = (id >> 16) & 0xFF;
    return true;
}

bool tagtinker_barcode_to_type(const char* barcode, uint16_t* type_code) {
    if(!barcode || strlen(barcode) != 17 || !type_code) return false;

    uint16_t type = 0;
    for(int i = 12; i < 16; i++) {
        if(barcode[i] < '0' || barcode[i] > '9') return false;
        type = (uint16_t)(type * 10 + (barcode[i] - '0'));
    }

    *type_code = type;
    return true;
}

bool tagtinker_barcode_to_profile(const char* barcode, TagTinkerTagProfile* profile) {
    if(!profile) return false;

    memset(profile, 0, sizeof(*profile));

    uint16_t type_code = 0;
    if(!tagtinker_barcode_to_type(barcode, &type_code)) return false;

    profile->type_code = type_code;
    const TagTinkerProfileEntry* entry = find_profile_entry(type_code);
    if(!entry) return true;

    profile->width = entry->width;
    profile->height = entry->height;
    profile->kind = entry->kind;
    profile->color = entry->color;
    profile->known = true;
    return true;
}

size_t tagtinker_build_broadcast_page_frame(
    uint8_t* buf, uint8_t page, bool forever, uint16_t duration) {

    const uint8_t plid[4] = {0};
    size_t p = raw_frame(buf, TAGTINKER_PROTO_DM, plid, 0x06);
    buf[p++] = ((page & 7) << 3) | 0x01 | (forever ? 0x80 : 0x00);
    buf[p++] = 0x00;
    buf[p++] = 0x00;
    buf[p++] = forever ? 0x00 : ((duration >> 8) & 0xFF);
    buf[p++] = forever ? 0x00 : (duration & 0xFF);
    return terminate(buf, p);
}

size_t tagtinker_build_broadcast_debug_frame(uint8_t* buf) {
    const uint8_t plid[4] = {0};
    size_t p = raw_frame(buf, TAGTINKER_PROTO_DM, plid, 0x06);
    buf[p++] = 0xF1;
    buf[p++] = 0x00;
    buf[p++] = 0x00;
    buf[p++] = 0x00;
    buf[p++] = 0x0A;
    return terminate(buf, p);
}

size_t tagtinker_make_addressed_frame(
    uint8_t* buf, const uint8_t plid[4],
    const uint8_t* payload, size_t payload_len) {

    size_t p = raw_frame(buf, TAGTINKER_PROTO_DM, plid, payload[0]);
    memcpy(&buf[p], payload + 1, payload_len - 1);
    p += payload_len - 1;
    return terminate(buf, p);
}

size_t tagtinker_make_ping_frame(uint8_t* buf, const uint8_t plid[4]) {
    size_t p = raw_frame(buf, TAGTINKER_PROTO_DM, plid, 0x17);
    buf[p++] = 0x01;
    buf[p++] = 0x00;
    buf[p++] = 0x00;
    buf[p++] = 0x00;
    for(int i = 0; i < 22; i++) buf[p++] = 0x01;
    return terminate(buf, p);
}

size_t tagtinker_make_refresh_frame(uint8_t* buf, const uint8_t plid[4]) {
    size_t p = mcu_frame(buf, plid, 0x01);
    for(int i = 0; i < 22; i++) buf[p++] = 0x00;
    return terminate(buf, p);
}

size_t tagtinker_make_mcu_frame(
    uint8_t* buf, const uint8_t plid[4], uint8_t cmd) {
    return mcu_frame(buf, plid, cmd);
}

static void record_run(uint8_t* out, size_t* pos, size_t cap, uint32_t run_count) {
    uint8_t bits[32];
    int n = 0;
    uint32_t v = run_count;
    while(v) { bits[n++] = v & 1; v >>= 1; }
    for(int i = 0; i < n / 2; i++) {
        uint8_t t = bits[i]; bits[i] = bits[n - 1 - i]; bits[n - 1 - i] = t;
    }
    /* Runs are unary-prefixed: zeros mark bit-length, then the count bits follow. */
    for(int i = 1; i < n; i++)
        if(*pos < cap) out[(*pos)++] = 0;
    for(int i = 0; i < n; i++)
        if(*pos < cap) out[(*pos)++] = bits[i];
}

size_t tagtinker_rle_compress(
    const uint8_t* pixels, size_t count,
    uint8_t* out, size_t out_cap, uint8_t* comp_type) {

    if(count == 0) { *comp_type = 0; return 0; }

    size_t pos = 0;
    if(pos < out_cap) out[pos++] = pixels[0];

    uint8_t run_pixel = pixels[0];
    uint32_t run_count = 1;

    for(size_t i = 1; i < count; i++) {
        if(pixels[i] == run_pixel) {
            run_count++;
        } else {
            record_run(out, &pos, out_cap, run_count);
            run_pixel = pixels[i];
            run_count = 1;
        }
    }
    if(run_count > 1) record_run(out, &pos, out_cap, run_count);

    if(pos < count) {
        *comp_type = 2;
        return pos;
    }

    memcpy(out, pixels, count < out_cap ? count : out_cap);
    *comp_type = 0;
    return count < out_cap ? count : out_cap;
}

static inline uint8_t plane_pixel_at(
    const uint8_t* primary_pixels,
    const uint8_t* secondary_pixels,
    size_t pixel_count,
    size_t index) {
    if(index < pixel_count) return primary_pixels[index];
    return secondary_pixels[index - pixel_count];
}

typedef struct {
    uint8_t* data;
    size_t bit_pos;
} TagTinkerBitWriter;

static inline void bit_writer_append(TagTinkerBitWriter* writer, uint8_t bit) {
    size_t byte_idx = writer->bit_pos / 8U;
    size_t bit_idx = 7U - (writer->bit_pos % 8U);
    if(bit) writer->data[byte_idx] |= (uint8_t)(1U << bit_idx);
    writer->bit_pos++;
}

static size_t record_run_bit_length(uint32_t run_count) {
    size_t bit_count = 0;
    do {
        bit_count++;
        run_count >>= 1;
    } while(run_count);

    return (bit_count * 2U) - 1U;
}

static void bit_writer_append_run(TagTinkerBitWriter* writer, uint32_t run_count) {
    uint8_t bits[32];
    int n = 0;
    uint32_t v = run_count;
    while(v) {
        bits[n++] = v & 1U;
        v >>= 1;
    }

    for(int i = 0; i < n / 2; i++) {
        uint8_t t = bits[i];
        bits[i] = bits[n - 1 - i];
        bits[n - 1 - i] = t;
    }

    for(int i = 1; i < n; i++) bit_writer_append(writer, 0U);
    for(int i = 0; i < n; i++) bit_writer_append(writer, bits[i]);
}

static size_t tagtinker_rle_planes_bit_length(
    const uint8_t* primary_pixels,
    const uint8_t* secondary_pixels,
    size_t pixel_count) {
    if(!primary_pixels) return 0;

    size_t total_count = secondary_pixels ? (pixel_count * 2U) : pixel_count;
    if(total_count == 0) return 0;

    size_t bit_len = 1U;
    uint8_t run_pixel = plane_pixel_at(primary_pixels, secondary_pixels, pixel_count, 0);
    uint32_t run_count = 1;

    for(size_t i = 1; i < total_count; i++) {
        uint8_t pixel = plane_pixel_at(primary_pixels, secondary_pixels, pixel_count, i);
        if(pixel == run_pixel) {
            run_count++;
        } else {
            bit_len += record_run_bit_length(run_count);
            run_pixel = pixel;
            run_count = 1;
        }
    }

    if(run_count > 1U) bit_len += record_run_bit_length(run_count);
    return bit_len;
}

static void tagtinker_pack_planes_raw(
    const uint8_t* primary_pixels,
    const uint8_t* secondary_pixels,
    size_t pixel_count,
    uint8_t* out) {
    size_t total_count = secondary_pixels ? (pixel_count * 2U) : pixel_count;
    TagTinkerBitWriter writer = {.data = out, .bit_pos = 0};

    for(size_t i = 0; i < total_count; i++) {
        bit_writer_append(
            &writer, plane_pixel_at(primary_pixels, secondary_pixels, pixel_count, i));
    }
}

static void tagtinker_pack_planes_rle(
    const uint8_t* primary_pixels,
    const uint8_t* secondary_pixels,
    size_t pixel_count,
    uint8_t* out) {
    size_t total_count = secondary_pixels ? (pixel_count * 2U) : pixel_count;
    if(total_count == 0) return;

    TagTinkerBitWriter writer = {.data = out, .bit_pos = 0};
    uint8_t run_pixel = plane_pixel_at(primary_pixels, secondary_pixels, pixel_count, 0);
    uint32_t run_count = 1;

    bit_writer_append(&writer, run_pixel);

    for(size_t i = 1; i < total_count; i++) {
        uint8_t pixel = plane_pixel_at(primary_pixels, secondary_pixels, pixel_count, i);
        if(pixel == run_pixel) {
            run_count++;
        } else {
            bit_writer_append_run(&writer, run_count);
            run_pixel = pixel;
            run_count = 1;
        }
    }

    if(run_count > 1U) bit_writer_append_run(&writer, run_count);
}

#define DATA_BYTES_PER_FRAME 20
#define DATA_BITS_PER_FRAME  (DATA_BYTES_PER_FRAME * 8)

bool tagtinker_encode_planes_payload(
    const uint8_t* primary_pixels,
    const uint8_t* secondary_pixels,
    size_t pixel_count,
    TagTinkerCompressionMode mode,
    TagTinkerImagePayload* payload) {
    if(!primary_pixels || !payload) return false;

    memset(payload, 0, sizeof(*payload));

    size_t total_pixels = secondary_pixels ? (pixel_count * 2U) : pixel_count;
    size_t comp_len = tagtinker_rle_planes_bit_length(primary_pixels, secondary_pixels, pixel_count);
    bool use_compressed = false;
    if(mode == TagTinkerCompressionRle) {
        use_compressed = true;
    } else if(mode == TagTinkerCompressionAuto) {
        /* Auto mode picks RLE only when it is smaller than the raw bitstream. */
        use_compressed = (comp_len > 0U) && (comp_len < total_pixels);
    }
    size_t src_len = use_compressed ? comp_len : total_pixels;

    size_t padding = (DATA_BITS_PER_FRAME - (src_len % DATA_BITS_PER_FRAME)) % DATA_BITS_PER_FRAME;
    size_t padded_bits = src_len + padding;
    size_t padded_bytes = padded_bits / 8U;

    uint8_t* data_bytes = calloc(padded_bytes, 1);
    if(!data_bytes) return false;

    if(use_compressed) {
        tagtinker_pack_planes_rle(primary_pixels, secondary_pixels, pixel_count, data_bytes);
    } else {
        tagtinker_pack_planes_raw(primary_pixels, secondary_pixels, pixel_count, data_bytes);
    }

    payload->data = data_bytes;
    payload->byte_count = padded_bytes;
    payload->comp_type = use_compressed ? 2U : 0U;
    return true;
}

bool tagtinker_encode_image_payload(
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    bool color_clear,
    TagTinkerCompressionMode mode,
    TagTinkerImagePayload* payload) {
    size_t pixel_count = (size_t)width * height;
    uint8_t* second_plane = NULL;

    if(color_clear) {
        second_plane = malloc(pixel_count);
        if(!second_plane) return false;
        memset(second_plane, 1, pixel_count);
    }

    bool ok = tagtinker_encode_planes_payload(pixels, second_plane, pixel_count, mode, payload);
    free(second_plane);
    return ok;
}

void tagtinker_free_image_payload(TagTinkerImagePayload* payload) {
    if(!payload) return;

    free(payload->data);
    payload->data = NULL;
    payload->byte_count = 0;
    payload->comp_type = 0;
}

size_t tagtinker_make_image_param_frame(
    uint8_t* buf,
    const uint8_t plid[4],
    uint16_t byte_count,
    uint8_t comp_type,
    uint8_t page,
    uint16_t width,
    uint16_t height,
    uint16_t pos_x,
    uint16_t pos_y) {
    /* Command 0x05 tells the tag how many bytes are coming and where to place them. */
    size_t p = mcu_frame(buf, plid, 0x05);
    append_word(buf, &p, byte_count);
    buf[p++] = 0x00;
    buf[p++] = comp_type;
    buf[p++] = page;
    append_word(buf, &p, width);
    append_word(buf, &p, height);
    append_word(buf, &p, pos_x);
    append_word(buf, &p, pos_y);
    append_word(buf, &p, 0x0000);
    buf[p++] = 0x88;
    append_word(buf, &p, 0x0000);
    for(int i = 0; i < 4; i++) buf[p++] = 0x00;
    return terminate(buf, p);
}

size_t tagtinker_make_image_data_frame(
    uint8_t* buf,
    const uint8_t plid[4],
    uint16_t frame_index,
    const uint8_t data_bytes[20]) {
    /* Command 0x20 carries one fixed 20-byte image block. */
    size_t p = mcu_frame(buf, plid, 0x20);
    append_word(buf, &p, frame_index);
    memcpy(&buf[p], data_bytes, DATA_BYTES_PER_FRAME);
    p += DATA_BYTES_PER_FRAME;
    return terminate(buf, p);
}

void tagtinker_build_image_sequence(
    TagTinkerApp* app,
    const uint8_t plid[4],
    const uint8_t* pixels,
    uint16_t width, uint16_t height,
    uint8_t page,
    uint16_t pos_x, uint16_t pos_y,
    uint16_t wake_repeats) {

    TagTinkerImagePayload payload;
    if(!tagtinker_encode_image_payload(
           pixels, width, height, app->color_clear, app->compression_mode, &payload))
        return;

    /* The tag expects 20 data bytes per frame, so pad the payload to that boundary. */
    size_t frame_count = payload.byte_count / DATA_BYTES_PER_FRAME;

    FURI_LOG_I("TagTinker", "IMG %ux%u pg=%u comp=%u %zu->%zu frames=%zu",
        width,
        height,
        page,
        payload.comp_type,
        (size_t)width * height * (app->color_clear ? 2U : 1U),
        payload.byte_count,
        frame_count);

    size_t total = 2 + frame_count + 1;

    app->frame_seq_count = total;
    app->frame_sequence = malloc(sizeof(uint8_t*) * total);
    app->frame_lengths  = malloc(sizeof(size_t) * total);
    app->frame_repeats  = malloc(sizeof(uint16_t) * total);

    if(!app->frame_sequence || !app->frame_lengths || !app->frame_repeats) {
        tagtinker_free_image_payload(&payload);
        app->frame_seq_count = 0;
        return;
    }

    size_t idx = 0;

    /* Wake the tag before sending the upload. */
    app->frame_sequence[idx] = malloc(TAGTINKER_MAX_FRAME_SIZE);
    app->frame_lengths[idx]  = tagtinker_make_ping_frame(app->frame_sequence[idx], plid);
    app->frame_repeats[idx]  = wake_repeats;
    idx++;

    /* The parameter frame describes size, page, compression mode, and placement. */
    app->frame_sequence[idx] = malloc(TAGTINKER_MAX_FRAME_SIZE);
    app->frame_lengths[idx] = tagtinker_make_image_param_frame(
        app->frame_sequence[idx],
        plid,
        (uint16_t)payload.byte_count,
        payload.comp_type,
        page,
        width,
        height,
        pos_x,
        pos_y);
    app->frame_repeats[idx] = 1;
    idx++;

    /* Data frames follow in order and carry the packed bitmap bytes. */
    for(size_t fi = 0; fi < frame_count; fi++) {
        app->frame_sequence[idx] = malloc(TAGTINKER_MAX_FRAME_SIZE);
        size_t start = fi * DATA_BYTES_PER_FRAME;
        app->frame_lengths[idx] = tagtinker_make_image_data_frame(
            app->frame_sequence[idx], plid, (uint16_t)fi, &payload.data[start]);
        app->frame_repeats[idx] = 3;
        idx++;
    }

    /* Refresh asks the tag to display the uploaded image. */
    app->frame_sequence[idx] = malloc(TAGTINKER_MAX_FRAME_SIZE);
    app->frame_lengths[idx]  = tagtinker_make_refresh_frame(app->frame_sequence[idx], plid);
    app->frame_repeats[idx]  = 1;

    if(app->frame_seq_count > 1) {
        memcpy(app->frame_buf, app->frame_sequence[1],
               app->frame_lengths[1] < TAGTINKER_MAX_FRAME_SIZE
                   ? app->frame_lengths[1] : TAGTINKER_MAX_FRAME_SIZE);
        app->frame_len = app->frame_lengths[1];
    }

    tagtinker_free_image_payload(&payload);
}

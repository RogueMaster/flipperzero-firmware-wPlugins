/*
 * ESL protocol helpers.
 *
 * This layer turns barcodes, pixels, and payload bytes into the frames
 * that the Flipper sends over IR to the tag.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define TAGTINKER_PROTO_DM  0x85
#define TAGTINKER_PROTO_SEG 0x84
#define TAGTINKER_MAX_FRAME_SIZE 96

typedef struct TagTinkerApp TagTinkerApp;

/* CRC used by the ESL wire format. */
uint16_t tagtinker_crc16(const uint8_t* data, size_t len);

typedef enum {
    TagTinkerTagKindUnknown = 0,
    TagTinkerTagKindDotMatrix,
    TagTinkerTagKindSegment,
} TagTinkerTagKind;

typedef enum {
    TagTinkerTagColorMono = 0,
    TagTinkerTagColorRed,
    TagTinkerTagColorYellow,
} TagTinkerTagColor;

typedef struct {
    uint16_t type_code;
    uint16_t width;
    uint16_t height;
    TagTinkerTagKind kind;
    TagTinkerTagColor color;
    bool known;
} TagTinkerTagProfile;

bool tagtinker_barcode_to_plid(const char* barcode, uint8_t plid[4]);
bool tagtinker_barcode_to_type(const char* barcode, uint16_t* type_code);
bool tagtinker_barcode_to_profile(const char* barcode, TagTinkerTagProfile* profile);

typedef struct {
    uint8_t* data;
    size_t byte_count;
    uint8_t comp_type;
} TagTinkerImagePayload;

typedef enum {
    TagTinkerCompressionAuto = 0,
    TagTinkerCompressionRaw,
    TagTinkerCompressionRle,
} TagTinkerCompressionMode;

bool tagtinker_encode_image_payload(
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    bool color_clear,
    TagTinkerCompressionMode mode,
    TagTinkerImagePayload* payload);
bool tagtinker_encode_planes_payload(
    const uint8_t* primary_pixels,
    const uint8_t* secondary_pixels,
    size_t pixel_count,
    TagTinkerCompressionMode mode,
    TagTinkerImagePayload* payload);
void tagtinker_free_image_payload(TagTinkerImagePayload* payload);
size_t tagtinker_make_image_param_frame(
    uint8_t* buf,
    const uint8_t plid[4],
    uint16_t byte_count,
    uint8_t comp_type,
    uint8_t page,
    uint16_t width,
    uint16_t height,
    uint16_t pos_x,
    uint16_t pos_y);
size_t tagtinker_make_image_data_frame(
    uint8_t* buf,
    const uint8_t plid[4],
    uint16_t frame_index,
    const uint8_t data_bytes[20]);

/* Broadcast frames address every listening tag. */
size_t tagtinker_build_broadcast_page_frame(
    uint8_t* buf, uint8_t page, bool forever, uint16_t duration);

size_t tagtinker_build_broadcast_debug_frame(uint8_t* buf);

/* Addressed frames use the PLID decoded from the barcode. */
size_t tagtinker_make_addressed_frame(
    uint8_t* buf, const uint8_t plid[4],
    const uint8_t* payload, size_t payload_len);

/* Tags need a wake ping before most addressed commands. */
size_t tagtinker_make_ping_frame(uint8_t* buf, const uint8_t plid[4]);

size_t tagtinker_make_refresh_frame(uint8_t* buf, const uint8_t plid[4]);

/* Image upload uses MCU frames: one parameter frame followed by data frames. */
size_t tagtinker_make_mcu_frame(
    uint8_t* buf, const uint8_t plid[4], uint8_t cmd);

/* RLE is the tag's compact bitmap format. Raw mode keeps one bit per pixel. */
size_t tagtinker_rle_compress(
    const uint8_t* pixels, size_t count,
    uint8_t* out, size_t out_cap, uint8_t* comp_type);

/* Builds the full IR sequence: wake, image params, data chunks, refresh. */
void tagtinker_build_image_sequence(
    TagTinkerApp* app,
    const uint8_t plid[4],
    const uint8_t* pixels,
    uint16_t width, uint16_t height,
    uint8_t page,
    uint16_t pos_x, uint16_t pos_y,
    uint16_t wake_repeats);

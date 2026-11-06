/* Decoding of the host → Flipper messages of protocol/PROTOCOL.md.
 *
 * proto_decode_line() takes one line (without its '\n'; surrounding JSON
 * whitespace and a trailing '\r' are fine) and fills a ProtoMessage:
 *
 *   ProtoDecodeOk         a valid hello, lookup or result
 *   ProtoDecodeIgnored    valid JSON whose `t` the Flipper does not consume
 *                         (unknown, scan, intent) or whose `v` is not
 *                         PROTO_VERSION
 *   ProtoDecodeOtherVersion  a `hello` whose `v` (1..99) is not PROTO_VERSION:
 *                         msg->type is ProtoMsgOtherVersion and
 *                         msg->other_version holds `v`; the host app is
 *                         there but speaks another protocol. Any other
 *                         message of another version is Ignored.
 *   ProtoDecodeMalformed  not JSON, or a known `t` with missing, ill-typed
 *                         or out-of-range fields (per the schema), too many
 *                         or too long `lines`, a `spool` / `created` /
 *                         `tag_info` the message's status does not allow,
 *                         or a `lookup.spool.id` other than `spool_id`
 *
 * Fields may come in any order; unknown fields are skipped, also inside
 * `spool` and `tag_info`. Strings are
 * ASCII as the Flipper renders them (see json_reader.h). Unless the status
 * is ProtoDecodeOk (or OtherVersion, see above) the message is left zeroed
 * with type ProtoMsgNone.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "proto_encode.h"

#define PROTO_LINES_MAX       4
#define PROTO_LINE_CHARS      40
#define PROTO_ERROR_MAX       200
#define PROTO_HELLO_FIELD_MAX 40
#define PROTO_NAME_MAX        40 /* material, subtype, colour name */
#define PROTO_CODE_MAX        16 /* tag_info.code */
#define PROTO_DIAMETER_MAX    5 /* tag_info.diameter, "99.99" */
#define PROTO_TEMP_MAX        999 /* tag_info.nozzle_min / nozzle_max */

typedef enum {
    ProtoDecodeOk,
    ProtoDecodeIgnored,
    ProtoDecodeMalformed,
    ProtoDecodeOtherVersion,
} ProtoDecodeStatus;

typedef enum {
    ProtoMsgNone,
    ProtoMsgHello,
    ProtoMsgLookup,
    ProtoMsgResult,
    ProtoMsgOtherVersion, /* only with ProtoDecodeOtherVersion */
} ProtoMsgType;

typedef struct {
    uint8_t count;
    char line[PROTO_LINES_MAX][PROTO_LINE_CHARS + 1];
} ProtoLines;

typedef struct {
    char app[PROTO_HELLO_FIELD_MAX + 1];
    char fw[PROTO_HELLO_FIELD_MAX + 1]; /* "" unless sent by a Flipper */
    char os[PROTO_HELLO_FIELD_MAX + 1]; /* "" unless sent by a host */
} ProtoHello;

typedef enum {
    ProtoLookupFound,
    ProtoLookupUnknown,
    ProtoLookupError,
} ProtoLookupStatus;

typedef struct {
    bool has_gross;
    bool has_label;
    bool has_core;
    uint16_t gross;
    uint16_t label;
    uint16_t core;
} ProtoDefaults;

/* A Bambuddy spool (`lookup.spool`, `result.spool`). */
typedef struct {
    int32_t id; /* >= 1 */
    char material[PROTO_NAME_MAX + 1];
    char subtype[PROTO_NAME_MAX + 1]; /* "" when absent */
    char color_name[PROTO_NAME_MAX + 1]; /* "" when absent */
    bool has_rgba;
    uint32_t rgba; /* 0xRRGGBBAA */
    uint16_t remaining;
    uint16_t label;
    uint16_t core;
    bool archived;
} ProtoSpool;

/* What the host parsed from an unknown tag (`lookup.tag_info`). */
typedef struct {
    char material[PROTO_NAME_MAX + 1]; /* "" when absent */
    char subtype[PROTO_NAME_MAX + 1]; /* "" when absent */
    bool has_rgba;
    uint32_t rgba; /* 0xRRGGBBAA */
    bool has_label;
    uint16_t label;
    char color_name[PROTO_NAME_MAX + 1]; /* "" when absent */
    char code[PROTO_CODE_MAX + 1]; /* Bambu's colour code, "11803"; "" when absent */
    char filament_id[PROTO_CODE_MAX + 1]; /* "GFA01", "" when absent */
    char diameter[PROTO_DIAMETER_MAX + 1]; /* "1.75", "" when absent */
    bool has_nozzle_min;
    uint16_t nozzle_min; /* °C */
    bool has_nozzle_max;
    uint16_t nozzle_max;
} ProtoTagInfo;

typedef struct {
    char scan_id[PROTO_ID_MAX + 1];
    ProtoLookupStatus status;
    int32_t spool_id; /* >= 1 when found, 0 otherwise */
    bool prefill;
    bool has_spool; /* found only */
    ProtoSpool spool;
    bool created; /* found only: created on scan (autosave) */
    bool has_tag_info; /* unknown only */
    ProtoTagInfo tag_info;
    ProtoLines lines;
    ProtoDefaults defaults;
    char error[PROTO_ERROR_MAX + 1]; /* "" when absent */
} ProtoLookup;

typedef struct {
    char id[PROTO_ID_MAX + 1];
    bool ok;
    bool has_spool; /* ok only */
    ProtoSpool spool;
    ProtoLines lines;
    char error[PROTO_ERROR_MAX + 1]; /* "" when absent */
} ProtoResult;

typedef struct {
    ProtoMsgType type;
    uint8_t other_version; /* ProtoMsgOtherVersion: the `v` of the host's hello */
    union {
        ProtoHello hello;
        ProtoLookup lookup;
        ProtoResult result;
    };
} ProtoMessage;

ProtoDecodeStatus proto_decode_line(const char* line, ProtoMessage* msg);

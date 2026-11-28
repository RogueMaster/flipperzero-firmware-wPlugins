#pragma once

#include <furi.h>
#include <notification/notification_messages.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "proto6_raw_samples.h"

#define TAG "TPMSProto6"
#define BITMAP_SEEK_NOT_FOUND UINT32_MAX
#define DEBUG_MSG 0

typedef struct ProtoViewApp ProtoViewApp;
typedef struct ProtoViewMsgInfo ProtoViewMsgInfo;
typedef struct ProtoViewFieldSet ProtoViewFieldSet;
typedef struct ProtoViewDecoder ProtoViewDecoder;

typedef enum {
    FieldTypeStr,
    FieldTypeSignedInt,
    FieldTypeUnsignedInt,
    FieldTypeBinary,
    FieldTypeHex,
    FieldTypeBytes,
    FieldTypeFloat,
} ProtoViewFieldType;

typedef struct {
    ProtoViewFieldType type;
    uint32_t len;
    char* name;
    union {
        char* str;
        int64_t value;
        uint64_t uvalue;
        uint8_t* bytes;
        float fvalue;
    };
} ProtoViewField;

struct ProtoViewFieldSet {
    ProtoViewField** fields;
    uint32_t numfields;
};

typedef enum {
    ProtoTpmsModulationFSK = 4,
    ProtoTpmsModulationOOK = 5,
} ProtoTpmsModulation;

struct ProtoViewDecoder {
    const char* name;
    bool (*decode)(uint8_t* bits, uint32_t numbytes, uint32_t numbits, ProtoViewMsgInfo* info);
    void (*get_fields)(ProtoViewFieldSet* fields);
    void (*build_message)(RawSamplesBuffer* samples, ProtoViewFieldSet* fields);
};

struct ProtoViewMsgInfo {
    ProtoViewDecoder* decoder;
    ProtoViewFieldSet* fieldset;
    uint32_t start_off;
    uint32_t pulses_count;
    uint32_t short_pulse_dur;
    uint8_t modulation;
    uint8_t* bits;
    uint32_t bits_bytes;
};

struct ProtoViewApp {
    NotificationApp* notification;
    uint32_t signal_bestlen;
    uint32_t signal_last_scan_idx;
    bool signal_decoded;
    ProtoViewMsgInfo* msg_info;
    uint32_t signal_offset;
    uint32_t us_scale;
    uint8_t modulation;
};

extern RawSamplesBuffer* RawSamples;
extern RawSamplesBuffer* DetectedSamples;
extern ProtoViewDecoder* Decoders[];

uint8_t crc8(const uint8_t* data, size_t len, uint8_t init, uint8_t poly);
uint8_t sum_bytes(const uint8_t* data, size_t len, uint8_t init);
uint8_t xor_bytes(const uint8_t* data, size_t len, uint8_t init);

uint32_t duration_delta(uint32_t a, uint32_t b);
void reset_current_signal(ProtoViewApp* app);
void scan_for_signal(ProtoViewApp* app, RawSamplesBuffer* source, uint32_t min_duration);
bool decode_signal(RawSamplesBuffer* s, uint64_t len, ProtoViewMsgInfo* info);
void init_msg_info(ProtoViewMsgInfo* i, ProtoViewApp* app);
void free_msg_info(ProtoViewMsgInfo* i);

bool bitmap_get(uint8_t* b, uint32_t blen, uint32_t bitpos);
void bitmap_set(uint8_t* b, uint32_t blen, uint32_t bitpos, bool val);
void bitmap_copy(
    uint8_t* d,
    uint32_t dlen,
    uint32_t doff,
    uint8_t* s,
    uint32_t slen,
    uint32_t soff,
    uint32_t count);
void bitmap_set_pattern(uint8_t* b, uint32_t blen, uint32_t off, const char* pat);
void bitmap_reverse_bytes_bits(uint8_t* p, uint32_t len);
bool bitmap_match_bits(uint8_t* b, uint32_t blen, uint32_t bitpos, const char* bits);
uint32_t bitmap_seek_bits(
    uint8_t* b,
    uint32_t blen,
    uint32_t startpos,
    uint32_t maxbits,
    const char* bits);
bool bitmap_match_bitmap(
    uint8_t* b1,
    uint32_t b1len,
    uint32_t b1off,
    uint8_t* b2,
    uint32_t b2len,
    uint32_t b2off,
    uint32_t cmplen);
void bitmap_to_string(
    char* dst,
    uint8_t* b,
    uint32_t blen,
    uint32_t off,
    uint32_t len);
uint32_t convert_signal_to_bits(
    uint8_t* b,
    uint32_t blen,
    RawSamplesBuffer* s,
    uint32_t idx,
    uint32_t count,
    uint32_t rate);
uint32_t convert_from_line_code(
    uint8_t* buf,
    uint64_t buflen,
    uint8_t* bits,
    uint32_t len,
    uint32_t off,
    const char* zero_pattern,
    const char* one_pattern);
uint32_t convert_from_diff_manchester(
    uint8_t* buf,
    uint64_t buflen,
    uint8_t* bits,
    uint32_t len,
    uint32_t off,
    bool previous);

ProtoViewFieldSet* fieldset_new(void);
void fieldset_free(ProtoViewFieldSet* fs);
void fieldset_add_int(ProtoViewFieldSet* fs, const char* name, int64_t val, uint8_t bits);
void fieldset_add_uint(ProtoViewFieldSet* fs, const char* name, uint64_t uval, uint8_t bits);
void fieldset_add_hex(ProtoViewFieldSet* fs, const char* name, uint64_t uval, uint8_t bits);
void fieldset_add_bin(ProtoViewFieldSet* fs, const char* name, uint64_t uval, uint8_t bits);
void fieldset_add_str(ProtoViewFieldSet* fs, const char* name, const char* s, size_t len);
void fieldset_add_bytes(
    ProtoViewFieldSet* fs,
    const char* name,
    const uint8_t* bytes,
    uint32_t count_nibbles);
void fieldset_add_float(
    ProtoViewFieldSet* fs,
    const char* name,
    float val,
    uint32_t digits_after_dot);

uint8_t tpms_decoder_modulation(const ProtoViewDecoder* decoder);
bool tpms_decoder_matches_modulation(const ProtoViewDecoder* decoder, uint8_t modulation);
ProtoViewField* proto6_field_find(ProtoViewFieldSet* fs, const char* name);

/* Classic UI does not render ProtoView's raw waveform. */
void adjust_raw_view_scale(ProtoViewApp* app, uint32_t short_pulse_dur);

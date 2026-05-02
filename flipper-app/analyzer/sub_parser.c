#include "sub_parser.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>

/* Largest line we will buffer. 16KB bits worst case = ~6KB hex chars + spaces.
 * Round up to 8KB; longer lines are truncated with a flag. */
#define LINE_BUF_SZ 8192

void sub_file_init(SubFile* sub) {
    memset(sub, 0, sizeof(*sub));
}

void sub_file_reset(SubFile* sub) {
    for(uint16_t i = 0; i < sub->segment_count; i++) {
        if(sub->segment_bits[i]) {
            free(sub->segment_bits[i]);
            sub->segment_bits[i] = NULL;
        }
    }
    memset(sub, 0, sizeof(*sub));
}

static int hex_nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return 10 + c - 'a';
    if(c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

static char* skip_ws(char* s) {
    while(*s == ' ' || *s == '\t') s++;
    return s;
}

static char* trim_inplace(char* s) {
    while(*s == ' ' || *s == '\t') s++;
    char* end = s + strlen(s);
    while(end > s && (end[-1] == ' ' || end[-1] == '\t')) end--;
    *end = 0;
    return s;
}

static bool starts_with(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool decode_data_raw(
    const char* hex,
    uint32_t bit_raw_count,
    SubFile* sub,
    bool* truncated_out) {
    if(sub->segment_count >= BITRAW_MAX_SEGMENTS) {
        *truncated_out = true;
        return true;
    }

    uint32_t already = 0;
    for(uint16_t i = 0; i < sub->segment_count; i++) {
        already += sub->segment_bit_lens[i];
    }
    if(already >= BITRAW_MAX_TOTAL_BITS) {
        *truncated_out = true;
        return true;
    }

    uint32_t declared = bit_raw_count;
    if(declared == 0) {
        uint32_t nibbles = 0;
        for(const char* p = hex; *p; p++) {
            if(hex_nibble(*p) >= 0) nibbles++;
        }
        declared = (nibbles / 2u) * 8u;
    }

    if(declared > BITRAW_MAX_BITS_PER_SEG) {
        declared = BITRAW_MAX_BITS_PER_SEG;
        *truncated_out = true;
    }
    if(already + declared > BITRAW_MAX_TOTAL_BITS) {
        declared = BITRAW_MAX_TOTAL_BITS - already;
        *truncated_out = true;
    }
    if(declared == 0) return true;

    uint8_t* buf = malloc(declared);
    if(!buf) return false;

    uint32_t emitted = 0;
    int high = -1;
    for(const char* p = hex; *p && emitted < declared; p++) {
        int n = hex_nibble(*p);
        if(n < 0) continue;
        if(high < 0) {
            high = n;
            continue;
        }
        uint8_t byte = (uint8_t)((high << 4) | n);
        high = -1;
        for(int b = 7; b >= 0 && emitted < declared; b--) {
            buf[emitted++] = (uint8_t)((byte >> b) & 1u);
        }
    }
    if(emitted < declared) {
        memset(buf + emitted, 0, declared - emitted);
        *truncated_out = true;
    }

    sub->segment_bits[sub->segment_count] = buf;
    sub->segment_bit_lens[sub->segment_count] = (uint16_t)declared;
    sub->segment_count++;
    return true;
}

static void process_line(
    char* trimmed,
    SubFile* sub,
    uint32_t* pending_bit_raw,
    bool* pending_set,
    bool* have_freq,
    bool* have_te,
    bool* truncated) {
    if(*trimmed == 0) return;

    if(starts_with(trimmed, "Frequency:")) {
        sub->frequency_hz = (uint32_t)strtoul(skip_ws(trimmed + 10), NULL, 10);
        *have_freq = true;
    } else if(starts_with(trimmed, "TE:")) {
        sub->te_us = (uint32_t)strtoul(skip_ws(trimmed + 3), NULL, 10);
        *have_te = true;
    } else if(starts_with(trimmed, "Preset:")) {
        char* v = skip_ws(trimmed + 7);
        strncpy(sub->preset, v, BITRAW_MAX_PRESET_LEN - 1);
        sub->preset[BITRAW_MAX_PRESET_LEN - 1] = 0;
    } else if(starts_with(trimmed, "Lat:")) {
        sub->lat = strtof(skip_ws(trimmed + 4), NULL);
        if(sub->lat != 0.0f) sub->has_gps = true;
    } else if(starts_with(trimmed, "Lon:")) {
        sub->lon = strtof(skip_ws(trimmed + 4), NULL);
        if(sub->lon != 0.0f) sub->has_gps = true;
    } else if(starts_with(trimmed, "Bit_RAW:")) {
        *pending_bit_raw = (uint32_t)strtoul(skip_ws(trimmed + 8), NULL, 10);
        *pending_set = true;
    } else if(starts_with(trimmed, "Bit:") && !starts_with(trimmed, "Bit_RAW:")) {
        sub->total_bit_header = (uint32_t)strtoul(skip_ws(trimmed + 4), NULL, 10);
    } else if(starts_with(trimmed, "Data_RAW:")) {
        char* hex = skip_ws(trimmed + 9);
        decode_data_raw(hex, *pending_set ? *pending_bit_raw : 0u, sub, truncated);
        *pending_set = false;
        *pending_bit_raw = 0;
    }
}

SubParseStatus sub_parser_parse(
    Storage* storage,
    const char* path,
    SubFile* sub,
    FuriString* err_out) {
    sub_file_reset(sub);

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(err_out) furi_string_cat_str(err_out, "Cannot open file");
        storage_file_free(file);
        return SubParseErrorOpen;
    }

    char* line = malloc(LINE_BUF_SZ);
    if(!line) {
        storage_file_close(file);
        storage_file_free(file);
        if(err_out) furi_string_cat_str(err_out, "Out of memory");
        return SubParseErrorOpen;
    }

    bool have_freq = false;
    bool have_te = false;
    uint32_t pending_bit_raw = 0;
    bool pending_set = false;
    bool truncated = false;

    char chunk[256];
    size_t line_len = 0;
    bool line_overflowed = false;

    while(true) {
        uint16_t got = storage_file_read(file, chunk, sizeof(chunk));
        if(got == 0) break;
        for(uint16_t ci = 0; ci < got; ci++) {
            char c = chunk[ci];
            if(c == '\n' || c == '\r') {
                if(line_overflowed) truncated = true;
                line[line_len] = 0;
                char* trimmed = trim_inplace(line);
                process_line(
                    trimmed,
                    sub,
                    &pending_bit_raw,
                    &pending_set,
                    &have_freq,
                    &have_te,
                    &truncated);
                line_len = 0;
                line_overflowed = false;
            } else if(!line_overflowed && line_len + 1 < LINE_BUF_SZ) {
                line[line_len++] = c;
            } else {
                line_overflowed = true;
            }
        }
    }
    if(line_len > 0 || line_overflowed) {
        if(line_overflowed) truncated = true;
        line[line_len] = 0;
        char* trimmed = trim_inplace(line);
        process_line(
            trimmed,
            sub,
            &pending_bit_raw,
            &pending_set,
            &have_freq,
            &have_te,
            &truncated);
    }

    free(line);
    storage_file_close(file);
    storage_file_free(file);

    if(!have_freq) {
        if(err_out) furi_string_cat_str(err_out, "Missing Frequency");
        sub_file_reset(sub);
        return SubParseErrorMissingField;
    }
    if(!have_te) {
        if(err_out) furi_string_cat_str(err_out, "Missing TE");
        sub_file_reset(sub);
        return SubParseErrorMissingField;
    }
    if(sub->segment_count == 0) {
        if(err_out) furi_string_cat_str(err_out, "No Data_RAW");
        sub_file_reset(sub);
        return SubParseErrorNoData;
    }

    sub->truncated = truncated;
    return truncated ? SubParseTruncated : SubParseOk;
}

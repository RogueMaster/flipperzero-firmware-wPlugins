// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file sig_db.c
 * SD-card loader for the optional, updatable Flock/ALPR signature database.
 *
 * Reads apps_data/flipdeflock/signatures.json, parses it with the vendored
 * jsmn tokenizer, copies the extras into owned arrays, and registers them with
 * flock_db via flock_db_set_extra_*. See sig_db.h for the full posture
 * (load-only, fail-safe, unverified, bounded RAM). Every error path here frees
 * whatever it allocated and returns NULL so the built-ins stay intact -- a
 * malformed user file must never corrupt detection.
 */
#include "sig_db.h"
#include "flock_db.h"
#include "../recon_app_i.h" // RECON_APP_FOLDER

#include <stdlib.h>
#include <string.h>

// Header-only jsmn (MIT), static so it pulls into this TU only. JSMN_HEADER is
// intentionally NOT defined, so the implementation is emitted here.
#define JSMN_STATIC
#include "../lib/jsmn/jsmn.h"

#define SIG_DB_PATH RECON_APP_FOLDER "/signatures.json"

// Hard bounds. A FAP loads fully into 256 KB RAM, so keep everything tiny.
#define SIG_MAX_OUIS     64u /**< owned OUI table cap (overflow silently ignored) */
#define SIG_MAX_PATTERNS 32u /**< per-list SSID-pattern cap (overflow ignored) */
#define SIG_MAX_IE_FPS   32u /**< IE-fingerprint table cap (overflow ignored) */
#define SIG_MAX_FILE     8192u /**< largest signatures.json we will read */
#define SIG_MAX_TOKENS   512u /**< jsmn token budget (bounds parse RAM) */
#define SIG_MAX_NEEDLE   48u /**< longest SSID substring we keep (lowercased) */
#define SIG_MAX_DEPTH    16 /**< max JSON nesting we recurse (schema is flat) -> bounds stack */

struct SigDb {
    uint8_t (*ouis)[3]; /**< owned OUI table, NULL if none */
    size_t oui_count;
    char** confirmed; /**< owned lowercased needle array, NULL if none */
    size_t confirmed_count;
    char** likely;
    size_t likely_count;
    uint32_t* ie_fps; /**< owned IE-fingerprint table, NULL if none */
    size_t ie_fp_count;
    FlockDbExtras extras; /**< caller-owned view registered with flock_db (points at the above) */
};

/** ASCII lower-case (no locale), matching flock_db.c's matcher contract. */
static char sig_ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/** Hex nibble value, or -1 if `c` is not a hex digit. */
static int sig_hex_nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * Strict "aa:bb:cc" -> 3 bytes. Rejects anything that is not exactly three
 * colon-separated hex byte pairs (length 8, colons at [2] and [5]). Returns
 * true and fills out[3] on success; leaves out untouched on failure so a bad
 * entry is simply skipped.
 */
static bool sig_parse_oui(const char* s, size_t len, uint8_t out[3]) {
    if(len != 8) return false;
    if(s[2] != ':' || s[5] != ':') return false;
    const int pos[3] = {0, 3, 6};
    for(int b = 0; b < 3; b++) {
        int hi = sig_hex_nibble(s[pos[b]]);
        int lo = sig_hex_nibble(s[pos[b] + 1]);
        if(hi < 0 || lo < 0) return false;
        out[b] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

/**
 * Return the total number of tokens that the value at tokens[i] spans
 * (recursively, including nested objects/arrays). Used to skip over the value
 * of an unrecognised top-level key without mis-parsing it.
 */
static int sig_token_span_d(const jsmntok_t* tokens, int count, int i, int depth) {
    if(i >= count) return 0;
    // Depth cap: jsmn bounds the token COUNT but not nesting depth, so a crafted
    // file like {"x":[[[[...]]]]} recurses once per level and overflows the 4 KB
    // app stack (~100 levels) while staying within the token budget. Bail by
    // claiming the rest of the tokens -- the caller then stops parsing safely.
    if(depth > SIG_MAX_DEPTH) return count - i;
    const jsmntok_t* t = &tokens[i];
    if(t->type == JSMN_PRIMITIVE || t->type == JSMN_STRING) {
        return 1;
    }
    if(t->type == JSMN_OBJECT) {
        int span = 1;
        for(int k = 0; k < t->size; k++) {
            span += sig_token_span_d(tokens, count, i + span, depth + 1); // key
            span += sig_token_span_d(tokens, count, i + span, depth + 1); // value
        }
        return span;
    }
    if(t->type == JSMN_ARRAY) {
        int span = 1;
        for(int k = 0; k < t->size; k++) {
            span += sig_token_span_d(tokens, count, i + span, depth + 1);
        }
        return span;
    }
    return 1;
}

/** Token span of the value at tokens[i] (depth-capped; see sig_token_span_d). */
static int sig_token_span(const jsmntok_t* tokens, int count, int i) {
    return sig_token_span_d(tokens, count, i, 0);
}

/** True if string token `t` equals the (NUL-terminated) literal `key`. */
static bool sig_tok_eq(const char* js, const jsmntok_t* t, const char* key) {
    if(t->type != JSMN_STRING) return false;
    size_t tlen = (size_t)(t->end - t->start);
    return tlen == strlen(key) && strncmp(js + t->start, key, tlen) == 0;
}

/**
 * Parse the OUI array whose token is tokens[arr_idx] into a freshly malloc'd
 * uint8_t[][3], capped at SIG_MAX_OUIS. Malformed/oversize-skipped entries are
 * dropped. Returns the table (caller frees) and writes the kept count to
 * *out_count, or NULL on alloc failure or when nothing valid was kept.
 */
static uint8_t (*sig_parse_ouis(
    const char* js,
    const jsmntok_t* tokens,
    int count,
    int arr_idx,
    size_t* out_count))[3] {
    const jsmntok_t* arr = &tokens[arr_idx];
    if(arr->type != JSMN_ARRAY || arr->size <= 0) return NULL;

    uint8_t(*table)[3] = malloc(sizeof(uint8_t[3]) * SIG_MAX_OUIS);
    if(!table) return NULL;

    size_t kept = 0;
    int idx = arr_idx + 1; // first element token
    for(int e = 0; e < arr->size && idx < count; e++) {
        const jsmntok_t* el = &tokens[idx];
        if(el->type == JSMN_STRING && kept < SIG_MAX_OUIS) {
            uint8_t oui[3];
            if(sig_parse_oui(js + el->start, (size_t)(el->end - el->start), oui)) {
                table[kept][0] = oui[0];
                table[kept][1] = oui[1];
                table[kept][2] = oui[2];
                kept++;
            }
        }
        idx += sig_token_span(tokens, count, idx);
    }

    if(kept == 0) {
        free(table);
        return NULL;
    }
    *out_count = kept;
    return table;
}

/**
 * Parse a string array into a freshly malloc'd char* array of LOWERCASED,
 * NUL-terminated needles (capped at SIG_MAX_PATTERNS, each <= SIG_MAX_NEEDLE).
 * Empty strings are skipped. Returns the array (caller frees array + each
 * string) and writes the kept count, or NULL on alloc failure / nothing kept.
 */
static char** sig_parse_patterns(
    const char* js,
    const jsmntok_t* tokens,
    int count,
    int arr_idx,
    size_t* out_count) {
    const jsmntok_t* arr = &tokens[arr_idx];
    if(arr->type != JSMN_ARRAY || arr->size <= 0) return NULL;

    char** list = malloc(sizeof(char*) * SIG_MAX_PATTERNS);
    if(!list) return NULL;

    size_t kept = 0;
    int idx = arr_idx + 1;
    for(int e = 0; e < arr->size && idx < count; e++) {
        const jsmntok_t* el = &tokens[idx];
        if(el->type == JSMN_STRING && kept < SIG_MAX_PATTERNS) {
            size_t slen = (size_t)(el->end - el->start);
            if(slen > 0 && slen <= SIG_MAX_NEEDLE) {
                char* s = malloc(slen + 1);
                if(!s) {
                    // Alloc failure mid-list: roll back everything kept so far
                    // and fail safe (caller registers nothing).
                    for(size_t k = 0; k < kept; k++)
                        free(list[k]);
                    free(list);
                    return NULL;
                }
                for(size_t c = 0; c < slen; c++)
                    s[c] = sig_ascii_lower(js[el->start + c]);
                s[slen] = '\0';
                list[kept++] = s;
            }
        }
        idx += sig_token_span(tokens, count, idx);
    }

    if(kept == 0) {
        free(list);
        return NULL;
    }
    *out_count = kept;
    return list;
}

/**
 * Parse an IE-fingerprint array (each element an exactly-8-hex-char string = a
 * FNV-1a uint32, matching the companion's `,fp=` field) into a freshly malloc'd
 * uint32_t table, capped at SIG_MAX_IE_FPS. Non-8-hex or zero entries are
 * skipped. Returns the table (caller frees) + kept count, or NULL on alloc
 * failure / nothing kept.
 */
static uint32_t* sig_parse_ie_fps(
    const char* js,
    const jsmntok_t* tokens,
    int count,
    int arr_idx,
    size_t* out_count) {
    const jsmntok_t* arr = &tokens[arr_idx];
    if(arr->type != JSMN_ARRAY || arr->size <= 0) return NULL;

    uint32_t* table = malloc(sizeof(uint32_t) * SIG_MAX_IE_FPS);
    if(!table) return NULL;

    size_t kept = 0;
    int idx = arr_idx + 1;
    for(int e = 0; e < arr->size && idx < count; e++) {
        const jsmntok_t* el = &tokens[idx];
        if(el->type == JSMN_STRING && kept < SIG_MAX_IE_FPS) {
            size_t slen = (size_t)(el->end - el->start);
            if(slen == 8) { // exactly one uint32 worth of hex
                uint32_t v = 0;
                bool ok = true;
                for(size_t c = 0; c < 8; c++) {
                    int nib = sig_hex_nibble(js[el->start + c]);
                    if(nib < 0) {
                        ok = false;
                        break;
                    }
                    v = (v << 4) | (uint32_t)nib;
                }
                if(ok && v != 0) table[kept++] = v; // 0 = "no fingerprint", skip
            }
        }
        idx += sig_token_span(tokens, count, idx);
    }

    if(kept == 0) {
        free(table);
        return NULL;
    }
    *out_count = kept;
    return table;
}

/** Free a SigDb and everything it owns (after deregistering). NULL-safe. */
static void sig_db_destroy(SigDb* db) {
    if(!db) return;
    free(db->ouis);
    free(db->ie_fps);
    if(db->confirmed) {
        for(size_t i = 0; i < db->confirmed_count; i++)
            free(db->confirmed[i]);
        free(db->confirmed);
    }
    if(db->likely) {
        for(size_t i = 0; i < db->likely_count; i++)
            free(db->likely[i]);
        free(db->likely);
    }
    free(db);
}

/** Read the whole signatures file into a fresh, NUL-terminated buffer. */
static char* sig_read_file(Storage* storage, size_t* out_len) {
    File* file = storage_file_alloc(storage);
    char* buf = NULL;
    if(storage_file_open(file, SIG_DB_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        if(size > 0 && size <= SIG_MAX_FILE) {
            buf = malloc((size_t)size + 1);
            if(buf) {
                size_t n = storage_file_read(file, buf, (uint16_t)size);
                if(n == (size_t)size) {
                    buf[n] = '\0';
                    *out_len = n;
                } else {
                    // short read -> fail safe
                    free(buf);
                    buf = NULL;
                }
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return buf;
}

SigDb* sig_db_load(Storage* storage) {
    if(!storage) return NULL;

    size_t len = 0;
    char* js = sig_read_file(storage, &len);
    if(!js) return NULL; // absent / empty / oversized / read error -> built-ins only

    jsmntok_t* tokens = malloc(sizeof(jsmntok_t) * SIG_MAX_TOKENS);
    if(!tokens) {
        free(js);
        return NULL;
    }

    jsmn_parser parser;
    jsmn_init(&parser);
    int count = jsmn_parse(&parser, js, len, tokens, SIG_MAX_TOKENS);

    // Need a well-formed object root. Any parse error (incl. NOMEM from a file
    // with too many tokens) fails safe.
    if(count < 1 || tokens[0].type != JSMN_OBJECT) {
        free(tokens);
        free(js);
        return NULL;
    }

    SigDb* db = malloc(sizeof(SigDb));
    if(!db) {
        free(tokens);
        free(js);
        return NULL;
    }
    memset(db, 0, sizeof(SigDb));

    // Walk the root object's key/value pairs. Keys are the odd children; for
    // each recognised key parse the following array, else skip its value span.
    int i = 1; // first key token
    for(int k = 0; k < tokens[0].size && i < count; k++) {
        const jsmntok_t* key = &tokens[i];
        int val_idx = i + 1;
        if(val_idx >= count) break;

        // Guard each assignment so a duplicate top-level key can't overwrite (and
        // leak) the array we already parsed -- keep the first, ignore the rest.
        if(sig_tok_eq(js, key, "ouis")) {
            if(!db->ouis) db->ouis = sig_parse_ouis(js, tokens, count, val_idx, &db->oui_count);
        } else if(sig_tok_eq(js, key, "ssid_confirmed")) {
            if(!db->confirmed)
                db->confirmed =
                    sig_parse_patterns(js, tokens, count, val_idx, &db->confirmed_count);
        } else if(sig_tok_eq(js, key, "ssid_likely")) {
            if(!db->likely)
                db->likely = sig_parse_patterns(js, tokens, count, val_idx, &db->likely_count);
        } else if(sig_tok_eq(js, key, "ie_fps")) {
            if(!db->ie_fps)
                db->ie_fps = sig_parse_ie_fps(js, tokens, count, val_idx, &db->ie_fp_count);
        }

        // Advance past key + its (possibly nested/unknown) value.
        i = val_idx + sig_token_span(tokens, count, val_idx);
    }

    free(tokens);
    free(js);

    // Nothing usable parsed -> behave exactly like an absent file.
    if(!db->ouis && !db->confirmed && !db->likely && !db->ie_fps) {
        sig_db_destroy(db);
        return NULL;
    }

    // Register the owned arrays with flock_db as one caller-owned context (lives
    // in db, so it outlives the registration; cleared before free in sig_db_free).
    db->extras = (FlockDbExtras){
        .ouis = (const uint8_t(*)[3])db->ouis,
        .oui_count = db->ouis ? db->oui_count : 0,
        .ssid_confirmed = (const char* const*)db->confirmed,
        .ssid_confirmed_count = db->confirmed ? db->confirmed_count : 0,
        .ssid_likely = (const char* const*)db->likely,
        .ssid_likely_count = db->likely ? db->likely_count : 0,
        .ie_fps = db->ie_fps,
        .ie_fp_count = db->ie_fps ? db->ie_fp_count : 0,
    };
    flock_db_set_extras(&db->extras);

    return db;
}

void sig_db_free(SigDb* db) {
    if(!db) return;
    // Deregister FIRST so the matchers stop reading our arrays before we free.
    flock_db_set_extras(NULL);
    sig_db_destroy(db);
}

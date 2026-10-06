// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
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
#define SIG_MAX_MACS     32u /**< pinned whole-address cap (overflow ignored) */
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
    uint8_t (*macs)[6]; /**< owned pinned-address table, NULL if none */
    size_t mac_count;
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

/** "aa:bb:cc:dd:ee:ff" -> 6 bytes. Colons required, exact length, no shorthand. */
static bool sig_parse_mac(const char* s, size_t len, uint8_t out[6]) {
    if(len != 17) return false;
    for(int b = 0; b < 6; b++) {
        int p = b * 3;
        if(b < 5 && s[p + 2] != ':') return false;
        int hi = sig_hex_nibble(s[p]);
        int lo = sig_hex_nibble(s[p + 1]);
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
 * Parse a string array of whole MAC addresses into a freshly malloc'd
 * uint8_t[][6], capped at SIG_MAX_MACS. Same fail-open posture as the OUI
 * parser: malformed entries are dropped, not fatal.
 */
static uint8_t (*sig_parse_macs(
    const char* js,
    const jsmntok_t* tokens,
    int count,
    int arr_idx,
    size_t* out_count))[6] {
    const jsmntok_t* arr = &tokens[arr_idx];
    if(arr->type != JSMN_ARRAY || arr->size <= 0) return NULL;

    uint8_t(*table)[6] = malloc(sizeof(uint8_t[6]) * SIG_MAX_MACS);
    if(!table) return NULL;

    size_t kept = 0;
    int idx = arr_idx + 1;
    for(int e = 0; e < arr->size && idx < count; e++) {
        const jsmntok_t* el = &tokens[idx];
        if(el->type == JSMN_STRING && kept < SIG_MAX_MACS) {
            uint8_t mac[6];
            if(sig_parse_mac(js + el->start, (size_t)(el->end - el->start), mac)) {
                memcpy(table[kept], mac, 6);
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
    free(db->macs);
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

/* ---- learned fingerprints (learned.txt) --------------------------------- */

/**
 * Plain text, one 8-hex fingerprint per line, '#' comments ignored.
 *
 * NOT JSON, deliberately. Teaching the app one fingerprint has to be an APPEND:
 * a JSON array would mean read, parse, re-serialise and rewrite the whole file
 * on every confirmation, which turns a one-line write into a path that can lose
 * the existing contents if it is interrupted. A line-oriented file appends in
 * one write, and a corrupt or half-written line is skipped by the reader
 * instead of poisoning the parse.
 */
#define SIG_LEARNED_PATH RECON_APP_FOLDER "/learned.txt"
#define SIG_LEARNED_HEADER                                                         \
    "# FlipDeFlock learned signatures v2\n"                                        \
    "# One value per line, written when you confirm a device you looked at with\n" \
    "# your own eyes:\n"                                                           \
    "#   8 hex digits  = probe IE fingerprint (survives the MAC changing)\n"       \
    "#   12 hex digits = a whole MAC address you pinned\n"                         \
    "# Both score \"Class?\" only -- never Confirmed. Delete this file to\n"       \
    "# forget them all.\n"

/**
 * Parse one line as 8 hex digits (a fingerprint) or 12 (a whole MAC).
 *
 * Returns the digit count so the caller can tell which it got, or 0 for a blank,
 * a comment, or anything malformed. ONE FILE AND ONE PARSER for both, rather
 * than a second learned_macs.txt: the width is unambiguous, older 8-digit files
 * keep working untouched, and a duplicated reader is how two formats drift.
 */
static int sig_learned_parse_line(const char* line, size_t len, uint64_t* out) {
    size_t i = 0;
    while(i < len && (line[i] == ' ' || line[i] == '\t'))
        i++;
    if(i >= len || line[i] == '#') return 0;
    uint64_t v = 0;
    int digits = 0;
    for(; i < len; i++) {
        char c = line[i];
        if(c == '\r' || c == '\n') break;
        int d = sig_hex_nibble(c);
        if(d < 0) return 0; // any junk -> skip the whole line rather than guess
        if(++digits > 12) return 0;
        v = (v << 4) | (uint64_t)d;
    }
    // 0 is the "no fingerprint" sentinel everywhere else in this codebase, so it
    // can never be a learned value, and an all-zero MAC is not a real address.
    if(v == 0) return 0;
    if(out) *out = v;
    return (digits == 8 || digits == 12) ? digits : 0;
}

/**
 * Read learned.txt into caller-supplied arrays, either of which may be NULL.
 * Kept counts go to the out params. Absent file, unreadable file and garbage
 * lines all yield 0 -- the same fail-safe posture as the JSON path.
 */
static void sig_lines_read(
    Storage* storage,
    const char* path,
    uint32_t* fps,
    size_t fp_max,
    uint8_t (*macs)[6],
    size_t mac_max,
    size_t* out_fps,
    size_t* out_macs) {
    size_t nf = 0, nm = 0;
    if(storage) {
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            uint64_t size = storage_file_size(file);
            if(size > 0 && size <= SIG_MAX_FILE) {
                char* buf = malloc((size_t)size + 1);
                if(buf) {
                    size_t got = storage_file_read(file, buf, (uint16_t)size);
                    if(got == (size_t)size) {
                        buf[got] = '\0';
                        size_t start = 0;
                        for(size_t i = 0; i <= got; i++) {
                            if(i != got && buf[i] != '\n') continue;
                            uint64_t v = 0;
                            int digits = sig_learned_parse_line(buf + start, i - start, &v);
                            start = i + 1;
                            if(digits == 8 && fps && nf < fp_max) {
                                uint32_t fp = (uint32_t)v;
                                bool dup = false;
                                for(size_t k = 0; k < nf; k++) {
                                    if(fps[k] == fp) {
                                        dup = true;
                                        break;
                                    }
                                }
                                if(!dup) fps[nf++] = fp;
                            } else if(digits == 12 && macs && nm < mac_max) {
                                uint8_t m[6];
                                for(int b = 0; b < 6; b++) {
                                    m[b] = (uint8_t)((v >> ((5 - b) * 8)) & 0xFF);
                                }
                                bool dup = false;
                                for(size_t k = 0; k < nm; k++) {
                                    if(memcmp(macs[k], m, 6) == 0) {
                                        dup = true;
                                        break;
                                    }
                                }
                                if(!dup) memcpy(macs[nm++], m, 6);
                            }
                        }
                    }
                    free(buf);
                }
            }
        }
        storage_file_close(file);
        storage_file_free(file);
    }
    if(out_fps) *out_fps = nf;
    if(out_macs) *out_macs = nm;
}

/** Thin wrapper so the learned callers below read unchanged. */
static void sig_learned_read(
    Storage* storage,
    uint32_t* fps,
    size_t fp_max,
    uint8_t (*macs)[6],
    size_t mac_max,
    size_t* out_fps,
    size_t* out_macs) {
    sig_lines_read(storage, SIG_LEARNED_PATH, fps, fp_max, macs, mac_max, out_fps, out_macs);
}

size_t sig_db_learned_count(Storage* storage) {
    uint32_t fps[SIG_MAX_IE_FPS];
    uint8_t macs[SIG_MAX_MACS][6];
    size_t nf = 0, nm = 0;
    sig_learned_read(storage, fps, SIG_MAX_IE_FPS, macs, SIG_MAX_MACS, &nf, &nm);
    // Counted together: Reports offers one "forget" action over one file, so a
    // split count would describe something the operator cannot act on separately.
    return nf + nm;
}

/** Append one already-validated line, writing `header` if the file is new. */
static bool
    sig_lines_append(Storage* storage, const char* path, const char* header, const char* line) {
    storage_common_mkdir(storage, RECON_APP_FOLDER);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        if(storage_file_size(file) == 0) {
            storage_file_write(file, header, strlen(header));
        }
        uint16_t n = (uint16_t)strlen(line);
        ok = storage_file_write(file, line, n) == n;
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static bool sig_learned_append(Storage* storage, const char* line) {
    return sig_lines_append(storage, SIG_LEARNED_PATH, SIG_LEARNED_HEADER, line);
}

bool sig_db_learn_fp(Storage* storage, uint32_t fp) {
    // 0 is "no fingerprint captured" everywhere in this codebase. A BLE-only or
    // beacon-only detection has none, and confirming one of those must not write
    // a wildcard entry that then matches every device with no fingerprint.
    if(!storage || fp == 0) return false;
    // A commodity scan skeleton is not a signature. The operator saw a camera --
    // that part is true -- but the row they had selected was a phone, and this
    // hash would go on to flag phones everywhere. Refuse the write rather than
    // store something the matcher is only going to ignore.
    if(flock_ie_fp_is_generic(fp)) return false;

    uint32_t have[SIG_MAX_IE_FPS];
    size_t n = 0;
    sig_learned_read(storage, have, SIG_MAX_IE_FPS, NULL, 0, &n, NULL);
    for(size_t i = 0; i < n; i++) {
        if(have[i] == fp) return false; // already known
    }
    if(n >= SIG_MAX_IE_FPS) return false; // bounded, same cap as the JSON path

    char line[16];
    if(snprintf(line, sizeof(line), "%08lx\n", (unsigned long)fp) <= 0) return false;
    return sig_learned_append(storage, line);
}

bool sig_db_learn_mac(Storage* storage, const uint8_t* mac) {
    if(!storage || !mac) return false;
    // All-zero and broadcast are not devices worth pinning, and both turn up in
    // malformed frames.
    bool all_zero = true, all_ff = true;
    for(int i = 0; i < 6; i++) {
        if(mac[i] != 0x00) all_zero = false;
        if(mac[i] != 0xFF) all_ff = false;
    }
    if(all_zero || all_ff) return false;

    uint8_t have[SIG_MAX_MACS][6];
    size_t n = 0;
    sig_learned_read(storage, NULL, 0, have, SIG_MAX_MACS, NULL, &n);
    for(size_t i = 0; i < n; i++) {
        if(memcmp(have[i], mac, 6) == 0) return false; // already pinned
    }
    if(n >= SIG_MAX_MACS) return false;

    char line[16];
    if(snprintf(
           line,
           sizeof(line),
           "%02x%02x%02x%02x%02x%02x\n",
           mac[0],
           mac[1],
           mac[2],
           mac[3],
           mac[4],
           mac[5]) <= 0)
        return false;
    return sig_learned_append(storage, line);
}
/* ---- operator exclusions (ignored.txt) ---------------------------------- */

/**
 * The same line format and the same parser as learned.txt, one tier below it in
 * intent: learned.txt says "this IS a camera", ignored.txt says "this is MINE".
 *
 * A SEPARATE FILE rather than a flag inside learned.txt, because the two are
 * forgotten for different reasons and on different schedules. An operator who
 * decides their learned signatures have gone bad wants them all gone; the
 * exclusion for the router in their own kitchen should survive that, and vice
 * versa. One file with a polarity column would make "Forget Learned" either
 * destroy exclusions or silently keep some lines, and both of those are
 * surprises in a file the operator cannot see from the device.
 */
#define SIG_IGNORED_PATH RECON_APP_FOLDER "/ignored.txt"
#define SIG_IGNORED_HEADER                                                       \
    "# FlipDeFlock exclusions v1\n"                                              \
    "# Devices you told the app are YOURS. Nothing listed here is reported as\n" \
    "# a detection, whatever it scores:\n"                                       \
    "#   12 hex digits = a whole MAC address\n"                                  \
    "#   8 hex digits  = probe IE fingerprint (survives the MAC changing)\n"     \
    "# Delete this file, or use Reports > Forget Ignored, to hear them again.\n"

void sig_db_ignored_read(
    Storage* storage,
    uint32_t* fps,
    size_t fp_max,
    uint8_t (*macs)[6],
    size_t mac_max,
    size_t* out_fps,
    size_t* out_macs) {
    sig_lines_read(storage, SIG_IGNORED_PATH, fps, fp_max, macs, mac_max, out_fps, out_macs);
}

size_t sig_db_ignored_count(Storage* storage) {
    uint32_t fps[SIG_IGNORED_MAX_FPS];
    uint8_t macs[SIG_IGNORED_MAX_MACS][6];
    size_t nf = 0, nm = 0;
    sig_db_ignored_read(storage, fps, SIG_IGNORED_MAX_FPS, macs, SIG_IGNORED_MAX_MACS, &nf, &nm);
    return nf + nm;
}

bool sig_db_forget_ignored(Storage* storage) {
    if(!storage) return false;
    return storage_simply_remove(storage, SIG_IGNORED_PATH);
}

bool sig_db_ignore_add(Storage* storage, const uint8_t* mac, uint32_t fp, bool* out_fp_written) {
    if(out_fp_written) *out_fp_written = false;
    if(!storage || !mac) return false;

    bool all_zero = true, all_ff = true;
    for(int i = 0; i < 6; i++) {
        if(mac[i] != 0x00) all_zero = false;
        if(mac[i] != 0xFF) all_ff = false;
    }
    if(all_zero || all_ff) return false;

    uint32_t have_fps[SIG_IGNORED_MAX_FPS];
    uint8_t have_macs[SIG_IGNORED_MAX_MACS][6];
    size_t nf = 0, nm = 0;
    sig_db_ignored_read(
        storage, have_fps, SIG_IGNORED_MAX_FPS, have_macs, SIG_IGNORED_MAX_MACS, &nf, &nm);

    bool wrote = false;
    bool dup_mac = false;
    for(size_t i = 0; i < nm; i++) {
        if(memcmp(have_macs[i], mac, 6) == 0) {
            dup_mac = true;
            break;
        }
    }
    // NO ROOM FOR THE ADDRESS MEANS NO EXCLUSION AT ALL. The contract is that
    // the address is always recorded; writing only the fingerprint and calling
    // that success left the device excluded on the sightings that carried the
    // fingerprint and alerting again on the ones that did not.
    if(!dup_mac && nm >= SIG_IGNORED_MAX_MACS) return false;
    if(!dup_mac) {
        char line[16];
        if(snprintf(
               line,
               sizeof(line),
               "%02x%02x%02x%02x%02x%02x\n",
               mac[0],
               mac[1],
               mac[2],
               mac[3],
               mac[4],
               mac[5]) > 0) {
            wrote = sig_lines_append(storage, SIG_IGNORED_PATH, SIG_IGNORED_HEADER, line);
        }
    }

    // THE FINGERPRINT IS THE HALF THAT CATCHES A ROTATING ADDRESS, and also the
    // half that can blind the operator, so it is written only when the hash
    // identifies a device family rather than a commodity scan pattern.
    //
    // A generic skeleton is shared with phones and consumer gear at large --
    // that is what flock_ie_fp_is_generic() means -- so excluding one would
    // suppress every device carrying it, a camera included. Refusing it costs
    // the operator nothing they had: the MAC above still silences the device in
    // front of them, and a commodity hash was never going to pick their phone
    // out of the street anyway. sig_db_learn_fp() refuses the same hashes for
    // the mirror-image reason.
    if(fp != 0 && !flock_ie_fp_is_generic(fp)) {
        bool dup_fp = false;
        for(size_t i = 0; i < nf; i++) {
            if(have_fps[i] == fp) {
                dup_fp = true;
                break;
            }
        }
        if(!dup_fp && nf < SIG_IGNORED_MAX_FPS) {
            char line[16];
            if(snprintf(line, sizeof(line), "%08lx\n", (unsigned long)fp) > 0) {
                if(sig_lines_append(storage, SIG_IGNORED_PATH, SIG_IGNORED_HEADER, line)) {
                    wrote = true;
                    if(out_fp_written) *out_fp_written = true;
                }
            }
        }
    }

    // Already excluded counts as success: the operator asked for this device to
    // go quiet and it is quiet. Reporting failure would send them looking for a
    // problem that does not exist.
    return wrote || dup_mac;
}

bool sig_db_forget_learned(Storage* storage) {
    if(!storage) return false;
    return storage_simply_remove(storage, SIG_LEARNED_PATH);
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

/**
 * Fold learned.txt into db->ie_fps, growing or allocating the array as needed.
 *
 * Merged into the SAME user tier as signatures.json rather than a tier of its
 * own, because they carry the same weight: both are unverified, both cap at
 * "Class?", and giving learned entries their own rung would be inventing a
 * confidence level nobody has justified.
 */
static void sig_merge_learned(SigDb* db, Storage* storage) {
    uint32_t learned[SIG_MAX_IE_FPS];
    uint8_t learned_macs[SIG_MAX_MACS][6];
    size_t ln = 0, lm = 0;
    sig_learned_read(storage, learned, SIG_MAX_IE_FPS, learned_macs, SIG_MAX_MACS, &ln, &lm);

    if(ln) {
        size_t have = db->ie_fps ? db->ie_fp_count : 0;
        if(have < SIG_MAX_IE_FPS) {
            uint32_t* merged = malloc(sizeof(uint32_t) * SIG_MAX_IE_FPS);
            if(merged) { // fail-safe: on alloc failure keep whatever the JSON gave us
                size_t n = 0;
                for(size_t i = 0; i < have && n < SIG_MAX_IE_FPS; i++)
                    merged[n++] = db->ie_fps[i];
                for(size_t i = 0; i < ln && n < SIG_MAX_IE_FPS; i++) {
                    bool dup = false;
                    for(size_t k = 0; k < n; k++) {
                        if(merged[k] == learned[i]) {
                            dup = true;
                            break;
                        }
                    }
                    if(!dup) merged[n++] = learned[i];
                }
                free(db->ie_fps);
                db->ie_fps = merged;
                db->ie_fp_count = n;
            }
        }
    }

    if(lm) {
        size_t have = db->macs ? db->mac_count : 0;
        if(have < SIG_MAX_MACS) {
            uint8_t(*merged)[6] = malloc(sizeof(uint8_t[6]) * SIG_MAX_MACS);
            if(merged) {
                size_t n = 0;
                for(size_t i = 0; i < have && n < SIG_MAX_MACS; i++) {
                    memcpy(merged[n++], db->macs[i], 6);
                }
                for(size_t i = 0; i < lm && n < SIG_MAX_MACS; i++) {
                    bool dup = false;
                    for(size_t k = 0; k < n; k++) {
                        if(memcmp(merged[k], learned_macs[i], 6) == 0) {
                            dup = true;
                            break;
                        }
                    }
                    if(!dup) memcpy(merged[n++], learned_macs[i], 6);
                }
                free(db->macs);
                db->macs = merged;
                db->mac_count = n;
            }
        }
    }
}

SigDb* sig_db_load(Storage* storage) {
    if(!storage) return NULL;

    size_t len = 0;
    char* js = sig_read_file(storage, &len);
    if(!js) {
        // NO signatures.json, but there may still be a learned.txt -- and for
        // most operators there will be, because learning is a menu action while
        // hand-writing JSON is not. Returning early here meant every fingerprint
        // taught to the app was silently ignored unless the user ALSO happened to
        // keep a signatures file.
        SigDb* only_learned = calloc(1, sizeof(SigDb));
        if(!only_learned) return NULL;
        sig_merge_learned(only_learned, storage);
        if(!only_learned->ie_fps && !only_learned->macs) {
            free(only_learned);
            return NULL; // nothing at all -> built-ins only, exactly as before
        }
        only_learned->extras = (FlockDbExtras){
            .ie_fps = only_learned->ie_fps,
            .ie_fp_count = only_learned->ie_fps ? only_learned->ie_fp_count : 0,
            .macs = (const uint8_t(*)[6])only_learned->macs,
            .mac_count = only_learned->macs ? only_learned->mac_count : 0,
        };
        flock_db_set_extras(&only_learned->extras);
        return only_learned;
    }

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
        } else if(sig_tok_eq(js, key, "macs")) {
            if(!db->macs) db->macs = sig_parse_macs(js, tokens, count, val_idx, &db->mac_count);
        }

        // Advance past key + its (possibly nested/unknown) value.
        i = val_idx + sig_token_span(tokens, count, val_idx);
    }

    free(tokens);
    free(js);

    sig_merge_learned(db, storage);

    // Nothing usable parsed -> behave exactly like an absent file.
    if(!db->ouis && !db->confirmed && !db->likely && !db->ie_fps && !db->macs) {
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
        .macs = (const uint8_t(*)[6])db->macs,
        .mac_count = db->macs ? db->mac_count : 0,
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

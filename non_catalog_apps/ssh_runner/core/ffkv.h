/*
 * Minimal FlipperFormat-compatible key/value text (pure C, no Furi).
 *
 * Files look like stock FlipperFormat files:
 *     Filetype: SSH Runner Host
 *     Version: 1
 *     # comment
 *     Alias: nas
 *     Cmd: uptime
 *     Cmd: df -h
 * Keys may repeat; order is kept. Values run to end of line (leading spaces trimmed,
 * trailing CR/LF/spaces trimmed). Parsing is done in place on a caller-owned buffer.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FFKV_MAX_PAIRS 48

typedef struct {
    const char* key;
    const char* value;
} FfkvPair;

typedef struct {
    FfkvPair pairs[FFKV_MAX_PAIRS];
    size_t count;
    bool overflow; /* more pairs than FFKV_MAX_PAIRS; extra lines ignored */
} Ffkv;

/* Parse `text` (NUL-terminated, modified in place). Lines without ':' are ignored. */
void ffkv_parse(Ffkv* kv, char* text);

/* First value for key, or NULL. */
const char* ffkv_get(const Ffkv* kv, const char* key);
/* n-th (0-based) value for a repeated key, or NULL. */
const char* ffkv_get_nth(const Ffkv* kv, const char* key, size_t n);
size_t ffkv_count(const Ffkv* kv, const char* key);
/* Check Filetype and that Version <= max_version. */
bool ffkv_check_header(const Ffkv* kv, const char* filetype, uint32_t max_version);

/* Bounded text writer for producing files. */
typedef struct {
    char* buf;
    size_t cap;
    size_t len;
    bool err;
} FfkvWriter;

void ffkv_writer_init(FfkvWriter* w, char* buf, size_t cap);
void ffkv_write_header(FfkvWriter* w, const char* filetype, uint32_t version);
/* Values containing CR/LF are rejected (w->err). */
void ffkv_write(FfkvWriter* w, const char* key, const char* value);
void ffkv_write_u32(FfkvWriter* w, const char* key, uint32_t value);
void ffkv_write_comment(FfkvWriter* w, const char* text);

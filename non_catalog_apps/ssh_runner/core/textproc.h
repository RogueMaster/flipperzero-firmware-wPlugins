/*
 * Output text processing: streaming ANSI/VT stripping and the bounded output
 * buffer behind the Output screen. Pure C, unit-tested on the host.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---- ANSI stripper ----
 * Removes CSI (ESC [ ... final), OSC (ESC ] ... BEL | ESC \), two-byte ESC
 * sequences and C0 controls; maps CR LF / lone CR to LF, TAB to two spaces, and any
 * non-ASCII UTF-8 sequence to '?'. State carries across calls, so sequences split
 * between chunks are handled. */
typedef struct {
    uint8_t state;
    uint8_t utf8_left;
    bool pending_cr;
} AnsiStripper;

void ansi_init(AnsiStripper* a);
/* `out` must hold at least 2 * len + 1 bytes. Returns bytes written (not NUL-terminated). */
size_t ansi_strip(AnsiStripper* a, const uint8_t* in, size_t len, char* out);

/* ---- Output buffer ----
 * Keeps the first OUTBUF_HEAD bytes and the last OUTBUF_TAIL bytes of a stream. When
 * more arrives, the middle is dropped and counted; the rendered text shows a marker. */
/* 1 + 3 KiB: a FAP's code shares the heap; 3 KiB of tail keeps the walks above 8 KB free
 * with the Protect secrets code loaded */
#define OUTBUF_HEAD 1024
#define OUTBUF_TAIL 3072

/* Head, then the tail (oldest byte first), in one array: until the middle is dropped, the
 * whole output is one run that the Output screen shows in place, with no copy. */
typedef struct {
    char buf[OUTBUF_HEAD + OUTBUF_TAIL];
    size_t head_len;
    size_t tail_len; /* at buf + OUTBUF_HEAD */
    size_t total; /* bytes appended since reset */
} OutBuf;

void outbuf_reset(OutBuf* o);
void outbuf_append(OutBuf* o, const char* data, size_t len);
size_t outbuf_dropped(const OutBuf* o);
bool outbuf_truncated(const OutBuf* o);
/* Upper bound for outbuf_render()'s output, including the NUL. */
size_t outbuf_render_size(const OutBuf* o);
/* The text as up to three runs: `head` (the whole output while nothing was dropped), then,
 * when truncated, "[... N bytes skipped ...]\n" (after a '\n' if the head ends mid-line) and
 * the tail from its first line boundary. head and tail point into the OutBuf: valid until
 * it next changes. */
typedef struct {
    const char* head;
    size_t head_len;
    char marker[48];
    size_t marker_len;
    const char* tail;
    size_t tail_len;
} OutbufParts;
void outbuf_parts(const OutBuf* o, OutbufParts* p);
/* The same text in one string. Returns length written (NUL-terminated). */
size_t outbuf_render(const OutBuf* o, char* dst, size_t cap);
/* The same text in pieces, with no buffer of its own (RAM on the Flipper). */
typedef void (*OutbufSink)(void* ctx, const char* data, size_t len);
void outbuf_emit(const OutBuf* o, OutbufSink sink, void* ctx);

/* Text for the Flipper's fonts, which hold printable ASCII only: every other character (a whole
 * UTF-8 sequence, e.g. a Chinese SSID) becomes one `missing`, so the length still reads right.
 * The app's own views draw TEXT_TOFU as a hollow box, the usual sign of a character a font
 * lacks (views/tofu.h); text for the stock modules uses '?'. The original bytes stay where they
 * are needed (joining a network). Returns dst. */
#define TEXT_TOFU '\x7f'
char* text_ascii(const char* src, char* dst, size_t size, char missing);

/* "exit 0 · 1.24 s" style status line; `signal` uses "signal N" instead. */
void format_exit_line(
    char* buf,
    size_t size,
    int32_t exit_code,
    uint8_t done_flags,
    uint32_t duration_ms);
/* "1.2 s", "850 ms", "2 m 05 s" */
void format_duration(char* buf, size_t size, uint32_t ms);
/* "1.2 KB" / "850 B" */
void format_size(char* buf, size_t size, uint32_t bytes);

/* ---- IPv4 helpers (u32 with first octet in the MSB, as on the wire) ---- */
void ip4_format(uint32_t ip, char* buf, size_t size);
bool ip4_parse(const char* s, uint32_t* out);
/* Split a fingerprint into lines of at most `width` chars for small screens. */
void fp_wrap(const char* fp, size_t width, char* out, size_t size);
/* "ssh-ed25519" -> "ED25519", "ecdsa-sha2-nistp256" -> "ECDSA P-256", "rsa-sha2-512" -> "RSA". */
const char* key_type_short(const char* key_type);

/* ---- Fonts per content ----
 * Technical text is drawn in the monospace FontKeyboard, never in a proportional font.
 * text_is_technical() (titles): one of @ / $ # [ ] % ~ | or a backslash, a ':'
 * glued to the next character ("host:22", "SHA256:", not the label colon of "Exit: 0"),
 * or a '.' between digits.
 * text_is_technical_body() (body text): the same, except that a single '.'
 * between digits ("1.2 s") is a plain number; an address or version ("192.168.1.10",
 * "0.1.0") has two. */
bool text_is_technical(const char* s);
bool text_is_technical_body(const char* s);

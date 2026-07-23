// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "report_escape.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// RFC-4180 CSV field: quote if it contains comma/quote/CR/LF; double quotes.
// Prevents an SSID with a comma/quote from breaking the column count.
void csv_field_escape(const char* in, char* out, size_t out_len) {
    if(out_len == 0) return;
    bool needs = false;
    for(const char* p = in; *p; p++) {
        if(*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            needs = true;
            break;
        }
    }
    if(!needs) {
        snprintf(out, out_len, "%s", in);
        return;
    }
    // Quoted field. Reserve room for the opening quote, closing quote and NUL, and
    // never split a doubled "" across the truncation boundary -- so even a field
    // truncated to fit the buffer stays well-formed (properly closed) CSV.
    if(out_len < 3) {
        out[0] = '\0';
        return;
    }
    size_t j = 0;
    out[j++] = '"';
    for(const char* p = in; *p; p++) {
        size_t need = (*p == '"') ? 2 : 1; // a literal quote is written doubled
        if(j + need > out_len - 2) break; // leave room for the closing quote + NUL
        if(*p == '"') out[j++] = '"';
        out[j++] = *p;
    }
    out[j++] = '"';
    out[j] = '\0';
}

// JSON string-content escape (the surrounding quotes live in the format string).
// Escapes \ " and control chars so an odd SSID or a user-set BLE tracker name
// can't produce invalid JSON that downstream tools (geojson.io, QGIS) reject.
// Truncation-safe: never emits a partial escape sequence.
void json_escape(const char* in, char* out, size_t out_len) {
    if(out_len == 0) return;
    size_t j = 0;
    for(const char* p = in; *p; p++) {
        unsigned char c = (unsigned char)*p;
        char seq[8];
        size_t need;
        switch(c) {
        case '"':
            seq[0] = '\\', seq[1] = '"', need = 2;
            break;
        case '\\':
            seq[0] = '\\', seq[1] = '\\', need = 2;
            break;
        case '\n':
            seq[0] = '\\', seq[1] = 'n', need = 2;
            break;
        case '\r':
            seq[0] = '\\', seq[1] = 'r', need = 2;
            break;
        case '\t':
            seq[0] = '\\', seq[1] = 't', need = 2;
            break;
        default:
            if(c < 0x20) {
                snprintf(seq, sizeof(seq), "\\u%04x", c);
                need = 6;
            } else {
                seq[0] = (char)c, need = 1;
            }
            break;
        }
        if(j + need > out_len - 1) break; // leave room for NUL; don't split a seq
        memcpy(out + j, seq, need);
        j += need;
    }
    out[j] = '\0';
}

// Markdown table-cell escape: a raw '|' breaks the column layout and a backtick
// opens a code span, so an odd SSID could corrupt the table; newlines would split
// the row. Truncation-safe (mirrors json_escape).
void md_escape(const char* in, char* out, size_t out_len) {
    if(out_len == 0) return;
    size_t j = 0;
    for(const char* p = in; *p; p++) {
        unsigned char c = (unsigned char)*p;
        char seq[2];
        size_t need;
        switch(c) {
        case '|':
            seq[0] = '\\', seq[1] = '|', need = 2;
            break;
        case '`':
            seq[0] = '\'', need = 1;
            break;
        case '\n':
        case '\r':
        case '\t':
            seq[0] = ' ', need = 1;
            break;
        default:
            seq[0] = (c < 0x20) ? ' ' : (char)c, need = 1;
            break;
        }
        if(j + need > out_len - 1) break; // leave room for NUL; don't split a seq
        memcpy(out + j, seq, need);
        j += need;
    }
    out[j] = '\0';
}

// XML/KML escape for element text and attribute values. Same goal as json_escape
// but for the KML reports (which are XML). Truncation-safe.
void xml_escape(const char* in, char* out, size_t out_len) {
    if(out_len == 0) return;
    size_t j = 0;
    for(const char* p = in; *p; p++) {
        unsigned char c = (unsigned char)*p;
        const char* rep = NULL;
        switch(c) {
        case '&':
            rep = "&amp;";
            break;
        case '<':
            rep = "&lt;";
            break;
        case '>':
            rep = "&gt;";
            break;
        case '"':
            rep = "&quot;";
            break;
        case '\'':
            rep = "&apos;";
            break;
        default:
            break;
        }
        if(rep) {
            size_t need = strlen(rep);
            if(j + need > out_len - 1) break;
            memcpy(out + j, rep, need);
            j += need;
        } else if(c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
            continue; // drop other control chars
        } else {
            if(j + 1 > out_len - 1) break;
            out[j++] = (char)c;
        }
    }
    out[j] = '\0';
}

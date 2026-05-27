/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <furi.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

#define ZF_NFC_TRACE_TAG "ZeroFIDO:NFC"
#define ZF_NFC_TRACE_PREVIEW_BYTES 8U
#define ZF_NFC_TRACE_QUEUE_DEPTH 16U
#define ZF_NFC_TRACE_TEXT_LEN 80U

typedef struct FuriMessageQueue FuriMessageQueue;

typedef struct {
    char text[ZF_NFC_TRACE_TEXT_LEN];
} ZfNfcTraceRecord;

#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS
void zf_transport_nfc_trace_bind(FuriMessageQueue *queue, FuriThreadId thread_id);
void zf_transport_nfc_trace_unbind(FuriMessageQueue *queue);
void zf_transport_nfc_trace_drain(FuriMessageQueue *queue);
void zf_transport_nfc_trace_format(const char *fmt, ...);

static inline void zf_transport_nfc_trace_bytes(const char *label, const uint8_t *data,
                                                size_t data_len) {
    const char *name = label ? label : "bytes";
    char hex[(ZF_NFC_TRACE_PREVIEW_BYTES * 3U) + 1U] = {0};
    size_t hex_offset = 0U;
    size_t preview_len = data_len;

    if (!data && data_len > 0U) {
        zf_transport_nfc_trace_format("%s len=%u null", name, (unsigned)data_len);
        return;
    }

    if (data_len == 0U) {
        zf_transport_nfc_trace_format("%s len=0", name);
        return;
    }

    if (preview_len > ZF_NFC_TRACE_PREVIEW_BYTES) {
        preview_len = ZF_NFC_TRACE_PREVIEW_BYTES;
    }

    for (size_t i = 0; i < preview_len; ++i) {
        int written = snprintf(&hex[hex_offset], sizeof(hex) - hex_offset, "%s%02X",
                               i == 0U ? "" : " ", data[i]);
        if (written <= 0 || (size_t)written >= (sizeof(hex) - hex_offset)) {
            break;
        }
        hex_offset += (size_t)written;
    }

    zf_transport_nfc_trace_format("%s len=%u head=%s%s", name, (unsigned)data_len, hex,
                                  data_len > preview_len ? " ..." : "");
}

static inline void zf_transport_nfc_trace_event(const char *event) {
    zf_transport_nfc_trace_format("event %s", event ? event : "?");
}

static inline void zf_transport_nfc_trace_apdu_header(const char *direction, uint8_t cla,
                                                      uint8_t ins, uint8_t p1, uint8_t p2,
                                                      size_t data_len, bool extended, bool chained,
                                                      bool has_le, size_t le) {
    zf_transport_nfc_trace_format("apdu-%s %02X %02X %02X %02X data=%u ext=%u chain=%u le=%s%u",
                                  direction ? direction : "?", cla, ins, p1, p2, (unsigned)data_len,
                                  extended ? 1U : 0U, chained ? 1U : 0U, has_le ? "" : "none/",
                                  has_le ? (unsigned)le : 0U);
}

static inline void zf_transport_nfc_trace_apdu_status(uint16_t status_word) {
    zf_transport_nfc_trace_format("apdu-tx sw=%04X data=0", status_word);
}

static inline void zf_transport_nfc_trace_apdu_tx(const uint8_t *data, size_t data_len,
                                                  uint16_t status_word) {
    (void)data;
    zf_transport_nfc_trace_format("apdu-tx sw=%04X data=%u", status_word, (unsigned)data_len);
}

#else
#define zf_transport_nfc_trace_bind(...) ((void)0)
#define zf_transport_nfc_trace_unbind(...) ((void)0)
#define zf_transport_nfc_trace_drain(...) ((void)0)
#define zf_transport_nfc_trace_format(...) ((void)0)
#define zf_transport_nfc_trace_bytes(...) ((void)0)
#define zf_transport_nfc_trace_event(...) ((void)0)
#define zf_transport_nfc_trace_apdu_header(...) ((void)0)
#define zf_transport_nfc_trace_apdu_status(...) ((void)0)
#define zf_transport_nfc_trace_apdu_tx(...) ((void)0)
#endif

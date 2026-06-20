// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Cafe com Solda / Cristiano Santana

#include "scan_log.h"
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CSV_HEADER    "timestamp,freq,protocol,uid,data,risk\n"
#define READ_BUF_SIZE 8192U

static void csv_escape(char* dst, size_t dst_size, const char* src) {
    if(!src) {
        dst[0] = '\0';
        return;
    }
    size_t i = 0;
    for(const char* p = src; *p && i < dst_size - 1; p++) {
        char c = *p;
        if(c == ',' || c == '\n' || c == '\r') c = ' ';
        dst[i++] = c;
    }
    dst[i] = '\0';
}

bool scan_log_append(Storage* storage, const ScanLogEntry* entry) {
    if(!storage || !entry) return false;

    bool need_header = !storage_common_exists(storage, CHAOS_ID_LOG_FILE);
    if(need_header) {
        storage_simply_mkdir(storage, CHAOS_ID_LOG_DIR);
    }

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char freq_buf[32], proto_buf[64], uid_buf[64], data_buf[96], risk_buf[40];
    csv_escape(freq_buf, sizeof(freq_buf), entry->freq_label);
    csv_escape(proto_buf, sizeof(proto_buf), entry->protocol);
    csv_escape(uid_buf, sizeof(uid_buf), entry->uid);
    csv_escape(data_buf, sizeof(data_buf), entry->data);
    csv_escape(risk_buf, sizeof(risk_buf), entry->risk_label);

    char line[512];
    int n = snprintf(
        line,
        sizeof(line),
        "%04d-%02d-%02d %02d:%02d:%02d,%s,%s,%s,%s,%s\n",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second,
        freq_buf,
        proto_buf,
        uid_buf,
        data_buf,
        risk_buf);

    if(n <= 0) return false;

    File* f = storage_file_alloc(storage);
    bool ok = storage_file_open(f, CHAOS_ID_LOG_FILE, FSAM_WRITE, FSOM_OPEN_APPEND);
    if(ok) {
        if(need_header) {
            storage_file_write(f, CSV_HEADER, strlen(CSV_HEADER));
        }
        size_t written = storage_file_write(f, line, (size_t)n);
        if(written != (size_t)n) ok = false;
    }
    storage_file_close(f);
    storage_file_free(f);

    FURI_LOG_I(
        "ChaosID",
        "scan_log_append: %s (%s) -> %s",
        entry->protocol,
        entry->uid,
        ok ? "ok" : "fail");
    return ok;
}

bool scan_log_read_display(Storage* storage, FuriString* out) {
    if(!storage || !out) return false;

    // HEAP alloc (8KB seria estouro de stack guard num app Flipper tipico)
    char* buffer = malloc(READ_BUF_SIZE);
    if(!buffer) {
        FURI_LOG_E("ChaosID", "scan_log_read: malloc fail");
        return false;
    }

    File* f = storage_file_alloc(storage);
    bool ok = storage_file_open(f, CHAOS_ID_LOG_FILE, FSAM_READ, FSOM_OPEN_EXISTING);
    if(!ok) {
        storage_file_free(f);
        free(buffer);
        return false;
    }

    uint16_t bytes_read = storage_file_read(f, buffer, READ_BUF_SIZE - 1);
    storage_file_close(f);
    storage_file_free(f);

    if(bytes_read == 0) {
        free(buffer);
        return false;
    }
    buffer[bytes_read] = '\0';

    // Pula header CSV
    char* line_start = strchr(buffer, '\n');
    if(!line_start) {
        free(buffer);
        return false;
    }
    line_start++;

    int count = 0;
    while(*line_start) {
        char* line_end = strchr(line_start, '\n');
        if(line_end) *line_end = '\0';

        if(strlen(line_start) > 0) {
            char ts[24] = {0}, freq[24] = {0}, proto[48] = {0}, uid[48] = {0};
            int parsed =
                sscanf(line_start, "%23[^,],%23[^,],%47[^,],%47[^,]", ts, freq, proto, uid);

            if(parsed >= 3) {
                furi_string_cat_printf(out, "%s\n  %s\n", ts, proto);
                if(parsed >= 4 && strlen(uid) > 0) {
                    furi_string_cat_printf(out, "  %s\n", uid);
                }
                furi_string_cat_str(out, "\n");
                count++;
            }
        }

        if(!line_end) break;
        line_start = line_end + 1;
    }

    free(buffer);
    return count > 0;
}

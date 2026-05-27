/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#include "zerofido_usb_diagnostics.h"

#if ZF_USB_DIAGNOSTICS
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "zerofido_storage.h"

#define ZF_USB_DIAG_FILE_PATH ZF_APP_DATA_DIR "/usb_diag.log"
#define ZF_USB_DIAG_MAX_BYTES 8192U
#define ZF_USB_DIAG_LINE_MAX 128U

static size_t zf_usb_diag_file_size(Storage *storage) {
    File *file = NULL;
    size_t size = 0U;

    file = storage_file_alloc(storage);
    if (!file) {
        return 0U;
    }
    if (storage_file_open(file, ZF_USB_DIAG_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size = storage_file_size(file);
        storage_file_close(file);
    }
    storage_file_free(file);
    return size;
}

static bool zf_usb_diag_truncate(Storage *storage) {
    File *file = NULL;
    bool ok = false;

    if (!storage || !zf_storage_ensure_app_data_dir(storage)) {
        return false;
    }
    file = storage_file_alloc(storage);
    if (!file) {
        return false;
    }
    ok = storage_file_open(file, ZF_USB_DIAG_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if (ok) {
        storage_file_close(file);
    }
    storage_file_free(file);
    return ok;
}

static void zf_usb_diag_append(Storage *storage, const char *line, size_t len) {
    File *file = NULL;
    const char newline = '\n';

    if (!storage || !line || len == 0U || !zf_storage_ensure_app_data_dir(storage)) {
        return;
    }
    if (len + 1U > ZF_USB_DIAG_MAX_BYTES) {
        return;
    }
    if (zf_usb_diag_file_size(storage) + len + 1U > ZF_USB_DIAG_MAX_BYTES &&
        !zf_usb_diag_truncate(storage)) {
        return;
    }

    file = storage_file_alloc(storage);
    if (!file) {
        return;
    }
    if (storage_file_open(file, ZF_USB_DIAG_FILE_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        if (storage_file_write(file, line, len) == len) {
            (void)storage_file_write(file, &newline, sizeof(newline));
        }
        storage_file_close(file);
    }
    storage_file_free(file);
}

static size_t zf_usb_diag_line_len(const char *line) {
    size_t len = 0U;

    while (len < ZF_USB_DIAG_LINE_MAX && line[len] != '\0') {
        ++len;
    }
    return len;
}

void zf_usb_diag_reset(Storage *storage) {
    (void)zf_usb_diag_truncate(storage);
}

void zf_usb_diag_log(Storage *storage, const char *line) {
    if (!line) {
        return;
    }
    zf_usb_diag_append(storage, line, zf_usb_diag_line_len(line));
}

void zf_usb_diag_logf(Storage *storage, const char *fmt, ...) {
    char line[ZF_USB_DIAG_LINE_MAX];
    va_list args;
    int written = 0;
    size_t len = 0U;

    if (!fmt) {
        return;
    }
    va_start(args, fmt);
    written = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    if (written <= 0) {
        return;
    }

    len = (size_t)written;
    if (len >= sizeof(line)) {
        len = sizeof(line) - 1U;
        line[len] = '\0';
    }
    zf_usb_diag_append(storage, line, len);
}
#endif

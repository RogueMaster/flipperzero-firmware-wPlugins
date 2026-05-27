/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#pragma once

#include "zerofido_types.h"

typedef struct Storage Storage;

#if ZF_USB_DIAGNOSTICS
void zf_usb_diag_reset(Storage *storage);
void zf_usb_diag_log(Storage *storage, const char *line);
void zf_usb_diag_logf(Storage *storage, const char *fmt, ...);
#else
static inline void zf_usb_diag_reset(Storage *storage) {
    (void)storage;
}

static inline void zf_usb_diag_log(Storage *storage, const char *line) {
    (void)storage;
    (void)line;
}

static inline void zf_usb_diag_logf(Storage *storage, const char *fmt, ...) {
    (void)storage;
    (void)fmt;
}
#endif

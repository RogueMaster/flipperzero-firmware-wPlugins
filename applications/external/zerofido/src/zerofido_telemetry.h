/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef ZF_HOST_TEST
static inline void zf_telemetry_log(const char *event) {
    (void)event;
}

static inline void zf_telemetry_log_oom(const char *event, size_t requested_size) {
    (void)event;
    (void)requested_size;
}

static inline size_t zf_telemetry_heap_max_free_block(void) {
    return SIZE_MAX;
}
#elif !defined(ZF_RELEASE_DIAGNOSTICS) || !ZF_RELEASE_DIAGNOSTICS
static inline void zf_telemetry_log(const char *event) {
    (void)event;
}

static inline void zf_telemetry_log_oom(const char *event, size_t requested_size) {
    (void)event;
    (void)requested_size;
}

size_t zf_telemetry_heap_max_free_block(void);
#else
void zf_telemetry_log(const char *event);
void zf_telemetry_log_oom(const char *event, size_t requested_size);
size_t zf_telemetry_heap_max_free_block(void);
#endif

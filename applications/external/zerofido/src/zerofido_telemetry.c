/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#include "zerofido_telemetry.h"

#include "zerofido_types.h"

#include <furi.h>

#ifndef ZF_HOST_TEST
#include <furi/core/memmgr.h>
#include <furi/core/memmgr_heap.h>
#endif

#define ZF_TELEMETRY_TAG "ZeroFIDO:MEM"

#if !defined(ZF_HOST_TEST) && ZF_RELEASE_DIAGNOSTICS
void zf_telemetry_log(const char *event) {
    FuriThreadId thread_id = furi_thread_get_current_id();

    FURI_LOG_I(ZF_TELEMETRY_TAG, "%s free=%u min=%u maxblk=%u stack=%lu", event ? event : "?",
               (unsigned)memmgr_get_free_heap(), (unsigned)memmgr_get_minimum_free_heap(),
               (unsigned)memmgr_heap_get_max_free_block(),
               (unsigned long)(thread_id ? furi_thread_get_stack_space(thread_id) : 0U));
}

void zf_telemetry_log_oom(const char *event, size_t requested_size) {
    FuriThreadId thread_id = furi_thread_get_current_id();

    FURI_LOG_E(ZF_TELEMETRY_TAG, "%s oom request=%u free=%u min=%u maxblk=%u stack=%lu",
               event ? event : "?", (unsigned)requested_size, (unsigned)memmgr_get_free_heap(),
               (unsigned)memmgr_get_minimum_free_heap(), (unsigned)memmgr_heap_get_max_free_block(),
               (unsigned long)(thread_id ? furi_thread_get_stack_space(thread_id) : 0U));
}

size_t zf_telemetry_heap_max_free_block(void) {
    return memmgr_heap_get_max_free_block();
}
#endif

#if !defined(ZF_HOST_TEST) && !ZF_RELEASE_DIAGNOSTICS
size_t zf_telemetry_heap_max_free_block(void) {
    return memmgr_heap_get_max_free_block();
}
#endif

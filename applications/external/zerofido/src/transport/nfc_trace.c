/*
 * ZeroFIDO
 * Copyright (C) 2026 Alex Stoyanov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#ifndef ZF_USB_ONLY

#include "nfc_trace.h"

#if defined(ZF_RELEASE_DIAGNOSTICS) && ZF_RELEASE_DIAGNOSTICS

#include <string.h>

#if defined(ZF_HOST_TEST)
void zf_transport_nfc_trace_bind(FuriMessageQueue *queue, FuriThreadId thread_id) {
    (void)queue;
    (void)thread_id;
}

void zf_transport_nfc_trace_unbind(FuriMessageQueue *queue) {
    (void)queue;
}

void zf_transport_nfc_trace_drain(FuriMessageQueue *queue) {
    (void)queue;
}

void zf_transport_nfc_trace_format(const char *fmt, ...) {
    char text[ZF_NFC_TRACE_TEXT_LEN];
    va_list args;

    if (!fmt) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    FURI_LOG_I(ZF_NFC_TRACE_TAG, "%s", text);
}
#else
#include "nfc_worker.h"

static FuriMessageQueue *zf_nfc_trace_queue = NULL;
static FuriThreadId zf_nfc_trace_worker_thread_id = 0;
static uint32_t zf_nfc_trace_dropped = 0U;
static FuriMutex *zf_nfc_trace_mutex = NULL;

static void zf_transport_nfc_trace_lock(void) {
    if (zf_nfc_trace_mutex) {
        furi_mutex_acquire(zf_nfc_trace_mutex, FuriWaitForever);
    }
}

static void zf_transport_nfc_trace_unlock(void) {
    if (zf_nfc_trace_mutex) {
        furi_mutex_release(zf_nfc_trace_mutex);
    }
}

void zf_transport_nfc_trace_bind(FuriMessageQueue *queue, FuriThreadId thread_id) {
    if (!zf_nfc_trace_mutex) {
        zf_nfc_trace_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    }
    if (!zf_nfc_trace_mutex) {
        return;
    }
    zf_transport_nfc_trace_lock();
    zf_nfc_trace_queue = queue;
    zf_nfc_trace_worker_thread_id = thread_id;
    zf_nfc_trace_dropped = 0U;
    zf_transport_nfc_trace_unlock();
}

void zf_transport_nfc_trace_unbind(FuriMessageQueue *queue) {
    zf_transport_nfc_trace_lock();
    if (zf_nfc_trace_queue == queue) {
        zf_nfc_trace_queue = NULL;
        zf_nfc_trace_worker_thread_id = 0;
        zf_nfc_trace_dropped = 0U;
    }
    zf_transport_nfc_trace_unlock();
}

void zf_transport_nfc_trace_format(const char *fmt, ...) {
    ZfNfcTraceRecord record;
    FuriMessageQueue *queue = NULL;
    FuriThreadId worker_thread_id = 0;
    va_list args;

    if (!fmt) {
        return;
    }

    memset(&record, 0, sizeof(record));
    va_start(args, fmt);
    vsnprintf(record.text, sizeof(record.text), fmt, args);
    va_end(args);

    zf_transport_nfc_trace_lock();
    queue = zf_nfc_trace_queue;
    worker_thread_id = zf_nfc_trace_worker_thread_id;
    if (!queue) {
        zf_transport_nfc_trace_unlock();
        return;
    }
    if (furi_message_queue_put(queue, &record, 0U) != FuriStatusOk) {
        zf_nfc_trace_dropped++;
        zf_transport_nfc_trace_unlock();
        return;
    }
    zf_transport_nfc_trace_unlock();
    if (worker_thread_id) {
        furi_thread_flags_set(worker_thread_id, ZF_NFC_WORKER_EVT_TRACE);
    }
}

void zf_transport_nfc_trace_drain(FuriMessageQueue *queue) {
    ZfNfcTraceRecord record;
    uint32_t dropped = 0U;

    if (!queue) {
        return;
    }

    zf_transport_nfc_trace_lock();
    dropped = zf_nfc_trace_dropped;
    zf_nfc_trace_dropped = 0U;
    zf_transport_nfc_trace_unlock();
    if (dropped > 0U) {
        FURI_LOG_I(ZF_NFC_TRACE_TAG, "trace dropped=%lu", (unsigned long)dropped);
    }

    while (furi_message_queue_get(queue, &record, 0U) == FuriStatusOk) {
        FURI_LOG_I(ZF_NFC_TRACE_TAG, "%s", record.text);
    }
}

#endif
#endif

#endif

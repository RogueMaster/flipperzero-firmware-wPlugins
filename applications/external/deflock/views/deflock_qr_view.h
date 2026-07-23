// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#pragma once

#include <gui/view.h>

typedef struct DeflockQrView DeflockQrView;

/** Called when the user pages Left/Right; delta is -1 or +1. */
typedef void (*DeflockQrPageCallback)(void* context, int delta);

DeflockQrView* deflock_qr_view_alloc(void);
void deflock_qr_view_free(DeflockQrView* qv);
View* deflock_qr_view_get_view(DeflockQrView* qv);

/** Set the owning ReconApp pointer (read for live data inside the draw callback). */
void deflock_qr_view_set_app(DeflockQrView* qv, void* app);

/** Set the Left/Right paging callback. */
void deflock_qr_view_set_page_callback(DeflockQrView* qv, DeflockQrPageCallback cb, void* context);

/**
 * Render the given URL as a QR for the camera at list-position (index+1)/total, and
 * stash the on-screen text fields. Encoding happens here (off the draw path). Returns
 * true if the QR encoded; on false the view still shows the text fallback.
 */
bool deflock_qr_view_set_content(
    DeflockQrView* qv,
    const char* url,
    int index,
    int total,
    const char* coords,
    const char* conf,
    const char* tags);

/** Show the empty-state screen (no marked cameras). */
void deflock_qr_view_set_empty(DeflockQrView* qv);

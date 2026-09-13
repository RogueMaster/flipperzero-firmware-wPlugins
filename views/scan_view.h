// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// ScanView - the "waiting for a badge" screen used by Punch, register and
// replace-chip: header/hint text (what's being scanned for), the active
// reader technology, and Left/Right chevrons to change it. Back is not
// captured here - it propagates to the scene like any other unhandled key.
// =============================================================================

#include <gui/view.h>

typedef struct ScanView ScanView;
typedef void (*ScanViewNavCallback)(int direction, void* context);

ScanView* scan_view_alloc(void);
void scan_view_free(ScanView* view);
View* scan_view_get_view(ScanView* view);

// header/text describe what's being scanned for (purpose-specific); tech is
// the active reader technology's short label ("NFC" / "RFID" / "iBTN").
void scan_view_set_content(ScanView* view, const char* header, const char* text, const char* tech);

// Callback invoked (GUI thread) when the user presses Left (-1) or Right (+1).
void scan_view_set_nav_callback(ScanView* view, ScanViewNavCallback cb, void* context);

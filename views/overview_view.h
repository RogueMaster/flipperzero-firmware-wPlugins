// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// OverviewView - a simple per-collaborator dashboard: name in the middle,
// three stat lines underneath (today/week/month/break, pre-formatted and
// localized by the scene), Left/Right to switch to the previous/next
// collaborator. One glance per person, no menus.
// =============================================================================

#include <gui/view.h>

typedef struct OverviewView OverviewView;

// Invoked (GUI thread) when Left (-1) or Right (+1) is pressed.
typedef void (*OverviewNavCallback)(int direction, void* context);

OverviewView* overview_view_alloc(void);
void overview_view_free(OverviewView* view);
View* overview_view_get_view(OverviewView* view);

void overview_view_set_nav_callback(OverviewView* view, OverviewNavCallback cb, void* context);

// Set everything shown for the current collaborator. Each line is a complete,
// already-localized "Label  H:MM" string (or NULL/empty to leave it blank).
// index/count are 1-based / total, shown as a small "i/N" indicator.
void overview_view_set_person(
    OverviewView* view,
    const char* name,
    const char* line1,
    const char* line2,
    const char* line3,
    const char* line4,
    size_t index,
    size_t count);

// Show a placeholder message instead of a person (e.g. no badges yet).
void overview_view_set_empty(OverviewView* view, const char* message);

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// WorkView - the kiosk "Work mode" screen: a live date/time clock. When a
// collaborator taps a badge it briefly shows a greeting (Welcome / Goodbye).
// Back is captured (not handled here) so the scene can require the PIN to exit.
// =============================================================================

#include <gui/view.h>

typedef struct WorkView WorkView;
typedef void (*WorkViewExitCallback)(void* context);

WorkView* work_view_alloc(void);
void work_view_free(WorkView* work_view);
View* work_view_get_view(WorkView* work_view);

// Update the displayed clock.
void work_view_set_clock(WorkView* work_view, const char* date, const char* time);

// Show a greeting line (e.g. "Welcome, Mario"); pass NULL to clear it.
void work_view_set_greeting(WorkView* work_view, const char* greeting);

// Set the footer hint text (e.g. localized "PIN to exit").
void work_view_set_footer(WorkView* work_view, const char* footer);

// Callback invoked when the user presses Back (used to trigger PIN exit).
void work_view_set_exit_callback(WorkView* work_view, WorkViewExitCallback cb, void* context);

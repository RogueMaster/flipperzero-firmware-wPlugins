// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// PinView - a fast 4-step arrow-sequence "PIN".
//
// The code is a sequence of 4 directions (Up/Down/Left/Right). Each arrow press
// adds one step; after the 4th it auto-submits. OK clears the entry to restart.
// Entries are masked (filled dots), so the sequence is never shown on screen.
// get_code() returns the sequence as a string of 'U'/'D'/'L'/'R'.
// =============================================================================

#include <gui/view.h>

typedef struct PinView PinView;
typedef void (*PinViewCallback)(void* context);

PinView* pin_view_alloc(void);
void pin_view_free(PinView* pin_view);
View* pin_view_get_view(PinView* pin_view);

// Reset the entry and set the header title.
void pin_view_reset(PinView* pin_view, const char* title);

// Copy the entered code (digits + terminator) into out.
void pin_view_get_code(PinView* pin_view, char* out, size_t out_size);

// Callback invoked when the user submits the code (OK on the last digit).
void pin_view_set_callback(PinView* pin_view, PinViewCallback callback, void* context);

// Show a short message under the boxes (e.g. remaining attempts).
void pin_view_set_message(PinView* pin_view, const char* message);

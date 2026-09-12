// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// PinView - a small custom view for 4-digit PIN entry.
//
// Controls: Up/Down change the active digit, Left/Right move between digits,
// OK advances (and submits on the last digit). Digits are masked with '*';
// only the digit being edited is shown, so the full PIN is never displayed.
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

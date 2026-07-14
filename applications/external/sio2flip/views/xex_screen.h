/*
 * This file is part of the 8-bit ATAR SIO Emulator for Flipper Zero
 * (https://github.com/cepetr/sio2flip).
 * Copyright (c) 2025
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gui/view.h>

#include "emu/xex_loader.h"

// The screen showing the XEX loader state
typedef struct XexScreen XexScreen;

typedef enum {
    XexScreenMenuKey_Cancel,
} XexScreenMenuKey;

// Callback invoked when a menu button is pressed
typedef void (*XexScreenMenuCallback)(void* context, XexScreenMenuKey key);

// Allocates XEX loader screen
XexScreen* xex_screen_alloc(void);

// Frees XEX loader screen
void xex_screen_free(XexScreen* screen);

// Returns the view of the XEX loader screen
View* xex_screen_get_view(XexScreen* screen);

// Updates the XEX screen according XEX loader state
void xex_screen_update_state(XexScreen* screen, XexLoader* xex_loader);

// Sets the callback invoked when the menu button is pressed
void xex_screen_set_menu_callback(XexScreen* screen, XexScreenMenuCallback callback, void* context);

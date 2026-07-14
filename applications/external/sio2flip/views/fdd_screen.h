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

#include "emu/fdd_emulator.h"

// The screen showing the FDD emulator state
typedef struct FddScreen FddScreen;

typedef enum {
    FddScreenMenuKey_Prev,
    FddScreenMenuKey_Next,
    FddScreenMenuKey_Config,
} FddScreenMenuKey;

// Callback invoked when a menu button is pressed
typedef void (*FddScreenMenuCallback)(void* context, FddScreenMenuKey key);

// Allocates FDD screen
FddScreen* fdd_screen_alloc(void);

// Frees FDD screen
void fdd_screen_free(FddScreen* screen);

// Returns the view of the FDD screen
View* fdd_screen_get_view(FddScreen* screen);

// Updates the FDD screen according FDD emulator state
void fdd_screen_update_state(FddScreen* screen, FddEmulator* emu);

// Updates the FDD screen to show the FDD activity
void fdd_screen_update_activity(FddScreen* screen, FddActivity activity, uint16_t sector);

// Sets the callback invoked when the menu button is pressed
void fdd_screen_set_menu_callback(FddScreen* screen, FddScreenMenuCallback callback, void* context);

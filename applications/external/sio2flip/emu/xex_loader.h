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

#include <furi.h>
#include <storage/storage.h>

#include "app/app_config.h"

#include "sio_driver.h"
#include "xex_file.h"

typedef struct XexLoader XexLoader;

// Creates a XEX file loader
XexLoader* xex_loader_alloc(AppConfig* config);

// Frees the XEX file loader
void xex_loader_free(XexLoader* loader);

// Starts the XEX file loader on specified SIO driver
bool xex_loader_start(XexLoader* loader, XexFile* file, SIODriver* sio);

// Stops the XEX file loader
void xex_loader_stop(XexLoader* loader);

// Gets the loaded XEX file or NULL if no file is loaded
XexFile* xex_loader_file(XexLoader* loader);

typedef void (*XexActivityCallback)(void* context);

void xex_loader_set_activity_callback(
    XexLoader* loader,
    XexActivityCallback callback,
    void* context);

// Gets the offset of the last byte read from the XEX file
size_t xex_loader_last_offset(XexLoader* loader);

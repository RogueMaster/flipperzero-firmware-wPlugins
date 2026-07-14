
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

#include "app_common.h"

static FuriString* last_error_title = NULL;
static FuriString* last_error_message = NULL;

void set_last_error(const char* title, const char* fmt, ...) {
    if(last_error_title) {
        furi_string_free(last_error_title);
    }
    if(last_error_message) {
        furi_string_free(last_error_message);
    }

    last_error_title = furi_string_alloc_set(title);

    va_list args;
    va_start(args, fmt);
    last_error_message = furi_string_alloc_vprintf(fmt, args);
    va_end(args);

    FURI_LOG_E(TAG, "%s", furi_string_get_cstr(last_error_message));
}

FuriString* get_last_error_title(void) {
    return last_error_title;
}

FuriString* get_last_error_message(void) {
    return last_error_message;
}

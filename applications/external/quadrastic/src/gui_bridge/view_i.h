/*
 * Copyright 2025 Ivan Barsukov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file view_i.h
 * GUI: internal View API
 */

#pragma once

#include <gui/view.h>
#include <stddef.h>

typedef struct {
    FuriMutex* mutex;
    uint8_t data[];
} ViewModelLocking;

struct View {
    ViewDrawCallback draw_callback;
    ViewInputCallback input_callback;
    ViewCustomCallback custom_callback;

    ViewModelType model_type;
    ViewNavigationCallback previous_callback;
    ViewCallback enter_callback;
    ViewCallback exit_callback;
    ViewOrientation orientation;

    ViewUpdateCallback update_callback;
    void* update_callback_context;

    void* model;
    void* context;
    ViewAsciiCallback ascii_callback;
};

/* This app reads draw_callback / input_callback / context straight out of a
 * firmware-owned View, so this copy must stay byte-identical to the firmware's
 * private struct View. The layout is the same on Unleashed dev 000b238e and
 * Official 1.5.1, verified against each firmware.elf's DWARF (2026-11).
 * RM adds ascii_callback after context. Fail the build instead of the device
 * if this copy is ever edited. */
_Static_assert(
    offsetof(struct View, draw_callback) == 0,
    "struct View layout drifted: re-sync this copy against the firmware");
_Static_assert(
    offsetof(struct View, input_callback) == 4,
    "struct View layout drifted: re-sync this copy against the firmware");
_Static_assert(
    offsetof(struct View, context) == 44,
    "struct View layout drifted: re-sync this copy against the firmware");
_Static_assert(
    offsetof(struct View, ascii_callback) == 48,
    "struct View layout drifted: re-sync this copy against the firmware");
_Static_assert(
    sizeof(struct View) == 52,
    "struct View layout drifted: re-sync this copy against the firmware");

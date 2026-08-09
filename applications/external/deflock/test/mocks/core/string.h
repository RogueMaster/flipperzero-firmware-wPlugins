// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Minimal host mock of Flipper's <core/string.h> (FuriString). Only the surface
// the unit-tested modules + tests touch is provided; the real FuriString is
// firmware (mlib m-string). Resolved ahead of the SDK header via `-Imocks`.
#pragma once

#include <stddef.h>

typedef struct FuriString FuriString;

FuriString* furi_string_alloc(void);
void furi_string_free(FuriString* s);
void furi_string_reset(FuriString* s);
void furi_string_cat_str(FuriString* s, const char* suffix);
const char* furi_string_get_cstr(FuriString* s);

// The real header dispatches furi_string_cat(a, b) via _Generic to the FuriString
// or const-char* variant. The tested code only ever appends C
// string literals, so map straight to the _str variant.
#define furi_string_cat(s, cstr) furi_string_cat_str((s), (cstr))

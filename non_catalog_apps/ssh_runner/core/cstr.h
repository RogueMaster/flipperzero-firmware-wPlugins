/*
 * Small string helpers. The Flipper firmware API disables some libc functions for
 * apps (strpbrk, strtok_r, qsort, atol, ...: APPCHK rejects them), so the app uses
 * these instead. Pure C, host-tested.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

/* True if `s` contains any character of `set`. */
bool cstr_has_any(const char* s, const char* set);
/* In-place tokenizer on single spaces (like strtok_r(s, " ")); *save starts NULL. */
char* cstr_next_word(char* s, char** save);
/* Insertion sort of `count` fixed-size NUL-terminated strings, case-insensitive. */
void cstr_sort(char* base, size_t count, size_t stride);
/* Leading decimal number (ignores trailing junk), 0 if none; clamps to max. */
unsigned long cstr_to_ulong(const char* s, unsigned long max);

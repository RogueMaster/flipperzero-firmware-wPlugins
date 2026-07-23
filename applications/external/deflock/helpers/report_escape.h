// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file report_escape.h
 * Truncation-safe output escapers for the report writers (CSV / JSON / Markdown
 * / XML-KML). Each writes a NUL-terminated, well-formed escaping of `in` into
 * `out` (capacity `out_len`) and never emits a partial escape sequence at the
 * truncation boundary. Pure (libc only), so they can be unit-tested against
 * injection payloads independently of the firmware report writers (bug B12).
 */
#pragma once

#include <stddef.h>

/** RFC-4180 CSV field: quotes if it contains ,"/CR/LF and doubles embedded quotes. */
void csv_field_escape(const char* in, char* out, size_t out_len);

/** JSON string content: escapes \\ " and control chars (surrounding quotes are the caller's). */
void json_escape(const char* in, char* out, size_t out_len);

/** Markdown table cell: escapes | and backtick; spaces out newlines/tabs. */
void md_escape(const char* in, char* out, size_t out_len);

/** XML/KML text + attribute value: &<>"' entities; drops other control chars. */
void xml_escape(const char* in, char* out, size_t out_len);

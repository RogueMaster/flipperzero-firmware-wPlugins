/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 CinderSocket
 *
 * Append-only byte buffer used to assemble DESFire responses.
 *
 * Append-only, fixed-capacity, byte-addressed. The engine never needs
 * sub-byte addressing.
 *
 * Appends that would overflow the buffer are dropped rather than growing it;
 * `DFC_BYTEBUF_MAX` is sized above the largest response the engine can
 * produce, and the caller bounds output separately.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef DFC_BYTEBUF_MAX
#define DFC_BYTEBUF_MAX 256
#endif

typedef struct {
    uint8_t data[DFC_BYTEBUF_MAX];
    size_t size_bytes;
} DfcByteBuf;

DfcByteBuf* dfc_bytebuf_alloc(size_t max_size);
void dfc_bytebuf_free(DfcByteBuf* b);
void dfc_bytebuf_reset(DfcByteBuf* b);
void dfc_bytebuf_append_bytes(DfcByteBuf* b, const uint8_t* data, size_t len);
void dfc_bytebuf_append_byte(DfcByteBuf* b, uint8_t byte);
size_t dfc_bytebuf_get_size_bytes(const DfcByteBuf* b);
const uint8_t* dfc_bytebuf_get_data(const DfcByteBuf* b);
uint8_t dfc_bytebuf_get_byte(const DfcByteBuf* b, size_t index);

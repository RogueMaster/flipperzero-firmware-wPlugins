// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#ifndef PNG_H
#define PNG_H

#pragma once

/* Max width/height of a single image. Images can be smaller, but never
 * larger as this allocates the full amount of data.
 */
void* png_alloc(uint32_t width, uint32_t height);

void png_reset(void* png_handle, size_t px_w, size_t px_h);

void png_add_height(void* png_handle, size_t px_h);

/* Modifies the current IDAT chunk to unset BFINAL bit of deflate, and then
 * recalculates the whole CRC for that chunk. When done, the last IDAT chunk
 * can then be rewritten in preparation to append another IDAT chunk with more
 * image data.
 *
 * Must be run before png_add_height() as that will clobber current IDAT sections
 */
void png_deflate_unfinal(void* png_handle);

void png_palette_set(void* png_handle, uint8_t rgb[4][3]);

void png_free(void* png_handle);

enum png_chunks {
    CHUNK_START = 0,
    IHDR = 0, // Start of file through IHDR end
    PLTE,
    IDAT_ZLIB,
    IDAT, // Current whole IDAT
    IDAT_CHECK, // The final IDAT with zlib adler32
    IEND,
    CHUNK_COUNT, // Special, sentry of normal sections that can be looped
    LAST_IDAT, // Special, length of current whole IDAT, IDAT_check, and IEND.
        // Used to be able to overwrite a previous IDAT chunk, e.g.
        // to unmark the BFINAL bit.
};

uint8_t* png_buf_get(void* png_handle, enum png_chunks chunk);
size_t png_len_get(void* png_handle, enum png_chunks chunk);

void png_dat_write(void* png_handle, uint8_t* image_buf);

#endif // PNG_H

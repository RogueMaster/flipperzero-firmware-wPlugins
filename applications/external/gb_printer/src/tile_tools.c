// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Copy src to dst converting from gb tile format to scanline format for
 * standard image data.
 */
/* TODO: Make this able to run against a single buffer rather than copying from
 * one buffer to another.
 */
void tile_to_scanline(uint8_t* src, size_t tiles_w, size_t tiles_h) {
    //size_t tile_w = 2; // 2 byte wide
    size_t tile_h = 8; // 8 byte tall
    size_t tile_x_max = tiles_w;
    size_t tile_y_max = tiles_h;
    size_t tile_x = 0;
    size_t tile_y = 0;
    size_t tile_suby = 0;
    size_t tmp;
    int i;
    /* One row of tiles is 160 px (per line) / 4 (px per byte) * 8 lines */
    /* TODO: Should this be on the heap? */
    uint8_t line_buf[320];
    uint8_t* line_buf_ptr;

    for(tile_y = 0; tile_y < tile_y_max; tile_y++) {
        line_buf_ptr = line_buf;
        for(tile_suby = 0; tile_suby < tile_h; tile_suby++) {
            for(tile_x = 0; tile_x < tile_x_max; tile_x++) {
                /* need to shuffle bits */
                /* from: 0 1 2 3 4 5 6 7  8 9 a b c d e f
				 *   to: 8 0 9 1 a 2 b 3  c 4 d 5 e 6 f 7
				 */
                tmp = (tile_x * 16) + (tile_y * 320) + (tile_suby * 2);
                *line_buf_ptr = 0;
                for(i = 0; i < 8; i++) {
                    *line_buf_ptr <<= 1;
                    *line_buf_ptr |= !!(src[tmp + 1] & (0x80 >> i));
                    *line_buf_ptr <<= 1;
                    *line_buf_ptr |= !!(src[tmp] & (0x80 >> i));
                    if(i == 3) {
                        line_buf_ptr++;
                        *line_buf_ptr = 0;
                    }
                }
                line_buf_ptr++;
            }
        }
        memcpy(src + (tile_y * 320), line_buf, 320);
    }
}

void scanline_to_tile(uint8_t* dst, uint8_t* src, size_t tiles_w, size_t tiles_h) {
    //size_t tile_w = 2; // 2 byte wide
    size_t tile_h = 8; // 8 byte tall
    size_t tile_x_max = tiles_w;
    size_t tile_y_max = tiles_h;
    size_t tile_x = 0;
    size_t tile_y = 0;
    size_t tile_suby = 0;
    size_t tmp;
    int i;

    for(tile_y = 0; tile_y < tile_y_max; tile_y++) {
        for(tile_x = 0; tile_x < tile_x_max; tile_x++) {
            for(tile_suby = 0; tile_suby < tile_h; tile_suby++) {
                /* need to shuffle bits */
                /* from: 8 0 9 1 a 2 b 3  c 4 d 5 e 6 f 7
				 *   to: 0 1 2 3 4 5 6 7  8 9 a b c d e f
				 */
                tmp = (tile_x * 2) + (tile_y * 320) + (tile_suby * 40);
                *dst = 0;
                for(i = 0; i < 8; i++) {
                    dst[1] <<= 1;
                    dst[0] <<= 1;
                    dst[1] |= !!(src[tmp] & (0x80 >> ((i * 2) % 8)));
                    dst[0] |= !!(src[tmp] & (0x40 >> ((i * 2) % 8)));
                    if(i == 3) tmp++;
                }
                dst += 2;
            }
        }
    }
}

// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#include <furi.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <src/include/crc.h>
#include <src/include/fgp_palette.h>

#include <src/include/png.h>

/* XXX TODO NOTE:
 * the use of bswap32 is fine, but, should port some form of htonl and ntohl to
 * ensure future portability.
 */

/* Must always be followed by a CRC of the data, and type! */
struct __attribute__((__packed__)) ihdr {
    uint8_t magic
        [8]; // 137 80 78 71 13 10 26 10, while magic is not a part of IHDR, for ease of use we pretend it is
    uint32_t data_len; // len is just the data itself
    uint8_t type[4]; // Usually represented in ASCII
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t comp_method;
    uint8_t filter_method;
    uint8_t interlace_method;
    uint32_t crc;
};

/* Must always be followed by a CRC of the data, and type! */
struct __attribute__((__packed__)) plte {
    uint32_t data_len; // len is just the data itself
    uint8_t type[4]; // Usually represented in ASCII
    uint8_t color[4][3];
    uint32_t crc;
};

/* Must always be followed by a CRC of the data, and type! */
struct __attribute__((__packed__)) idat_zlib {
    uint32_t data_len; // len is just the data itself
    uint8_t type[4]; // Usually represented in ASCII
    uint8_t zlib_flags; // Should always be 0x78, 32k window size, DEFLATE
    uint8_t
        zlib_addl_flags; // Should always be 0x01, indicate no compression (unused for decomp), no dictionary, check bits for flags, and last deflate stream
    uint32_t crc;
};

/* Must always be followed by a CRC of the data, and type! */
struct __attribute__((__packed__)) idat_image {
    uint32_t data_len; // len is just the data itself
    uint8_t type[4]; // Usually represented in ASCII
    uint8_t deflate_btype; // Should always be 0x01, I think this says no compression?
    uint16_t len; // Len of image data, includes filter type
    uint16_t nlen; // 1s compliment of len
    uint8_t image[]; // Finally, the actual image data!
    /* NOTE! Add a uint32_t to this for the CRC! */
};

/* Must always be followed by a CRC of the data, and type! */
struct __attribute__((__packed__)) idat_check {
    uint32_t data_len; // len is just the data itself
    uint8_t type[4]; // Usually represented in ASCII
    uint32_t check_data; // zlib adler32 check
    uint32_t crc;
};

/* Must always be followed by a CRC of the data, and type! */
struct __attribute__((__packed__)) iend {
    uint32_t data_len; // len is just the data itself
    uint8_t type[4]; // Usually represented in ASCII
    uint32_t crc;
};

struct png_handle {
    /* Structures to represent the actual PNG file data directly */
    struct ihdr ihdr;
    struct plte plte;
    struct idat_zlib idat_zlib;
    //struct idat idat;
    struct idat_check idat_check;
    struct iend iend;

    /* Variables to track certain data that we need */
    size_t height_px; // Total height of the whole image
    size_t width_px; // Total width of the whole image
    size_t image_len; // IDAT Image data in bytes
    size_t image_height_px; // IDAT image height

    /* The max length a single IDAT chunk can be.
	 * Note that this includes the CRC for IDAT.
	 *
	 * In other words, the IDAT chunk will be the length of
	 * sizeof(struct idat) + image_len_max
	 */
    size_t image_len_max; // The max length a single IDAT chunk can be

    /* Tracking the adler32 steps. */
    uint32_t adler_a;
    uint32_t adler_b;

    /* The IDAT chunk is at the very end since it has variable length
	 * data. Just easier to do it this way than to deal with a pointer
	 * off to somewhere else in the middle of the struct. That is luckily
	 * the only variable data.
	 *
	 * The CRC of the idat chunk is considered to be included in that.
	 * That is, whatever the actual data length of IDAT is, add 4 to also
	 * hold the CRC.
	 */
    struct idat_image idat_image;
};

static const struct ihdr ihdr_data = {
    .magic = {137, 80, 78, 71, 13, 10, 26, 10},
    .data_len = 218103808, // (uint32_t)__builtin_bswap32(13)
    .type = {'I', 'H', 'D', 'R'},
    .width = 0, // Placeholder
    .height = 0, // Placeholder
    .bit_depth = 2, // Always 2bpp
    .color_type = 3, // Always indexed mode
    .comp_method = 0, // always 0
    .filter_method = 0, // No filters
    .interlace_method = 0, // No interlacing
};

static const struct plte plte_data = {
    .data_len = 201326592, // (uint32_t)__builtin_bswap32(12)
    .type = {'P', 'L', 'T', 'E'},
};

static const struct idat_zlib idat_zlib_data = {
    .data_len = 33554432, // (uint32_t)__builtin_bswap32(2)
    .type = {'I', 'D', 'A', 'T'}, // Usually represented in ASCII
    .zlib_flags = 0x58, // Should always be 0x58, 8k window size, DEFLATE
    .zlib_addl_flags =
        0x09, // Should always be 0x09, indicate no compression (unused for decomp), no dictionary, check bits for flags
    .crc = 0x42d24577,
};

static const struct idat_image idat_data = {
    .data_len = 0, // len is just the data itself
    .type = {'I', 'D', 'A', 'T'}, // Usually represented in ASCII
    .deflate_btype = 0x01, // This should be 0x01, no compression?
    .len = 0, // Len of image data, includes filter type
    .nlen = 0, // 1s compliment of len
};

static const struct idat_check idat_check_data = {
    .data_len = 67108864, // (uint32_t)__builtin_bswap32(4)
    .type = {'I', 'D', 'A', 'T'}, // Usually represented in ASCII
};

static const struct iend iend_data = {
    .data_len = 0,
    .type = {0x49, 0x45, 0x4e, 0x44},
    .crc = 0x826042ae,
};

/* Both updates ihdr as well as sets image_len used in idat*/
void png_add_height(void* png_handle, size_t px_h) {
    struct png_handle* png = png_handle;

    /* Since image data is 4 px per bit, the image length is (height*width/4),
	 * and, PNG has a byte starting each "scanline" (each row), so add in
	 * one more lump of height.
	 */
    png->image_len = (px_h * png->width_px / 4) + px_h;
    png->height_px += px_h;
    png->image_height_px = px_h;
    png->ihdr.height = __builtin_bswap32(png->height_px);

    png->idat_image.len = png->image_len;
    png->idat_image.nlen = ~(png->image_len);
    png->idat_image.data_len = __builtin_bswap32(png->image_len + 5); // 5 is zlib header size

    /* Set this DEFLATE stream to the FINAL */
    png->idat_image.deflate_btype = 1;

    /* Calculate CRCs */
    /* +4 is because LEN doesn't include type bytes, but CRC does */
    png->ihdr.crc =
        __builtin_bswap32(crc(png->ihdr.type, __builtin_bswap32(png->ihdr.data_len) + 4));
    /* IDAT not calculated here, since it would need to be recalculated again
	 * after the chunk is written later.
	 */
}

void png_deflate_unfinal(void* png_handle) {
    struct png_handle* png = png_handle;
    uint32_t i;

    png->idat_image.deflate_btype = 0;

    /* Calculate CRCs */
    /* +4 is because LEN doesn't include type bytes, but CRC does */
    /* The crc is stored as part of the image data since its a variable
	 * length argument.
	 */
    i = __builtin_bswap32(
        crc(png->idat_image.type, __builtin_bswap32(png->idat_image.data_len) + 4));
    memcpy(&png->idat_image.image[png->image_len], &i, sizeof(uint32_t));
}

void png_palette_set(void* png_handle, uint8_t rgb[4][3]) {
    struct png_handle* png = png_handle;
    memcpy(&png->plte.color, rgb, 12); // This is a constant size
    /* Calculate CRCs */
    /* +4 is because LEN doesn't include type bytes, but CRC does */
    png->plte.crc =
        __builtin_bswap32(crc(png->plte.type, __builtin_bswap32(png->plte.data_len) + 4));
}

void png_dat_write(void* png_handle, uint8_t* image_buf) {
    struct png_handle* png = png_handle;
    uint8_t* image_ptr = png->idat_image.image;
    uint32_t i;

    for(i = 0; i < png->image_height_px; i++) {
        /* Each row needs to begin with a 0 byte to indicate no filter
		 * for that row.
		 */
        *image_ptr = 0x00;
        image_ptr++;
        memcpy(image_ptr, image_buf, 40); // The magic number is that each row is 40 bytes long
        image_ptr += 40;
        image_buf += 40;
    }

    /* Calculate zlib adler32 */
    image_ptr = png->idat_image.image;
    for(i = 0; i < png->image_len; i++) {
        png->adler_a = (png->adler_a + image_ptr[i]) % 65521;
        png->adler_b = (png->adler_b + png->adler_a) % 65521;
    }
    png->idat_check.check_data = (__builtin_bswap32((png->adler_b << 16) | png->adler_a));

    /* Calculate CRCs */
    /* +4 is because LEN doesn't include type bytes, but CRC does */
    i = __builtin_bswap32(
        crc(png->idat_image.type, __builtin_bswap32(png->idat_image.data_len) + 4));
    memcpy(&png->idat_image.image[png->image_len], &i, sizeof(uint32_t));
    png->idat_check.crc = __builtin_bswap32(
        crc(png->idat_check.type, __builtin_bswap32(png->idat_check.data_len) + 4));
}

void png_reset(void* png_handle, size_t px_w, size_t px_h) {
    struct png_handle* png = png_handle;

    /* Clear each PNG chunk */
    memset(&png->ihdr, '\0', sizeof(struct ihdr));
    memset(&png->plte, '\0', sizeof(struct plte));
    memset(&png->idat_zlib, '\0', sizeof(struct idat_zlib));
    memset(&png->idat_check, '\0', sizeof(struct idat_check));
    memset(&png->iend, '\0', sizeof(struct iend));
    memset(&png->idat_image, '\0', (sizeof(struct idat_image) + png->image_len_max));

    /* Recalculate sizes, must be smaller than max allocated */
    png->height_px = px_h;
    png->image_height_px = px_h;
    png->width_px = px_w;
    /* Since image data is 4 px per bit, the image length is (height*width/4),
	 * and, PNG has a byte starting each "scanline" (each row), so add in
	 * one more lump of height.
	 */
    png->image_len = (px_h * px_w / 4) + px_h;
    furi_check((png->image_len + sizeof(uint32_t)) <= png->image_len_max);

    /* Copy in static data bits */
    memcpy(&png->ihdr, &ihdr_data, sizeof(struct ihdr));
    memcpy(&png->plte, &plte_data, sizeof(struct plte));
    png_palette_set(png_handle, palette_rgb16_get(0));
    memcpy(&png->idat_zlib, &idat_zlib_data, sizeof(struct idat_zlib));
    memcpy(&png->idat_check, &idat_check_data, sizeof(struct idat_check));
    memcpy(&png->iend, &iend_data, sizeof(struct iend));
    memcpy(&png->idat_image, &idat_data, sizeof(struct idat_image));

    /* Start adding in dynamic data */
    png->ihdr.width = __builtin_bswap32(png->width_px);
    png->ihdr.height = __builtin_bswap32(png->height_px);
    /* len and nlen are components of the deflate stream inside zlib */
    png->idat_image.len = png->image_len;
    png->idat_image.nlen = ~(png->image_len);
    png->idat_image.data_len = __builtin_bswap32(png->image_len + 5); // 5 is zlib header size

    /* Reset adler32 running */
    png->adler_a = 1;
    png->adler_b = 0;

    /* Calculate CRCs */
    /* +4 is because LEN doesn't include type bytes, but CRC does */
    png->ihdr.crc =
        __builtin_bswap32(crc(png->ihdr.type, __builtin_bswap32(png->ihdr.data_len) + 4));
}

/* Allocates pixel array */
void* png_alloc(uint32_t width, uint32_t height) {
    struct png_handle* png = NULL;
    uint32_t image_len;

    /* The IDAT chunk has variable length data, and we calculate that
	 * amount based on width and height.
	 *
	 * Since image data is 4 px per bit, the image length is (height*width/4),
	 * and, PNG has a byte starting each "scanline" (each row), so add in
	 * one more lump of height.
	 */
    image_len = (height * width / 4) + height;

    /* Allocate the data we will need. The whole png_handle, plus the length
	 * of the image data, plus the image data's crc32 which is considered a
	 * part of the data stream for our purposes.
	 */
    png = malloc(sizeof(struct png_handle) + image_len + sizeof(uint32_t));

    png->image_len = image_len;
    png->image_len_max = (image_len + sizeof(uint32_t));

    png_reset(png, width, height);

    return png;
}

void png_free(void* png_handle) {
    struct png_handle* png = png_handle;
    free(png);
}

size_t png_len_get(void* png_handle, enum png_chunks chunk) {
    struct png_handle* png = png_handle;

    switch(chunk) {
    case IHDR:
        return sizeof(struct ihdr);
    case PLTE:
        return sizeof(struct plte);
    case IDAT_ZLIB:
        return sizeof(struct idat_zlib);
    case IDAT:
        return sizeof(struct idat_image) + png->image_len +
               4; // +4 is for the CRC appended to the end.
    case IDAT_CHECK:
        return sizeof(struct idat_check);
    case IEND:
        return 12; // Always 12 bytes
    case LAST_IDAT:
        return (
            12 + sizeof(struct idat_check) + sizeof(struct idat_image) + png->image_len +
            4); // 12 is IEND, 4 is CRC to IDAT
    default:
        return 0;
    }
}

uint8_t* png_buf_get(void* png_handle, enum png_chunks chunk) {
    struct png_handle* png = png_handle;

    switch(chunk) {
    case IHDR:
        return (uint8_t*)&png->ihdr;
    case PLTE:
        return (uint8_t*)&png->plte;
    case IDAT_ZLIB:
        return (uint8_t*)&png->idat_zlib;
    case IDAT:
        return (uint8_t*)&png->idat_image;
    case IDAT_CHECK:
        return (uint8_t*)&png->idat_check;
    case IEND:
        return (uint8_t*)&png->iend;
    // LAST_IDAT doesn't have any meaning as a buffer
    default:
        return NULL;
    }
}

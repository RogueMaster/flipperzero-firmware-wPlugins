/*
 * BMP writer for plugin-rendered images.
 *
 * The ESP streams the canvas top-down (because that's what we draw into),
 * but the BMP file format stores rows bottom-up. We buffer the pixel
 * section in memory, then write it out reversed when the stream closes.
 *
 * Bit convention: bit==0 -> palette[0] (black), bit==1 -> palette[1] (white).
 * That matches the TagTinker canvas convention exactly so no inversion is
 * needed on the byte level.
 */
#include "tagtinker_wifi_bmp.h"

#include <furi.h>
#include <stdlib.h>
#include <string.h>

#define BMP_FILE_HDR  14U
#define BMP_DIB_HDR   40U
#define BMP_PALETTE_2 8U /* mono: 2 entries * 4 bytes BGRA */
#define BMP_PALETTE_3 12U /* tri:  3 entries (white / black / accent) */

static void put_le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void put_le32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

bool tagtinker_wifi_bmp_open(
    TagTinkerWifiBmpWriter* w,
    uint16_t width,
    uint16_t height,
    uint8_t planes,
    uint8_t accent_r,
    uint8_t accent_g,
    uint8_t accent_b) {
    memset(w, 0, sizeof(*w));
    w->width = width;
    w->height = height;
    w->planes = (planes >= 2) ? 2 : 1;
    w->accent_r = accent_r ? accent_r : 0xE0;
    w->accent_g = accent_g;
    w->accent_b = accent_b;
    w->row_stride = (uint16_t)(((width + 31U) / 32U) * 4U);
    w->plane_size = (size_t)w->row_stride * height;
    w->pixel_size = w->plane_size * w->planes;

    w->pixel_buf = malloc(w->pixel_size);
    if(!w->pixel_buf) return false;
    /* Default to all-zero (==white under our palette) so partial writes
     * don't show garbage. */
    memset(w->pixel_buf, 0x00, w->pixel_size);

    w->storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(w->storage, "/ext/apps_data/tagtinker");
    w->file = storage_file_alloc(w->storage);
    if(!storage_file_open(w->file, TAGTINKER_WIFI_TMP_BMP, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(w->file);
        w->file = NULL;
        furi_record_close(RECORD_STORAGE);
        w->storage = NULL;
        free(w->pixel_buf);
        w->pixel_buf = NULL;
        return false;
    }
    return true;
}

bool tagtinker_wifi_bmp_chunk(TagTinkerWifiBmpWriter* w, const uint8_t* data, size_t len) {
    if(!w->pixel_buf) return false;
    /* Append at the running byte offset. Tracking rows here would silently
     * corrupt partial rows because chunks from the worker aren't aligned
     * to row_stride. The worker emits planes back-to-back (plane 0 then
     * plane 1), and pixel_size already accounts for all planes. */
    size_t off = (size_t)w->bytes_written;
    if(off >= w->pixel_size) return true; /* overflow - drop */
    size_t remain = w->pixel_size - off;
    size_t take = (len < remain) ? len : remain;
    memcpy(w->pixel_buf + off, data, take);
    w->bytes_written += (uint32_t)take;
    return true;
}

bool tagtinker_wifi_bmp_close(TagTinkerWifiBmpWriter* w) {
    if(!w->file) return false;

    const uint16_t pal_bytes = (w->planes == 2) ? BMP_PALETTE_3 : BMP_PALETTE_2;
    const uint16_t hdr_total = BMP_FILE_HDR + BMP_DIB_HDR + pal_bytes;
    const uint32_t pixel_section = (uint32_t)w->pixel_size;
    const uint32_t total_size = hdr_total + pixel_section;

    /* The web-image-prep convention (which the rest of the FAP TX pipeline
     * already detects via info.bpp == 2 in tx_bmp_open) is:
     *
     *   biPlanes   = 1  (BMP standard requires this)
     *   biBitCount = 2  for 2-plane accent, 1 for plain mono
     *   palette has 2 (mono) or 3 (white/black/accent) entries
     *   pixel data is 2 stacked 1bpp planes when biBitCount==2
     *
     * The streaming TX reader uses biBitCount as the "accent in this BMP?"
     * flag, not biPlanes - so we have to write it that way too. */

    /* --- File + DIB headers ----------------------------------------- */
    uint8_t hdr[BMP_FILE_HDR + BMP_DIB_HDR + BMP_PALETTE_3] = {0};
    /* BITMAPFILEHEADER */
    hdr[0] = 'B';
    hdr[1] = 'M';
    put_le32(&hdr[2], total_size);
    put_le32(&hdr[10], hdr_total);

    /* BITMAPINFOHEADER */
    put_le32(&hdr[14], BMP_DIB_HDR);
    put_le32(&hdr[18], (uint32_t)w->width);
    put_le32(&hdr[22], (uint32_t)w->height); /* positive = bottom-up */
    put_le16(&hdr[26], 1); /* biPlanes (BMP req) */
    put_le16(&hdr[28], (uint16_t)w->planes); /* biBitCount: 1=mono, 2=accent */
    put_le32(&hdr[30], 0); /* BI_RGB */
    put_le32(&hdr[34], pixel_section);
    put_le32(&hdr[38], 2835); /* 72 DPI */
    put_le32(&hdr[42], 2835);
    put_le32(&hdr[46], (uint32_t)(w->planes == 2 ? 3 : 2)); /* colors used */
    put_le32(&hdr[50], 0); /* important colors */

    /* Palette (BGRA per entry). */
    hdr[54] = 0xFF;
    hdr[55] = 0xFF;
    hdr[56] = 0xFF;
    hdr[57] = 0x00; /* white */
    hdr[58] = 0x00;
    hdr[59] = 0x00;
    hdr[60] = 0x00;
    hdr[61] = 0x00; /* black */
    if(w->planes == 2) {
        hdr[62] = w->accent_b;
        hdr[63] = w->accent_g;
        hdr[64] = w->accent_r;
        hdr[65] = 0x00; /* accent */
    }

    if(storage_file_write(w->file, hdr, hdr_total) != hdr_total) goto fail;

    /* --- Flip rows bottom-up per plane and write ------------------- */
    for(uint8_t pl = 0; pl < w->planes; pl++) {
        const uint8_t* plane_base = w->pixel_buf + pl * w->plane_size;
        for(int32_t row = (int32_t)w->height - 1; row >= 0; row--) {
            const uint8_t* src = plane_base + (size_t)row * w->row_stride;
            if(storage_file_write(w->file, src, w->row_stride) != w->row_stride) goto fail;
        }
    }

    storage_file_close(w->file);
    storage_file_free(w->file);
    furi_record_close(RECORD_STORAGE);
    free(w->pixel_buf);
    memset(w, 0, sizeof(*w));
    return true;

fail:
    tagtinker_wifi_bmp_abort(w);
    return false;
}

void tagtinker_wifi_bmp_abort(TagTinkerWifiBmpWriter* w) {
    if(w->file) {
        storage_file_close(w->file);
        storage_file_free(w->file);
    }
    if(w->storage) furi_record_close(RECORD_STORAGE);
    free(w->pixel_buf);
    memset(w, 0, sizeof(*w));
}

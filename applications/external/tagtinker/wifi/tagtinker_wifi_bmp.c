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
#define BMP_PALETTE   8U
#define BMP_HDR_TOTAL (BMP_FILE_HDR + BMP_DIB_HDR + BMP_PALETTE)

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

bool tagtinker_wifi_bmp_open(TagTinkerWifiBmpWriter* w, uint16_t width, uint16_t height) {
    memset(w, 0, sizeof(*w));
    w->width = width;
    w->height = height;
    w->row_stride = (uint16_t)(((width + 31U) / 32U) * 4U);
    w->pixel_size = (size_t)w->row_stride * height;

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
     * to row_stride. Plane 1 (if any) is appended after plane 0; we simply
     * stop accepting bytes once plane 0 is full. */
    size_t off = (size_t)w->bytes_written;
    if(off >= w->pixel_size) return true; /* plane 1 / overflow - drop */
    size_t remain = w->pixel_size - off;
    size_t take = (len < remain) ? len : remain;
    memcpy(w->pixel_buf + off, data, take);
    w->bytes_written += (uint32_t)take;
    return true;
}

bool tagtinker_wifi_bmp_close(TagTinkerWifiBmpWriter* w) {
    if(!w->file) return false;

    uint32_t pixel_section = (uint32_t)w->pixel_size;
    uint32_t total_size = BMP_HDR_TOTAL + pixel_section;

    /* --- File + DIB headers ----------------------------------------- */
    uint8_t hdr[BMP_HDR_TOTAL] = {0};
    /* BITMAPFILEHEADER */
    hdr[0] = 'B';
    hdr[1] = 'M';
    put_le32(&hdr[2], total_size);
    put_le32(&hdr[10], BMP_HDR_TOTAL);

    /* BITMAPINFOHEADER */
    put_le32(&hdr[14], BMP_DIB_HDR);
    put_le32(&hdr[18], (uint32_t)w->width);
    put_le32(&hdr[22], (uint32_t)w->height); /* positive = bottom-up */
    put_le16(&hdr[26], 1); /* planes */
    put_le16(&hdr[28], 1); /* bpp */
    put_le32(&hdr[30], 0); /* BI_RGB */
    put_le32(&hdr[34], pixel_section);
    put_le32(&hdr[38], 2835); /* 72 DPI */
    put_le32(&hdr[42], 2835);
    put_le32(&hdr[46], 2); /* colors used */
    put_le32(&hdr[50], 0); /* important colors */

    /* Palette (BGRA): index 0 = white, index 1 = black.
     * Matches the convention used by the web image prep tool and the rest
     * of the TagTinker TX pipeline (bit value 1 = ink on / black pixel). */
    hdr[54] = 0xFF;
    hdr[55] = 0xFF;
    hdr[56] = 0xFF;
    hdr[57] = 0x00;
    hdr[58] = 0x00;
    hdr[59] = 0x00;
    hdr[60] = 0x00;
    hdr[61] = 0x00;

    if(storage_file_write(w->file, hdr, sizeof(hdr)) != sizeof(hdr)) goto fail;

    /* --- Flip rows bottom-up and write ------------------------------ */
    for(int32_t row = (int32_t)w->height - 1; row >= 0; row--) {
        const uint8_t* src = w->pixel_buf + (size_t)row * w->row_stride;
        if(storage_file_write(w->file, src, w->row_stride) != w->row_stride) goto fail;
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

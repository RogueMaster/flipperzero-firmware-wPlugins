/*
 * Stream the 1bpp planes returned by the ESP into a Windows-format BMP file
 * on the SD card so the existing TX pipeline can read it back without
 * needing to know that the image came from a WiFi plugin.
 */
#ifndef TAGTINKER_WIFI_BMP_H
#define TAGTINKER_WIFI_BMP_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <storage/storage.h>

/* Path used for plugin-rendered BMPs. The same file is overwritten every
 * time a plugin runs; previous output is discarded. */
#define TAGTINKER_WIFI_TMP_BMP "/ext/apps_data/tagtinker/wifi_temp.bmp"

typedef struct {
    File* file;
    Storage* storage;
    uint16_t width;
    uint16_t height;
    uint8_t planes; /* 1 = mono, 2 = mono + accent (matches web-image-prep BMPs) */
    uint8_t accent_r;
    uint8_t accent_g;
    uint8_t accent_b;
    uint16_t row_stride; /* bytes per row, padded to 4 (matches canvas) */
    uint32_t bytes_written; /* running offset into pixel_buf */
    /* The Flipper file system can't seek-write efficiently, and BMP rows are
     * stored bottom-up - so we buffer the whole pixel section in memory then
     * flip on close(). When planes==2, plane 0 (mono) and plane 1 (accent)
     * are concatenated in the buffer in worker order. */
    uint8_t* pixel_buf;
    size_t pixel_size; /* total bytes for ALL planes */
    size_t plane_size; /* bytes per plane */
} TagTinkerWifiBmpWriter;

/* `planes` matches the worker's RESULT_BEGIN plane count (1 or 2).
 * `accent_rgb` is sampled into the BMP palette[2] when planes==2; ignored
 * otherwise. Pass 0 to use a default red. */
bool tagtinker_wifi_bmp_open(
    TagTinkerWifiBmpWriter* w,
    uint16_t width,
    uint16_t height,
    uint8_t planes,
    uint8_t accent_r,
    uint8_t accent_g,
    uint8_t accent_b);
bool tagtinker_wifi_bmp_chunk(TagTinkerWifiBmpWriter* w, const uint8_t* data, size_t len);
bool tagtinker_wifi_bmp_close(TagTinkerWifiBmpWriter* w);
void tagtinker_wifi_bmp_abort(TagTinkerWifiBmpWriter* w);

#endif

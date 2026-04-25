/**
 * Transmit Scene
 * Displays cool animations while transmitting in a background thread.
 */

#include "../tagtinker_app.h"
#include "../views/tagtinker_font.h"
#include <furi_hal.h>
#include <storage/storage.h>

typedef struct {
    TagTinkerApp* app;
    uint32_t tick;
    bool completed;
    bool ok;
} TxViewModel;

typedef struct {
    uint32_t data_offset;
    uint32_t row_stride;
    uint16_t width;
    uint16_t height;
    uint16_t bpp;
    bool top_down;
} TxBmpInfo;

/* Full-job threshold: if the whole image fits in memory, render it in one
 * shot instead of streaming in chunks. 49152 covers 208×112×2 = 46592
 * pixels (the most common color DM images). */
#define TX_FULL_JOB_PIXEL_LIMIT 49152U

static uint16_t tx_pick_chunk_height(uint16_t width, uint16_t height, bool second_plane);

static void tx_debug_log(const char* fmt, ...) {
    UNUSED(fmt);
}

static uint16_t tx_apply_signal_mode(const TagTinkerApp* app, uint16_t repeats) {
    UNUSED(app);
    return repeats & 0x7FFFU;
}

static TagTinkerTagColor tx_target_color(const TagTinkerApp* app) {
    if(app->selected_target < 0 || app->selected_target >= app->target_count) {
        return TagTinkerTagColorMono;
    }

    return app->targets[app->selected_target].profile.color;
}

static bool tx_send_frame(TagTinkerApp* app, const uint8_t* frame, size_t len, uint16_t repeats) {
    if(!app->tx_active) return false;
    return tagtinker_ir_transmit(frame, len, tx_apply_signal_mode(app, repeats), 1);
}

static bool tx_send_ping(TagTinkerApp* app, const uint8_t plid[4]) {
    uint8_t frame[TAGTINKER_MAX_FRAME_SIZE];
    size_t len = tagtinker_make_ping_frame(frame, plid);
    return tx_send_frame(app, frame, len, 80);
}

static bool tx_send_refresh(TagTinkerApp* app, const uint8_t plid[4]) {
    uint8_t frame[TAGTINKER_MAX_FRAME_SIZE];
    size_t len = tagtinker_make_refresh_frame(frame, plid);
    return tx_send_frame(app, frame, len, 20);
}

static bool tx_should_send_full_job(uint16_t width, uint16_t height, bool second_plane) {
    size_t pixel_count = (size_t)width * height;
    if(second_plane) pixel_count *= 2U;
    return pixel_count <= TX_FULL_JOB_PIXEL_LIMIT;
}

static bool tx_send_image_start(
    TagTinkerApp* app,
    const uint8_t plid[4],
    uint16_t byte_count,
    uint8_t comp_type,
    uint8_t page,
    uint16_t width,
    uint16_t height,
    uint16_t pos_x,
    uint16_t pos_y) {
    uint8_t frame[TAGTINKER_MAX_FRAME_SIZE];
    size_t len = tagtinker_make_image_param_frame(
        frame, plid, byte_count, comp_type, page, width, height, pos_x, pos_y);
    return tx_send_frame(app, frame, len, 15);
}

static bool tx_send_payload_frames(
    TagTinkerApp* app,
    const uint8_t plid[4],
    const TagTinkerImagePayload* payload,
    uint8_t page,
    uint16_t width,
    uint16_t height,
    uint16_t pos_x,
    uint16_t pos_y) {
    uint8_t frame[TAGTINKER_MAX_FRAME_SIZE];
    bool ok = tx_send_image_start(
        app,
        plid,
        (uint16_t)payload->byte_count,
        payload->comp_type,
        page,
        width,
        height,
        pos_x,
        pos_y);
    if(ok) furi_delay_ms(50);

    size_t frame_count = payload->byte_count / TAGTINKER_IMAGE_DATA_BYTES_PER_FRAME;
    for(size_t i = 0; ok && i < frame_count; i++) {
        size_t len = tagtinker_make_image_data_frame(
            frame, plid, (uint16_t)i, &payload->data[i * TAGTINKER_IMAGE_DATA_BYTES_PER_FRAME]);
        ok = tx_send_frame(app, frame, len, app->data_frame_repeats);
        /* Short delay to avoid tag overflow */
        if(ok && ((i + 1U) % 32U) == 0U && (i + 1U) < frame_count) {
            furi_delay_ms(1);
        }
    }

    return ok;
}

static bool tx_send_image_chunk(
    TagTinkerApp* app,
    const uint8_t plid[4],
    const uint8_t* primary_pixels,
    const uint8_t* secondary_pixels,
    uint16_t width,
    uint16_t height,
    uint8_t page,
    uint16_t pos_x,
    uint16_t pos_y) {
    TagTinkerImagePayload payload;
    size_t pixel_count = (size_t)width * height;
    if(!tagtinker_encode_planes_payload(
           primary_pixels, secondary_pixels, pixel_count, app->compression_mode, &payload)) {
        return false;
    }

    bool ok = tx_send_payload_frames(app, plid, &payload, page, width, height, pos_x, pos_y);

    tagtinker_free_image_payload(&payload);
    return ok;
}

static uint32_t tx_chunk_settle_delay_ms(uint16_t width, uint16_t height, bool color_clear) {
    /* Tag needs time to process each chunk before accepting the next one.
     * Keep this as short as possible while remaining reliable. */
    UNUSED(color_clear);
    size_t work_pixels = (size_t)width * height;
    uint32_t delay_ms = 500U + (uint32_t)(work_pixels / 20U);
    if(delay_ms < 800U) delay_ms = 800U;
    if(delay_ms > 2000U) delay_ms = 2000U;
    return delay_ms;
}

static uint16_t tx_pick_chunk_height(uint16_t width, uint16_t height, bool second_plane) {
    UNUSED(second_plane);
    if(width == 0U || height == 0U) return 1U;

    /*
     * Keep each plane buffer ≤ 8 KB so two planes + encode overhead fits
     * in the Flipper's heap even right after BLE teardown.
     * 8 KB (down from 12 KB) leaves extra headroom for the encoder's
     * internal allocations and any lingering BLE buffers.
     */
    size_t per_plane_budget = 8192U; /* 8 KB */
    uint16_t chunk_h = (uint16_t)(per_plane_budget / width);
    if(chunk_h == 0U) chunk_h = 1U;
    if(chunk_h > height) chunk_h = height;

    /* Round down to 8-row boundary for alignment, but keep at least 1 */
    if(chunk_h >= 16U) {
        chunk_h = (uint16_t)(chunk_h & ~7U);
        if(chunk_h == 0U) chunk_h = 8U;
    }

    if(chunk_h > height) chunk_h = height;
    if(chunk_h == 0U) chunk_h = 1U;
    return chunk_h;
}

static bool tx_send_full_payload(
    TagTinkerApp* app,
    const uint8_t plid[4],
    const TagTinkerImagePayload* payload,
    uint8_t page,
    uint16_t width,
    uint16_t height,
    uint16_t pos_x,
    uint16_t pos_y) {
    bool ok = tx_send_ping(app, plid);
    if(ok) furi_delay_ms(50);
    if(ok) ok = tx_send_payload_frames(app, plid, payload, page, width, height, pos_x, pos_y);
    if(ok) furi_delay_ms(50);
    if(ok) ok = tx_send_refresh(app, plid);
    return ok;
}

static bool tx_send_full_text_image(TagTinkerApp* app) {
    const TagTinkerImageTxJob* job = &app->image_tx_job;
    const TagTinkerTarget* target =
        (app->selected_target >= 0) ? &app->targets[app->selected_target] : NULL;
    bool accent_capable = tagtinker_target_supports_accent(target);
    bool accent_text = accent_capable && app->color_clear;
    bool use_second_plane = accent_capable || app->color_clear;
    size_t pixel_count = (size_t)job->width * job->height;
    TagTinkerTagColor accent_color = tx_target_color(app);

    if(!tx_should_send_full_job(job->width, job->height, use_second_plane)) {
        return false;
    }

    uint8_t* primary = malloc(pixel_count);
    uint8_t* secondary = use_second_plane ? malloc(pixel_count) : NULL;
    if(!primary || (use_second_plane && !secondary)) {
        free(primary);
        free(secondary);
        return false;
    }

    if(accent_text) {
        uint8_t bg_primary = app->invert_text ? 0 : 1;
        uint8_t fg_primary = (accent_color == TagTinkerTagColorYellow) ? 0 : 1;
        render_text_ex(
            primary,
            job->width,
            job->height,
            app->text_input_buf,
            bg_primary,
            fg_primary,
            app->text_padding_pct);
        render_text_ex(
            secondary, job->width, job->height, app->text_input_buf, 1, 0, app->text_padding_pct);
    } else {
        render_text_ex(
            primary,
            job->width,
            job->height,
            app->text_input_buf,
            app->invert_text ? 0 : 1,
            app->invert_text ? 1 : 0,
            app->text_padding_pct);
        if(secondary) memset(secondary, 1, pixel_count);
    }

    TagTinkerImagePayload payload;
    bool ok = tagtinker_encode_planes_payload(
        primary, secondary, pixel_count, app->compression_mode, &payload);
    free(primary);
    free(secondary);

    if(!ok) return false;

    ok = tx_send_full_payload(
        app, job->plid, &payload, job->page, job->width, job->height, job->pos_x, job->pos_y);
    tagtinker_free_image_payload(&payload);
    return ok;
}

static bool tx_stream_text_image(TagTinkerApp* app) {
    if(tx_send_full_text_image(app)) {
        return true;
    }

    const TagTinkerImageTxJob* job = &app->image_tx_job;
    const TagTinkerTarget* target =
        (app->selected_target >= 0) ? &app->targets[app->selected_target] : NULL;
    bool accent_capable = tagtinker_target_supports_accent(target);
    bool accent_text = accent_capable && app->color_clear;
    bool use_second_plane = accent_capable || app->color_clear;
    TagTinkerTagColor accent_color = tx_target_color(app);
    uint16_t chunk_h = tx_pick_chunk_height(job->width, job->height, use_second_plane);
    uint8_t* primary = malloc((size_t)job->width * chunk_h);
    uint8_t* secondary = use_second_plane ? malloc((size_t)job->width * chunk_h) : NULL;
    if(!primary || (use_second_plane && !secondary)) {
        free(primary);
        free(secondary);
        return false;
    }

    bool ok = true;

    for(uint16_t y = 0; ok && y < job->height; y = (uint16_t)(y + chunk_h)) {
        uint16_t actual_h = job->height - y;
        if(actual_h > chunk_h) actual_h = chunk_h;

        if(accent_text) {
            uint8_t bg_primary = app->invert_text ? 0 : 1;
            uint8_t fg_primary = (accent_color == TagTinkerTagColorYellow) ? 0 : 1;
            render_text_region_ex(
                primary,
                job->width,
                job->height,
                y,
                actual_h,
                app->text_input_buf,
                bg_primary,
                fg_primary,
                app->text_padding_pct);
            render_text_region_ex(
                secondary,
                job->width,
                job->height,
                y,
                actual_h,
                app->text_input_buf,
                1,
                0,
                app->text_padding_pct);
        } else {
            render_text_region_ex(
                primary,
                job->width,
                job->height,
                y,
                actual_h,
                app->text_input_buf,
                app->invert_text ? 0 : 1,
                app->invert_text ? 1 : 0,
                app->text_padding_pct);

            if(secondary) memset(secondary, 1, (size_t)job->width * actual_h);
        }

        ok = tx_send_ping(app, job->plid);
        if(ok) furi_delay_ms(10);
        ok = tx_send_image_chunk(
            app,
            job->plid,
            primary,
            secondary,
            job->width,
            actual_h,
            job->page,
            job->pos_x,
            (uint16_t)(job->pos_y + y));
        if(ok) furi_delay_ms(10);
        if(ok) ok = tx_send_refresh(app, job->plid);

        if(ok && (uint16_t)(y + actual_h) < job->height) {
            furi_delay_ms(tx_chunk_settle_delay_ms(job->width, actual_h, use_second_plane));
        }
    }

    free(primary);
    free(secondary);
    return ok;
}

static bool tx_bmp_open(const char* path, File* file, TxBmpInfo* info) {
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) return false;

    uint8_t header[54];
    if(storage_file_read(file, header, sizeof(header)) != sizeof(header)) return false;
    if(header[0] != 'B' || header[1] != 'M') return false;

    uint16_t bpp = header[28] | (header[29] << 8);
    if(!(bpp == 1 || bpp == 2 || bpp == 24 || bpp == 32)) return false;
    info->bpp = bpp;

    int32_t bmp_h = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
    info->width =
        (uint16_t)(header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24));
    info->top_down = false;
    if(bmp_h < 0) {
        info->top_down = true;
        bmp_h = -bmp_h;
    }

    info->height = (uint16_t)bmp_h;
    info->data_offset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
    if(info->bpp == 1 || info->bpp == 2) {
        info->row_stride = ((info->width + 31U) / 32U) * 4U;
    } else if(info->bpp == 24) {
        info->row_stride = ((info->width * 3U) + 3U) & ~3U;
    } else {
        info->row_stride = info->width * 4U;
    }
    return true;
}

/*
 * Zero-allocation streaming BMP transmitter with RLE compression.
 *
 * Two-pass approach:
 *   Pass 1: Read all pixels from BMP, count RLE compressed bit length.
 *   Pass 2: Re-read pixels, RLE-encode on-the-fly into IR data frames.
 *
 * Total stack usage: ~200 bytes.  Zero heap allocation.
 *
 * Flow: PING -> PARAM (full image dims, RLE) -> DATA frames (streamed) -> REFRESH
 */

/* Elias gamma bit length for a run count */
static inline size_t rle_run_bits(uint32_t count) {
    uint8_t n = 0;
    uint32_t v = count;
    while(v) {
        n++;
        v >>= 1;
    }
    return (size_t)(n * 2U) - 1U;
}

static inline uint8_t bmp_read_pixel(const uint8_t* row_buf, uint16_t x) {
    uint8_t byte = row_buf[x / 8U];
    uint8_t bit = (byte >> (7U - (x % 8U))) & 1U;
    return bit ? 0U : 1U;
}

static inline bool bmp_read_row(
    File* file,
    const TxBmpInfo* info,
    uint16_t y,
    uint16_t tx_height,
    uint16_t plane_offset,
    uint8_t* row_buf) {
    uint16_t src_row = info->top_down ? y : (uint16_t)(tx_height - 1U - y);
    uint32_t off =
        info->data_offset + ((uint32_t)src_row + (uint32_t)plane_offset) * info->row_stride;
    storage_file_seek(file, off, true);
    return storage_file_read(file, row_buf, info->row_stride) == info->row_stride;
}

static bool tx_stream_bmp_image(TagTinkerApp* app) {
    const TagTinkerImageTxJob* job = &app->image_tx_job;
    tx_debug_log(
        "BMP TX: path=%s w=%u h=%u page=%u", job->image_path, job->width, job->height, job->page);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    TxBmpInfo info = {0};
    bool ok = tx_bmp_open(job->image_path, file, &info);
    tx_debug_log(
        "bmp_open=%d bmp_w=%u bmp_h=%u bpp=%u off=%lu",
        ok,
        info.width,
        info.height,
        info.bpp,
        (unsigned long)info.data_offset);

    if(!ok) {
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    uint16_t tx_width = info.width;
    uint16_t tx_height = info.height;

    TagTinkerTagColor accent_color = tx_target_color(app);
    bool accent_capable = accent_color == TagTinkerTagColorRed ||
                          accent_color == TagTinkerTagColorYellow;
    bool use_second_plane = app->color_clear || accent_capable;
    bool has_secondary_in_bmp = (info.bpp == 2);
    UNUSED(accent_color);

    uint8_t row_buf[64];

    /* ---- PASS 1: Count RLE compressed bit length ---- */
    size_t rle_bits = 0;
    uint8_t run_pixel = 0;
    uint32_t run_count = 0;
    bool first = true;

    for(uint16_t y = 0; ok && y < tx_height; y++) {
        if(!bmp_read_row(file, &info, y, tx_height, 0, row_buf)) {
            ok = false;
            break;
        }
        for(uint16_t x = 0; x < tx_width; x++) {
            uint8_t pix = bmp_read_pixel(row_buf, x);
            if(first) {
                rle_bits = 1;
                run_pixel = pix;
                run_count = 1;
                first = false;
            } else if(pix == run_pixel) {
                run_count++;
            } else {
                rle_bits += rle_run_bits(run_count);
                run_pixel = pix;
                run_count = 1;
            }
        }
    }
    if(ok && use_second_plane) {
        if(has_secondary_in_bmp) {
            for(uint16_t y = 0; ok && y < tx_height; y++) {
                if(!bmp_read_row(file, &info, y, tx_height, tx_height, row_buf)) {
                    ok = false;
                    break;
                }
                for(uint16_t x = 0; x < tx_width; x++) {
                    uint8_t pix = bmp_read_pixel(row_buf, x);
                    if(first) {
                        rle_bits = 1;
                        run_pixel = pix;
                        run_count = 1;
                        first = false;
                    } else if(pix == run_pixel) {
                        run_count++;
                    } else {
                        rle_bits += rle_run_bits(run_count);
                        run_pixel = pix;
                        run_count = 1;
                    }
                }
            }
        } else {
            uint32_t count = (uint32_t)tx_width * tx_height;
            if(first) {
                rle_bits = 1;
                run_pixel = 1;
                run_count = count;
                first = false;
            } else if(1U == run_pixel) {
                run_count += count;
            } else {
                rle_bits += rle_run_bits(run_count);
                run_pixel = 1;
                run_count = count;
            }
        }
    }
    if(run_count > 0) rle_bits += rle_run_bits(run_count);

    if(!ok) {
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    tx_debug_log("STREAM RLE: bits=%u", (unsigned)rle_bits);

    uint32_t raw_bits = (uint32_t)tx_width * tx_height;
    if(use_second_plane) raw_bits *= 2U;

    bool use_compressed = (app->compression_mode == TagTinkerCompressionRle) ||
                          (app->compression_mode == TagTinkerCompressionAuto && rle_bits > 0U &&
                           rle_bits < raw_bits);

    uint32_t target_bits = use_compressed ? (uint32_t)rle_bits : raw_bits;
    uint32_t padded_bytes = (target_bits + 7U) / 8U;
    padded_bytes += (TAGTINKER_IMAGE_DATA_BYTES_PER_FRAME -
                     (padded_bytes % TAGTINKER_IMAGE_DATA_BYTES_PER_FRAME)) %
                    TAGTINKER_IMAGE_DATA_BYTES_PER_FRAME;

    uint8_t* encoded = calloc(padded_bytes, 1);
    if(!encoded) {
        tx_debug_log("RLE/RAW encode malloc fail");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    if(use_compressed) {
        /* ---- PASS 2: RLE encode into a small heap buffer ---- */
        size_t enc_bit_pos = 0;

#define ENC_BIT(b)                                                            \
    do {                                                                      \
        if(b) encoded[enc_bit_pos / 8U] |= (1U << (7U - (enc_bit_pos % 8U))); \
        enc_bit_pos++;                                                        \
    } while(0)

#define ENC_RUN(cnt)                         \
    do {                                     \
        uint8_t _bits[32];                   \
        int _n = 0;                          \
        uint32_t _v = (cnt);                 \
        while(_v) {                          \
            _bits[_n++] = _v & 1U;           \
            _v >>= 1;                        \
        }                                    \
        for(int _i = 0; _i < _n / 2; _i++) { \
            uint8_t _t = _bits[_i];          \
            _bits[_i] = _bits[_n - 1 - _i];  \
            _bits[_n - 1 - _i] = _t;         \
        }                                    \
        for(int _i = 1; _i < _n; _i++)       \
            ENC_BIT(0U);                     \
        for(int _i = 0; _i < _n; _i++)       \
            ENC_BIT(_bits[_i]);              \
    } while(0)

        run_pixel = 0;
        run_count = 0;
        first = true;

        for(uint16_t y = 0; ok && y < tx_height; y++) {
            if(!bmp_read_row(file, &info, y, tx_height, 0, row_buf)) {
                ok = false;
                break;
            }
            for(uint16_t x = 0; x < tx_width; x++) {
                uint8_t pix = bmp_read_pixel(row_buf, x);
                if(first) {
                    ENC_BIT(pix);
                    run_pixel = pix;
                    run_count = 1;
                    first = false;
                } else if(pix == run_pixel) {
                    run_count++;
                } else {
                    ENC_RUN(run_count);
                    run_pixel = pix;
                    run_count = 1;
                }
            }
        }
        if(ok && use_second_plane) {
            if(has_secondary_in_bmp) {
                for(uint16_t y = 0; ok && y < tx_height; y++) {
                    if(!bmp_read_row(file, &info, y, tx_height, tx_height, row_buf)) {
                        ok = false;
                        break;
                    }
                    for(uint16_t x = 0; x < tx_width; x++) {
                        uint8_t pix = bmp_read_pixel(row_buf, x);
                        if(first) {
                            ENC_BIT(pix);
                            run_pixel = pix;
                            run_count = 1;
                            first = false;
                        } else if(pix == run_pixel) {
                            run_count++;
                        } else {
                            ENC_RUN(run_count);
                            run_pixel = pix;
                            run_count = 1;
                        }
                    }
                }
            } else {
                uint32_t count = (uint32_t)tx_width * tx_height;
                if(first) {
                    ENC_BIT(1U);
                    run_pixel = 1;
                    run_count = count;
                    first = false;
                } else if(1U == run_pixel) {
                    run_count += count;
                } else {
                    ENC_RUN(run_count);
                    run_pixel = 1;
                    run_count = count;
                }
            }
        }
        if(run_count > 0) {
            ENC_RUN(run_count);
        }

#undef ENC_BIT
#undef ENC_RUN
    } else {
        /* ---- PASS 2: RAW encode into heap buffer ---- */
        size_t bit_idx = 0;
        for(uint16_t y = 0; ok && y < tx_height; y++) {
            if(!bmp_read_row(file, &info, y, tx_height, 0, row_buf)) {
                ok = false;
                break;
            }
            for(uint16_t x = 0; x < tx_width; x++) {
                uint8_t pix = bmp_read_pixel(row_buf, x);
                if(pix != 0) encoded[bit_idx / 8U] |= (1U << (7U - (bit_idx % 8U)));
                bit_idx++;
            }
        }
        if(ok && use_second_plane) {
            if(has_secondary_in_bmp) {
                for(uint16_t y = 0; ok && y < tx_height; y++) {
                    if(!bmp_read_row(file, &info, y, tx_height, tx_height, row_buf)) {
                        ok = false;
                        break;
                    }
                    for(uint16_t x = 0; x < tx_width; x++) {
                        uint8_t pix = bmp_read_pixel(row_buf, x);
                        if(pix != 0) encoded[bit_idx / 8U] |= (1U << (7U - (bit_idx % 8U)));
                        bit_idx++;
                    }
                }
            } else {
                uint32_t count = (uint32_t)tx_width * tx_height;
                for(uint32_t i = 0; i < count; i++) {
                    encoded[bit_idx / 8U] |= (1U << (7U - (bit_idx % 8U)));
                    bit_idx++;
                }
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!ok) {
        free(encoded);
        return false;
    }

    /* Transmit encoded buffer */
    TagTinkerImagePayload payload;
    payload.data = encoded;
    payload.byte_count = padded_bytes;
    payload.comp_type = use_compressed ? 2U : 0U;

    ok = tx_send_full_payload(
        app, job->plid, &payload, job->page, tx_width, tx_height, job->pos_x, job->pos_y);

    free(encoded);
    return ok;
}

static int32_t tx_thread_callback(void* context) {
    TagTinkerApp* app = context;
    bool ok = true;

    /* Boost priority for IR timing */
    furi_thread_set_current_priority(FuriThreadPriorityHighest);

    tagtinker_ir_init();

    /* Let OS settle (especially BLE teardown) before IR blasting */
    if(app->image_tx_job.mode == TagTinkerTxModeBmpImage ||
       app->image_tx_job.mode == TagTinkerTxModeTextImage) {
        furi_delay_ms(500);
    }

    do {
        if(app->image_tx_job.mode == TagTinkerTxModeTextImage) {
            ok = tx_stream_text_image(app);
        } else if(app->image_tx_job.mode == TagTinkerTxModeBmpImage) {
            ok = tx_stream_bmp_image(app);
        } else if(app->frame_seq_count > 0) {
            for(size_t i = 0; i < app->frame_seq_count; i++) {
                if(!app->tx_active) {
                    ok = false;
                    break;
                }
                if(i > 0) furi_delay_ms(20);
                ok = tagtinker_ir_transmit(
                    app->frame_sequence[i],
                    app->frame_lengths[i],
                    tx_apply_signal_mode(app, app->frame_repeats[i]),
                    10);
                if(!ok) break;
            }
        } else {
            ok = tagtinker_ir_transmit(
                app->frame_buf, app->frame_len, tx_apply_signal_mode(app, app->repeats), 10);
        }

        if(app->tx_spam && app->tx_active) {
            furi_delay_ms(50);
        }
    } while(app->tx_spam && app->tx_active);

    app->tx_active = false;
    /* Use event payload to securely pass result instead of querying running thread! */
    view_dispatcher_send_custom_event(app->view_dispatcher, 101 + (ok ? 0 : 1));
    return 0;
}

static void transmit_draw_cb(Canvas* canvas, void* _model) {
    TxViewModel* model = _model;
    TagTinkerApp* app = model->app;
    uint32_t t = model->tick;

    /* Left: Top-Down Detailed Flipper Zero Vector Model */
    int f_x = -15; // Protruding from left screen edge
    int f_y = 10;
    int f_w = 40;
    int f_h = 36;
    /* Main casing */
    canvas_draw_rframe(canvas, f_x, f_y, f_w, f_h, 3);
    /* Outer screen outline */
    canvas_draw_rframe(canvas, f_x + 10, f_y + 4, 20, 16, 2);
    /* Screen inner solid */
    canvas_draw_box(canvas, f_x + 12, f_y + 6, 16, 12);
    /* Flipper Screen highlight glow */
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, f_x + 14, f_y + 8);
    canvas_draw_dot(canvas, f_x + 15, f_y + 8);
    canvas_set_color(canvas, ColorBlack);
    /* D-Pad Matrix */
    canvas_draw_circle(canvas, f_x + 18, f_y + 27, 5); // Outer
    canvas_draw_circle(canvas, f_x + 18, f_y + 27, 2); // Inner button
    /* GPIO / Case Vents */
    for(int i = 0; i < 4; i++) {
        canvas_draw_dot(canvas, f_x + 32, f_y + 8 + i * 3);
        canvas_draw_dot(canvas, f_x + 34, f_y + 8 + i * 3);
    }
    /* IR Blaster window on the right tip */
    int ir_x = f_x + f_w; // 25
    int ir_y = f_y + 10;
    canvas_draw_box(canvas, ir_x, ir_y, 4, 16);

    int center_y = f_y + 18; /* perfectly aligns with tag center */

    if(app->tx_active) {
        /* Aggressive Pulsing Data-Stream Chevrons with bits */
        uint8_t wave_phase = (t * 2) % 15;
        for(int w = 0; w < 2; w++) {
            int r = wave_phase + w * 18;
            if(r > 0 && r < 20) {
                int wave_x = ir_x + 4 + r;
                int wave_h = r / 2 + 3;
                /* 2px thick chevron pointing right */
                canvas_draw_line(canvas, wave_x, center_y, wave_x - wave_h / 2, center_y - wave_h);
                canvas_draw_line(canvas, wave_x, center_y, wave_x - wave_h / 2, center_y + wave_h);
                canvas_draw_line(
                    canvas, wave_x + 1, center_y, wave_x + 1 - wave_h / 2, center_y - wave_h);
                canvas_draw_line(
                    canvas, wave_x + 1, center_y, wave_x + 1 - wave_h / 2, center_y + wave_h);
                /* Data stream particles */
                if((t + w) % 2 == 0) {
                    canvas_draw_dot(canvas, wave_x + 3, center_y - wave_h / 2);
                    canvas_draw_dot(canvas, wave_x - 3, center_y + wave_h / 2 + 2);
                }
            }
        }
    }

    /* Right: Detailed ESL tag model */
    int tag_x = 52;
    int tag_y = 8;
    int tag_w = 75;
    int tag_h = 40;
    /* Outer casing bounding rect */
    canvas_draw_rframe(canvas, tag_x, tag_y, tag_w, tag_h, 2);
    /* Internal 3D bezel shadow */
    canvas_draw_rframe(canvas, tag_x + 1, tag_y + 1, tag_w - 2, tag_h - 2, 1);

    /* True 32-bit encoded barcode representation on the rim */
    int bc_x = tag_x + 3;
    int bc_w = 8;
    int bc_y = tag_y + 4;
    uint8_t bar_pattern[] = {0b10110100, 0b11010010, 0b10011011, 0b01101010};
    for(int i = 0; i < 32; i++) {
        if((bar_pattern[i / 8] >> (7 - (i % 8))) & 1) {
            canvas_draw_line(canvas, bc_x, bc_y + i, bc_x + bc_w - 1, bc_y + i);
        }
    }

    /* Screen display hardware border */
    int scr_x = tag_x + 14;
    int scr_y = tag_y + 3;
    int scr_w = tag_w - 17;
    int scr_h = tag_h - 6;
    canvas_draw_frame(canvas, scr_x - 1, scr_y - 1, scr_w + 2, scr_h + 2);

    /* E-Paper Sweep Logic */
    int cycle = t % 40; /* 2 second loop at 20fps */

    if(app->tx_active) {
        bool show_result = (cycle >= 20);

        if(!show_result) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas, scr_x + scr_w / 2, scr_y + scr_h / 2, AlignCenter, AlignCenter, "UPDATING");
        } else {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas,
                scr_x + scr_w / 2,
                scr_y + scr_h / 2,
                AlignCenter,
                AlignCenter,
                "FLIPPED ;)");
        }

        /* Glitchy Matrix Wipe Transition */
        if(cycle >= 15 && cycle < 20) {
            int wipe_h = ((cycle - 15) * scr_h) / 5;
            canvas_draw_box(canvas, scr_x, scr_y, scr_w, wipe_h);
            /* Algorithmic leading edge burn/glitch */
            for(int xx = 0; xx < scr_w; xx += 2) {
                canvas_draw_dot(canvas, scr_x + xx, scr_y + wipe_h + ((xx + t) % 4));
            }
        } else if(cycle >= 20 && cycle < 25) {
            int wipe_h = ((cycle - 20) * scr_h) / 5;
            canvas_draw_box(canvas, scr_x, scr_y + wipe_h, scr_w, scr_h - wipe_h);
            /* Algorithmic trailing edge burn/glitch */
            for(int xx = 0; xx < scr_w; xx += 2) {
                canvas_draw_dot(canvas, scr_x + xx, scr_y + wipe_h - 1 - ((xx + t) % 4));
            }
        }
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, scr_x + scr_w / 2, scr_y + scr_h / 2, AlignCenter, AlignCenter, "FLIPPED ;)");
    }

    /* Submenu action hint decoupled from canvas models */
    canvas_set_font(canvas, FontSecondary);
    if(app->tx_spam) {
        canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, "[<-] Stop Repeat");
    } else {
        canvas_draw_str_aligned(
            canvas, 64, 55, AlignCenter, AlignTop, app->tx_active ? "[<-] Cancel" : "[<-] Back");
    }
}

void tagtinker_scene_transmit_on_enter(void* context) {
    TagTinkerApp* app = context;

    if(!app->transmit_view_allocated) {
        view_allocate_model(app->transmit_view, ViewModelTypeLockFree, sizeof(TxViewModel));
        view_set_context(app->transmit_view, app);
        view_set_draw_callback(app->transmit_view, transmit_draw_cb);
        app->transmit_view_allocated = true;
    }

    TxViewModel* model = view_get_model(app->transmit_view);
    model->app = app;
    model->tick = 0;
    model->completed = false;
    model->ok = true;
    view_commit_model(app->transmit_view, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewTransmit);

    app->tx_active = true;
    furi_thread_set_callback(app->tx_thread, tx_thread_callback);
    furi_thread_start(app->tx_thread);
}

bool tagtinker_scene_transmit_on_event(void* context, SceneManagerEvent event) {
    TagTinkerApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        if(app->tx_active) {
            app->tx_active = false;
            tagtinker_ir_stop();
            return true;
        } else {
            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, TagTinkerSceneTargetActions)) {
                if(!scene_manager_search_and_switch_to_previous_scene(
                       app->scene_manager, TagTinkerSceneAbout)) {
                    if(!scene_manager_search_and_switch_to_previous_scene(
                           app->scene_manager, TagTinkerSceneBroadcast)) {
                        if(!scene_manager_search_and_switch_to_previous_scene(
                               app->scene_manager, TagTinkerSceneBroadcastMenu)) {
                            scene_manager_search_and_switch_to_previous_scene(
                                app->scene_manager, TagTinkerSceneMainMenu);
                        }
                    }
                }
            }
            return true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        TxViewModel* model = view_get_model(app->transmit_view);
        model->tick++;
        view_commit_model(app->transmit_view, true);
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 101 || event.event == 102) { /* Thread Done */
            app->tx_active = false;
            tagtinker_ir_deinit();
            TxViewModel* model = view_get_model(app->transmit_view);
            model->completed = true;
            model->ok = (event.event == 101);
            view_commit_model(app->transmit_view, true);
            notification_message(app->notifications, &sequence_success);
        }
        return true;
    }
    return false;
}

void tagtinker_scene_transmit_on_exit(void* context) {
    TagTinkerApp* app = context;

    app->tx_active = false;
    tagtinker_ir_stop();
    furi_thread_join(app->tx_thread);
    tagtinker_ir_deinit();

    tagtinker_free_frame_sequence(app);
    memset(&app->image_tx_job, 0, sizeof(app->image_tx_job));
}

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

static bool tx_is_pp16(const TagTinkerApp* app);

#define TX_FULL_JOB_PIXEL_LIMIT 49152U

static uint16_t tx_pick_chunk_height(uint16_t width, uint16_t height, bool second_plane);

static uint16_t tx_apply_signal_mode(const TagTinkerApp* app, uint16_t repeats) {
    if(repeats & 0x8000U) return repeats;

    if(tx_is_pp16(app)) {
        return (uint16_t)(repeats | 0x8000U);
    }

    return repeats;
}

static TagTinkerTagColor tx_target_color(const TagTinkerApp* app) {
    if(app->selected_target < 0 || app->selected_target >= app->target_count) {
        return TagTinkerTagColorMono;
    }

    return app->targets[app->selected_target].profile.color;
}

static bool tx_is_pp16(const TagTinkerApp* app) {
    return app->signal_mode == TagTinkerSignalPP16;
}

static bool tx_send_frame(TagTinkerApp* app, const uint8_t* frame, size_t len, uint16_t repeats) {
    if(!app->tx_active) return false;
    return tagtinker_ir_transmit(frame, len, tx_apply_signal_mode(app, repeats), 10);
}

static bool tx_send_ping(TagTinkerApp* app, const uint8_t plid[4]) {
    uint8_t frame[TAGTINKER_MAX_FRAME_SIZE];
    size_t len = tagtinker_make_ping_frame(frame, plid);
    uint16_t repeats = tx_is_pp16(app) ? 500U : 250U;
    return tx_send_frame(app, frame, len, repeats);
}

static bool tx_send_refresh(TagTinkerApp* app, const uint8_t plid[4]) {
    uint8_t frame[TAGTINKER_MAX_FRAME_SIZE];
    size_t len = tagtinker_make_refresh_frame(frame, plid);
    return tx_send_frame(app, frame, len, 50);
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
    return tx_send_frame(app, frame, len, 10);
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
    if(ok) furi_delay_ms(tx_is_pp16(app) ? 80 : 120);

    size_t frame_count = payload->byte_count / 20U;
    for(size_t i = 0; ok && i < frame_count; i++) {
        size_t len = tagtinker_make_image_data_frame(frame, plid, (uint16_t)i, &payload->data[i * 20U]);
        ok = tx_send_frame(app, frame, len, app->data_frame_repeats);
        if(ok && ((i + 1U) % 16U) == 0U && (i + 1U) < frame_count) {
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
           primary_pixels,
           secondary_pixels,
           pixel_count,
           app->compression_mode,
           &payload)) {
        return false;
    }

    bool ok = tx_send_payload_frames(app, plid, &payload, page, width, height, pos_x, pos_y);

    tagtinker_free_image_payload(&payload);
    return ok;
}

static uint32_t tx_chunk_settle_delay_ms(uint16_t width, uint16_t height, bool color_clear) {
    size_t work_pixels = (size_t)width * height * (color_clear ? 2U : 1U);
    uint32_t delay_ms = 2000U + (uint32_t)(work_pixels / 5U);
    if(delay_ms < 3500U) delay_ms = 3500U;
    if(delay_ms > 9000U) delay_ms = 9000U;
    return delay_ms;
}

static uint16_t tx_pick_chunk_height(uint16_t width, uint16_t height, bool second_plane) {
    uint16_t chunk_h = tagtinker_pick_chunk_height(width, second_plane);
    size_t budget = second_plane ? 24576U : 24576U;

    if(second_plane && (height % 2U) == 0U) {
        uint16_t half_h = height / 2U;
        if(((size_t)width * half_h * 2U) <= budget) {
            return half_h;
        }
    }

    if(chunk_h > height) chunk_h = height;

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
    if(ok) furi_delay_ms(120);
    if(ok) ok = tx_send_payload_frames(app, plid, payload, page, width, height, pos_x, pos_y);
    if(ok) furi_delay_ms(120);
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
        render_text_ex(primary, job->width, job->height, app->text_input_buf, bg_primary, fg_primary);
        render_text_ex(secondary, job->width, job->height, app->text_input_buf, 1, 0);
    } else {
        render_text_ex(
            primary,
            job->width,
            job->height,
            app->text_input_buf,
            app->invert_text ? 0 : 1,
            app->invert_text ? 1 : 0);
        if(secondary) memset(secondary, 1, pixel_count);
    }

    TagTinkerImagePayload payload;
    bool ok = tagtinker_encode_planes_payload(
        primary, secondary, pixel_count, app->compression_mode, &payload);
    free(primary);
    free(secondary);

    if(!ok) return false;

    ok = tx_send_full_payload(
        app,
        job->plid,
        &payload,
        job->page,
        job->width,
        job->height,
        job->pos_x,
        job->pos_y);
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
                fg_primary);
            render_text_region_ex(
                secondary,
                job->width,
                job->height,
                y,
                actual_h,
                app->text_input_buf,
                1,
                0);
        } else {
            render_text_region_ex(
                primary,
                job->width,
                job->height,
                y,
                actual_h,
                app->text_input_buf,
                app->invert_text ? 0 : 1,
                app->invert_text ? 1 : 0);

            if(secondary) memset(secondary, 1, (size_t)job->width * actual_h);
        }

        ok = tx_send_ping(app, job->plid);
        if(ok) furi_delay_ms(120);
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
        if(ok) furi_delay_ms(100);
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
    if(!(bpp == 1 || bpp == 24 || bpp == 32)) return false;
    info->bpp = bpp;

    int32_t bmp_h = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
    info->width = (uint16_t)(header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24));
    info->top_down = false;
    if(bmp_h < 0) {
        info->top_down = true;
        bmp_h = -bmp_h;
    }

    info->height = (uint16_t)bmp_h;
    info->data_offset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
    if(info->bpp == 1) {
        info->row_stride = ((info->width + 31U) / 32U) * 4U;
    } else if(info->bpp == 24) {
        info->row_stride = ((info->width * 3U) + 3U) & ~3U;
    } else {
        info->row_stride = info->width * 4U;
    }
    return true;
}

static void tx_map_rgb_pixel(
    uint8_t r,
    uint8_t g,
    uint8_t b,
    TagTinkerTagColor accent_color,
    uint8_t* primary,
    uint8_t* secondary) {
    bool is_red = (r > 127U) && (g < 128U) && (b < 128U);
    bool is_yellow = (r > 127U) && (g > 127U) && (b < 128U);
    bool is_accent = is_red || is_yellow;
    if(is_accent) {
        *primary = (accent_color == TagTinkerTagColorYellow) ? 0U : 1U;
        *secondary = 0U;
        return;
    }

    uint16_t luma = (uint16_t)(21U * r + 72U * g + 7U * b);
    *primary = (luma < 12800U) ? 0U : 1U;
    *secondary = 1U;
}

static bool tx_bmp_read_chunk(
    File* file,
    const TxBmpInfo* info,
    TagTinkerTagColor accent_color,
    uint16_t chunk_y,
    uint16_t chunk_h,
    uint8_t* primary,
    uint8_t* secondary) {
    uint8_t* row_buf = malloc(info->row_stride);
    if(!row_buf) return false;

    memset(primary, 1, (size_t)info->width * chunk_h);
    if(secondary) memset(secondary, 1, (size_t)info->width * chunk_h);

    bool ok = true;
    for(uint16_t local_y = 0; local_y < chunk_h; local_y++) {
        uint16_t global_y = chunk_y + local_y;
        uint32_t source_row = info->top_down ? global_y : (info->height - 1U - global_y);
        uint32_t offset = info->data_offset + source_row * info->row_stride;

        if(!storage_file_seek(file, offset, true) ||
           storage_file_read(file, row_buf, info->row_stride) != info->row_stride) {
            ok = false;
            break;
        }

        for(uint16_t x = 0; x < info->width; x++) {
            size_t idx = (size_t)local_y * info->width + x;

            if(info->bpp == 1) {
                uint8_t byte = row_buf[x / 8U];
                uint8_t bit = (byte >> (7U - (x % 8U))) & 1U;
                primary[idx] = bit ? 1U : 0U;
                if(secondary) secondary[idx] = 1U;
            } else {
                uint32_t pixel_off = x * (info->bpp / 8U);
                uint8_t b = row_buf[pixel_off + 0U];
                uint8_t g = row_buf[pixel_off + 1U];
                uint8_t r = row_buf[pixel_off + 2U];
                if(secondary) {
                    tx_map_rgb_pixel(r, g, b, accent_color, &primary[idx], &secondary[idx]);
                } else {
                    uint16_t luma = (uint16_t)(21U * r + 72U * g + 7U * b);
                    primary[idx] = (luma < 12800U) ? 0U : 1U;
                }
            }
        }
    }

    free(row_buf);
    return ok;
}

static bool tx_stream_bmp_image(TagTinkerApp* app) {
    const TagTinkerImageTxJob* job = &app->image_tx_job;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    TxBmpInfo info = {0};
    bool ok = tx_bmp_open(job->image_path, file, &info);

    if(ok && (info.width != job->width || info.height != job->height)) {
        ok = false;
    }

    TagTinkerTagColor accent_color = tx_target_color(app);
    bool accent_capable =
        accent_color == TagTinkerTagColorRed || accent_color == TagTinkerTagColorYellow;
    bool use_second_plane = app->color_clear || accent_capable;

    if(ok && tx_should_send_full_job(job->width, job->height, use_second_plane)) {
        size_t pixel_count = (size_t)job->width * job->height;
        uint8_t* primary = malloc(pixel_count);
        uint8_t* secondary = use_second_plane ? malloc(pixel_count) : NULL;

        if(!primary || (use_second_plane && !secondary)) {
            ok = false;
        } else {
            ok = tx_bmp_read_chunk(file, &info, accent_color, 0, job->height, primary, secondary);
            if(ok) {
                TagTinkerImagePayload payload;
                ok = tagtinker_encode_planes_payload(
                    primary, secondary, pixel_count, app->compression_mode, &payload);
                if(ok) {
                    ok = tx_send_full_payload(
                        app,
                        job->plid,
                        &payload,
                        job->page,
                        job->width,
                        job->height,
                        job->pos_x,
                        job->pos_y);
                    tagtinker_free_image_payload(&payload);
                }
            }
        }

        free(primary);
        free(secondary);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return ok;
    }

    uint16_t chunk_h = tx_pick_chunk_height(job->width, job->height, use_second_plane);
    uint8_t* primary = ok ? malloc((size_t)job->width * chunk_h) : NULL;
    uint8_t* secondary = (ok && use_second_plane) ? malloc((size_t)job->width * chunk_h) : NULL;
    if(ok && (!primary || (use_second_plane && !secondary))) ok = false;

    for(uint16_t y = 0; ok && y < job->height; y = (uint16_t)(y + chunk_h)) {
        uint16_t actual_h = job->height - y;
        if(actual_h > chunk_h) actual_h = chunk_h;

        ok = tx_send_ping(app, job->plid);
        if(ok) furi_delay_ms(120);
        ok = tx_bmp_read_chunk(file, &info, accent_color, y, actual_h, primary, secondary);
        if(ok)
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
        if(ok) furi_delay_ms(100);
        if(ok) ok = tx_send_refresh(app, job->plid);

        if(ok && (uint16_t)(y + actual_h) < job->height) {
            furi_delay_ms(tx_chunk_settle_delay_ms(job->width, actual_h, use_second_plane));
        }
    }

    free(primary);
    free(secondary);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static int32_t tx_thread_callback(void* context) {
    TagTinkerApp* app = context;
    bool ok = true;

    tagtinker_ir_init();

    do {
        if(app->image_tx_job.mode == TagTinkerTxModeTextImage) {
            ok = tx_stream_text_image(app);
        } else if(app->image_tx_job.mode == TagTinkerTxModeBmpImage) {
            ok = tx_stream_bmp_image(app);
        } else if(app->frame_seq_count > 0) {
            for(size_t i = 0; i < app->frame_seq_count; i++) {
                if(!app->tx_active) { ok = false; break; }
                if(i > 0) furi_delay_ms(20);
                ok = tagtinker_ir_transmit(
                    app->frame_sequence[i], app->frame_lengths[i],
                    app->frame_repeats[i], 10);
                if(!ok) break;
            }
        } else {
            ok = tagtinker_ir_transmit(app->frame_buf, app->frame_len, app->repeats, 10);
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
    for(int i=0; i<4; i++) {
        canvas_draw_dot(canvas, f_x + 32, f_y + 8 + i*3);
        canvas_draw_dot(canvas, f_x + 34, f_y + 8 + i*3);
    }
    /* IR Blaster window on the right tip */
    int ir_x = f_x + f_w; // 25
    int ir_y = f_y + 10;
    canvas_draw_box(canvas, ir_x, ir_y, 4, 16);

    int center_y = f_y + 18; /* perfectly aligns with tag center */

    if (app->tx_active) {
        /* Aggressive Pulsing Data-Stream Chevrons with bits */
        uint8_t wave_phase = (t * 2) % 15;
        for(int w=0; w<2; w++) {
            int r = wave_phase + w*18; 
            if(r > 0 && r < 20) {
                int wave_x = ir_x + 4 + r;
                int wave_h = r/2 + 3; 
                /* 2px thick chevron pointing right */
                canvas_draw_line(canvas, wave_x,   center_y, wave_x - wave_h/2, center_y - wave_h);
                canvas_draw_line(canvas, wave_x,   center_y, wave_x - wave_h/2, center_y + wave_h);
                canvas_draw_line(canvas, wave_x+1, center_y, wave_x+1 - wave_h/2, center_y - wave_h);
                canvas_draw_line(canvas, wave_x+1, center_y, wave_x+1 - wave_h/2, center_y + wave_h);
                /* Data stream particles */
                if((t + w) % 2 == 0) {
                    canvas_draw_dot(canvas, wave_x + 3, center_y - wave_h/2);
                    canvas_draw_dot(canvas, wave_x - 3, center_y + wave_h/2 + 2);
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
    canvas_draw_rframe(canvas, tag_x+1, tag_y+1, tag_w-2, tag_h-2, 1);
    
    /* True 32-bit encoded barcode representation on the rim */
    int bc_x = tag_x + 3;
    int bc_w = 8;
    int bc_y = tag_y + 4;
    uint8_t bar_pattern[] = {0b10110100, 0b11010010, 0b10011011, 0b01101010};
    for(int i=0; i<32; i++) {
        if( (bar_pattern[i/8] >> (7-(i%8))) & 1 ) {
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
    
    if (app->tx_active) {
        bool show_result = (cycle >= 20);
        
        if(!show_result) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, scr_x + 2, scr_y + 11, "LAB NOTE");
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, scr_x + 5, scr_y + 26, "STUDY");
        } else {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, scr_x + scr_w / 2, scr_y + scr_h / 2, AlignCenter, AlignCenter, "FLIPPED ;)");
        }

        /* Glitchy Matrix Wipe Transition */
        if(cycle >= 15 && cycle < 20) {
            int wipe_h = ((cycle - 15) * scr_h) / 5;
            canvas_draw_box(canvas, scr_x, scr_y, scr_w, wipe_h);
            /* Algorithmic leading edge burn/glitch */
            for(int xx = 0; xx < scr_w; xx += 2) {
                canvas_draw_dot(canvas, scr_x + xx, scr_y + wipe_h + ((xx+t)%4));
            }
        } else if (cycle >= 20 && cycle < 25) {
            int wipe_h = ((cycle - 20) * scr_h) / 5;
            canvas_draw_box(canvas, scr_x, scr_y + wipe_h, scr_w, scr_h - wipe_h);
            /* Algorithmic trailing edge burn/glitch */
            for(int xx = 0; xx < scr_w; xx += 2) {
                canvas_draw_dot(canvas, scr_x + xx, scr_y + wipe_h - 1 - ((xx+t)%4));
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
        canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, app->tx_active ? "[<-] Cancel" : "[<-] Back");
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
            if(!scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TagTinkerSceneTargetActions)) {
                if(!scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TagTinkerSceneBroadcast)) {
                    if(!scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TagTinkerSceneBroadcastMenu)) {
                        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TagTinkerSceneMainMenu);
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

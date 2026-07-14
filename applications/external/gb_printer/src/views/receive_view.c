// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2024 KBEmbedded

#include <furi.h>
#include <gui/modules/submenu.h>
#include <storage/storage.h>
#include <src/include/fgp_app.h>
#include <src/scenes/include/fgp_scene.h>

#include <gb_printer_icons.h>

// gblink protocol support
#include <protocols/printer/include/printer_proto.h>
#include <protocols/printer/include/printer_receive.h>

#include <src/include/file_handling.h>
#include <src/include/png.h>
#include <src/include/tile_tools.h>

/* XXX: TODO turn this in to an enum */
#define LINE_XFER 0x80000000
#define PRINT     0x40000000
#define COMPLETE  0x20000000

struct recv_model {
    int count;
    int converted;
    int errors;
};

struct recv_ctx {
    View* view;
    struct recv_model* model;
    struct fgp_app* fgp;
    FuriTimer* timer;
    ViewDispatcher* view_dispatcher;

    // Printer handling
    void* printer_handle;

    struct gb_image* volatile_image;
    struct gb_image* image;
    int packet_cnt;

    // PNG handling
    void* png_handle;

    // File operations
    void* file_handle;
};

static void fgp_receive_view_timer(void* context) {
    struct recv_ctx* ctx = context;

    with_view_model(ctx->view, struct recv_model * model, { UNUSED(model); }, true);
}

static void printer_callback(void* context, struct gb_image* image, enum cb_reason reason) {
    struct recv_ctx* ctx = context;

    FURI_LOG_D("printer", "printer_callback reason %d", (int)reason);

    switch(reason) {
    case reason_line_xfer:
        ctx->packet_cnt++;
        view_dispatcher_send_custom_event(ctx->view_dispatcher, LINE_XFER);
        break;
    case reason_print:
        /* Set up a pointer to the image just received and call our
		 * local handler for dealing with a print command.
		 * Our handler will copy this data to its own local image buffer
		 * before marking the print as complete. Since events are queued
		 * and dispatched sequentially, once the image is marked as
		 * printed, the receive proto handler will begin to receive the
		 * next image. If we end up here again and the previous handler
		 * call is still running another event is simply added to the
		 * queue but that image will not have been marked as printed so
		 * the receive proto will tell the GB that it is still printing.
		 */
        ctx->volatile_image = image;
        view_dispatcher_send_custom_event(ctx->view_dispatcher, PRINT);
        break;
    case reason_complete:
        view_dispatcher_send_custom_event(ctx->view_dispatcher, COMPLETE);
        break;
    }
}

static bool fgp_receive_view_event(uint32_t event, void* context) {
    struct recv_ctx* ctx = context;
    struct gb_image* image = ctx->image;
    bool consumed = false;
    bool error = false;
    FuriString* fs_tmp;
    uint8_t px_y = 0;
    uint8_t px_x = 0;
    bool last_margin_zero = false;
    bool same_image = false;
    enum png_chunks chunk;

    if(event == LINE_XFER) consumed = true;

    if(event == PRINT) {
        fs_tmp = furi_string_alloc();

        /* Before we clobber the image data, take note of the margins.
		 * If the last bottom margin was 0, and this top margin is 0,
		 * then we need to append this image to the last image we saved.
		 * Also, assuming if data_sz is 0, that this is the first image
		 * being printed since the application was launched.
		 */
        if(!(image->margins & 0x0f) && image->data_sz) last_margin_zero = true;

        /* Copy the volatile image from the printer protocol handler to
		 * a buffer we can work with locally after the print is marked
		 * as complete.
		 */
        memcpy(image, ctx->volatile_image, sizeof(struct gb_image));
        printer_receive_print_complete(ctx->printer_handle);

        /* Now look at the margins of this image, if there is no margin
		 * at the start, and there was no margin at the end of the last
		 * image, then assume these are intended to be the same image.
		 */
        if(last_margin_zero && !(image->margins & 0xf0)) same_image = true;

        /* If the last margin was zero, we didn't increment the file count.
		 * So if the last margin was zero, but its not the same image,
		 * then bump the file count now.
		 */
        if(last_margin_zero && !same_image) fgp_storage_next_count(ctx->file_handle);

        /* Prep some image information */
        px_x = 160; // TODO: Photo! transfer will be less than this
        px_y = image->data_sz / 40; // 40 is bytes per line, 160 px / 4 (px/byte)

        with_view_model(ctx->view, struct recv_model * model, { model->count++; }, false);

        /* Save binary version always */
        /* We don't care if this was previously opened or not, we just
		 * need to blindly append data to it and its fine.
		 */
        if(ctx->fgp->options & OPT_SAVE_BIN) {
            error |= !fgp_storage_open(ctx->file_handle, ".bin");
            error |= !fgp_storage_write(ctx->file_handle, image, image->data_sz);
            error |= !fgp_storage_close(ctx->file_handle);
        }

        /* Similar above, we want to just append to this file, but, if
		 * this is the same image, we don't want to re-add the header.
		 */
        if(ctx->fgp->options & OPT_SAVE_BIN_HDR) {
            error |= !fgp_storage_open(ctx->file_handle, "-hdr.bin");
            if(!same_image) error |= !fgp_storage_write(ctx->file_handle, "GB-BIN01", 8);
            error |= !fgp_storage_write(ctx->file_handle, image, image->data_sz);
            error |= !fgp_storage_close(ctx->file_handle);
        }

        if(!(ctx->fgp->options & OPT_SAVE_PNG)) goto skip_png;

        /* For saving to a PNG, the image data needs to be converted from
		 * tiles to scanlines.
		 */
        tile_to_scanline(image->data, px_x / 8, px_y / 8);

        /* Save PNG */
        if(!same_image) {
            png_reset(ctx->png_handle, px_x, px_y);
            png_dat_write(ctx->png_handle, image->data);
            png_palette_set(ctx->png_handle, palette_rgb16_get(ctx->fgp->palette_idx));
            furi_string_printf(fs_tmp, "-%s.png", palette_shortname_get(ctx->fgp->palette_idx));
            error |= !fgp_storage_open(ctx->file_handle, furi_string_get_cstr(fs_tmp));
            for(chunk = CHUNK_START; chunk < CHUNK_COUNT; chunk++)
                error |= !fgp_storage_write(
                    ctx->file_handle,
                    png_buf_get(ctx->png_handle, chunk),
                    png_len_get(ctx->png_handle, chunk));
            error |= !fgp_storage_close(ctx->file_handle);
        } else {
            furi_string_printf(fs_tmp, "-%s.png", palette_shortname_get(ctx->fgp->palette_idx));

            /* The PNGs are saved with multiple IDAT chunks to make
			 * expanding the images "easier". The first IDAT chunk is
			 * the zlib header. +n IDAT chunks are images, each with
			 * their own DEFLATE header. These can be up to 144 px tall
			 * but can also be less. The last IDAT chunk is the adler32
			 * zlib checksum.
			 *
			 * In order to expand an existing PNG image all we have to
			 * is:
			 * - Unset the BFINAL flag in the last image IDAT section
			 *   that we still have in memory.
			 * - Seek to the start of the last IDAT chunk with image data,
			 *   This is the special LAST_IDAT offset.
			 * - Rewrite the previous image (with BFINAL unset)
			 * - Update the image buffer with the new height, which
			 *   is also added to the IHDR in memory.
			 * - Write the new image data to the image buffer,
			 *   which also updates the running adler32.
			 * - Write the new image data to disk.
			 * - Rewrite the IDAT_CHECK, adler32 IDAT section.
			 * - Rewrite IEND (which is static anyway).
			 * - Jump to start of file.
			 * - Finally, rewrite IHDR with the updated height of
			 *   the full image.
			 */
            png_deflate_unfinal(ctx->png_handle);
            error |= !fgp_storage_open(ctx->file_handle, furi_string_get_cstr(fs_tmp));
            error |= !fgp_storage_seek(
                ctx->file_handle, -(png_len_get(ctx->png_handle, LAST_IDAT)), false);
            error |= !fgp_storage_write(
                ctx->file_handle,
                png_buf_get(ctx->png_handle, IDAT),
                png_len_get(ctx->png_handle, IDAT));
            png_add_height(ctx->png_handle, px_y);
            png_dat_write(ctx->png_handle, image->data);
            error |= !fgp_storage_write(
                ctx->file_handle,
                png_buf_get(ctx->png_handle, IDAT),
                png_len_get(ctx->png_handle, IDAT));
            error |= !fgp_storage_write(
                ctx->file_handle,
                png_buf_get(ctx->png_handle, IDAT_CHECK),
                png_len_get(ctx->png_handle, IDAT_CHECK));
            error |= !fgp_storage_write(
                ctx->file_handle,
                png_buf_get(ctx->png_handle, IEND),
                png_len_get(ctx->png_handle, IEND));
            error |= !fgp_storage_seek(ctx->file_handle, 0, true);
            error |= !fgp_storage_write(
                ctx->file_handle,
                png_buf_get(ctx->png_handle, IHDR),
                png_len_get(ctx->png_handle, IHDR));
            error |= !fgp_storage_close(ctx->file_handle);
        }

        if(!error) {
            with_view_model(ctx->view, struct recv_model * model, { model->converted++; }, false);
        } else {
            with_view_model(ctx->view, struct recv_model * model, { model->errors++; }, false);
        }

    skip_png:
        /* Don't increment yet if the end margin is 0 */
        if((image->margins & 0x0f)) fgp_storage_next_count(ctx->file_handle);

        furi_string_free(fs_tmp);

        consumed = true;
    }

    return consumed;
}

static void fgp_receive_view_enter(void* context) {
    struct recv_ctx* ctx = context;

    view_allocate_model(ctx->view, ViewModelTypeLockFree, sizeof(struct recv_model));

    /* XXX: TODO: Startup of this takes a long time. Presumably, it is all
	 * of the storage allocation, directory creation, finding the latest
	 * file number, etc. Because this runs at a higher priority than the
	 * timer callback, setting up the draw timer first has no impact on
	 * the issue.
	 * I believe draw calls are also in the same context as this thread,
	 * so I don't think there is a good way to issue a draw callback here.
	 * Need to figure out a better way to handle this setup.
	 */
    ctx->image = malloc(sizeof(struct gb_image));
    ctx->printer_handle = ctx->fgp->printer_handle;

    ctx->file_handle = fgp_storage_alloc("GCIM_", ".bin");

    printer_callback_context_set(ctx->printer_handle, ctx);
    printer_callback_set(ctx->printer_handle, printer_callback);
    printer_receive_start(ctx->printer_handle);

    ctx->png_handle = png_alloc(160, 144);

    ctx->timer = furi_timer_alloc(fgp_receive_view_timer, FuriTimerTypePeriodic, ctx);
    furi_timer_start(ctx->timer, furi_ms_to_ticks(200));
}

static void fgp_receive_view_exit(void* context) {
    struct recv_ctx* ctx = context;

    fgp_storage_free(ctx->file_handle);
    furi_timer_free(ctx->timer);

    png_free(ctx->png_handle);

    printer_stop(ctx->printer_handle);

    free(ctx->image);
    view_free_model(ctx->view);
}

static bool fgp_receive_view_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    bool ret = false;

    return ret;
}

static void fgp_receive_view_draw(Canvas* canvas, void* view_model) {
    struct recv_model* model = view_model;
    char string[26];
    canvas_draw_str(canvas, 38, 30, "Recv:");
    snprintf(string, sizeof(string), "%d", model->count);
    canvas_draw_str(canvas, 66, 30, string);

    canvas_draw_str(canvas, 38, 38, "Conv:");
    snprintf(string, sizeof(string), "%d", model->converted);
    canvas_draw_str(canvas, 66, 38, string);

    canvas_draw_str(canvas, 38, 46, "Err:");
    snprintf(string, sizeof(string), "%d", model->errors);
    canvas_draw_str(canvas, 66, 46, string);

    canvas_draw_icon(canvas, 96, 3, &I_gbc_32x58);
    canvas_draw_icon(canvas, 0, 2, &I_flipper_w_cable_26x61);
    canvas_draw_frame(canvas, 91, 16, 5, 6);
    canvas_draw_box(canvas, 88, 18, 3, 2);
    canvas_draw_box(canvas, 87, 2, 2, 17);
    canvas_draw_box(canvas, 24, 1, 63, 2);

    /* TODO: Animate when a link is established */
}

View* fgp_receive_view_get_view(void* context) {
    struct recv_ctx* ctx = context;
    return ctx->view;
}

void* fgp_receive_view_alloc(struct fgp_app* fgp) {
    struct recv_ctx* ctx = malloc(sizeof(struct recv_ctx));

    ctx->view_dispatcher = fgp->view_dispatcher;
    ctx->fgp = fgp;

    ctx->view = view_alloc();
    view_set_context(ctx->view, ctx);

    view_set_input_callback(ctx->view, fgp_receive_view_input);
    view_set_draw_callback(ctx->view, fgp_receive_view_draw);
    view_set_enter_callback(ctx->view, fgp_receive_view_enter);
    view_set_exit_callback(ctx->view, fgp_receive_view_exit);
    view_set_custom_callback(ctx->view, fgp_receive_view_event);

    return ctx;
}

void fgp_receive_view_free(void* recv_ctx) {
    struct recv_ctx* ctx = recv_ctx;

    view_free(ctx->view);
    free(ctx);
}

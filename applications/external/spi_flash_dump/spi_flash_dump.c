/* spi_flash_dump.c — Main application, UI views, and entry point for SPI Flash Dump.
 * Provides wiring guide, chip detection, read progress, verify progress,
 * hex preview, and settings views.  Delegates SPI I/O to spi_worker. */

#include "spi_flash_dump.h"
#include "spi_worker.h"
#include "hex_viewer.h"

#include <stdio.h>
#include <datetime/datetime.h>

/* ================================================================== */
/*  Helper – SPI speed enum → clock delay in microseconds             */
/* ================================================================== */
static uint32_t spi_speed_delay(SpiSpeed speed) {
    switch(speed) {
    case SpiSpeedSlow:
        return 10;
    case SpiSpeedMedium:
        return 2;
    case SpiSpeedFast:
    default:
        return 0;
    }
}

static const char* spi_speed_label(SpiSpeed s) {
    switch(s) {
    case SpiSpeedSlow:
        return "Slow (~50kHz)";
    case SpiSpeedMedium:
        return "Medium (~250kHz)";
    case SpiSpeedFast:
        return "Fast (~1MHz)";
    default:
        return "?";
    }
}

static const char* spi_read_cmd_label(SpiReadCmd c) {
    switch(c) {
    case SpiReadCmdNormal:
        return "Normal (0x03)";
    case SpiReadCmdFast:
        return "Fast (0x0B)";
    default:
        return "?";
    }
}

/* ================================================================== */
/*  Format helpers                                                    */
/* ================================================================== */

static void format_size(uint32_t bytes, char* buf, size_t buf_sz) {
    if(bytes >= 1024 * 1024) {
        snprintf(buf, buf_sz, "%lu MB", (unsigned long)(bytes / (1024 * 1024)));
    } else if(bytes >= 1024) {
        snprintf(buf, buf_sz, "%lu KB", (unsigned long)(bytes / 1024));
    } else {
        snprintf(buf, buf_sz, "%lu B", (unsigned long)bytes);
    }
}

/* ================================================================== */
/*  Custom View: Read Progress                                        */
/* ================================================================== */

typedef struct {
    uint32_t bytes_done;
    uint32_t total;
    uint32_t start_tick;
    uint32_t crc32;
    bool finished;
    bool success;
    bool crc32_valid;
} ReadProgressModel;

static void read_progress_draw_cb(Canvas* canvas, void* model_ptr) {
    ReadProgressModel* m = model_ptr;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Reading Flash...");

    canvas_set_font(canvas, FontSecondary);

    /* Progress bar */
    uint32_t pct = m->total ? (uint32_t)((uint64_t)m->bytes_done * 100 / m->total) : 0;
    canvas_draw_rframe(canvas, 4, 16, 120, 12, 2);
    uint32_t fill = m->total ? (uint32_t)((uint64_t)m->bytes_done * 116 / m->total) : 0;
    if(fill > 116) fill = 116;
    canvas_draw_rbox(canvas, 6, 18, (uint8_t)fill, 8, 1);

    /* Percentage */
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%lu%%", (unsigned long)pct);
    canvas_draw_str_aligned(canvas, 64, 19, AlignCenter, AlignTop, tmp);

    /* Bytes read / total */
    char sz_done[16], sz_total[16];
    format_size(m->bytes_done, sz_done, sizeof(sz_done));
    format_size(m->total, sz_total, sizeof(sz_total));
    char sz_line[48];
    snprintf(sz_line, sizeof(sz_line), "%s / %s", sz_done, sz_total);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignTop, sz_line);

    /* Speed & ETA */
    uint32_t elapsed_ms = furi_get_tick() - m->start_tick;
    if(elapsed_ms > 0 && m->bytes_done > 0) {
        uint32_t speed_bps = (uint32_t)((uint64_t)m->bytes_done * 1000 / elapsed_ms);
        char spd[16];
        if(speed_bps >= 1024) {
            snprintf(spd, sizeof(spd), "%lu KB/s", (unsigned long)(speed_bps / 1024));
        } else {
            snprintf(spd, sizeof(spd), "%lu B/s", (unsigned long)speed_bps);
        }

        uint32_t remaining = m->total - m->bytes_done;
        uint32_t eta_sec = speed_bps ? (remaining / speed_bps) : 0;
        uint32_t eta_min = eta_sec / 60;
        eta_sec %= 60;

        snprintf(tmp, sizeof(tmp), "Speed: %s", spd);
        canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignTop, tmp);

        snprintf(
            tmp, sizeof(tmp), "ETA: %lum %lus", (unsigned long)eta_min, (unsigned long)eta_sec);
        canvas_draw_str_aligned(canvas, 64, 53, AlignCenter, AlignTop, tmp);
    }

    if(m->finished) {
        canvas_set_font(canvas, FontPrimary);
        if(m->success) {
            canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, "DONE! Press OK");
            if(m->crc32_valid) {
                char crc_str[20];
                snprintf(crc_str, sizeof(crc_str), "CRC32: 0x%08lX", (unsigned long)m->crc32);
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, crc_str);
            }
        } else {
            canvas_draw_str_aligned(canvas, 64, 53, AlignCenter, AlignTop, "ERROR! Press Back");
        }
    }
}

static bool read_progress_input_cb(InputEvent* event, void* ctx) {
    SpiFlashDumpApp* app = ctx;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk && app->worker_state == SpiWorkerStateDone) {
        /* Proceed to verify or hex preview */
        if(app->verify_after_read) {
            view_dispatcher_send_custom_event(app->view_dispatcher, 100); /* start verify */
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, 101); /* show hex */
        }
        return true;
    }
    if(event->key == InputKeyBack) {
        if(app->worker_state == SpiWorkerStateDone || app->worker_state == SpiWorkerStateError) {
            view_dispatcher_send_custom_event(app->view_dispatcher, 102); /* back to chip info */
        }
        return true;
    }
    return false;
}

/* ================================================================== */
/*  Custom View: Verify Progress                                      */
/* ================================================================== */

typedef struct {
    uint32_t bytes_done;
    uint32_t total;
    uint32_t match_count;
    uint32_t mismatch_count;
    bool finished;
    bool all_match;
} VerifyProgressModel;

static void verify_progress_draw_cb(Canvas* canvas, void* model_ptr) {
    VerifyProgressModel* m = model_ptr;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Verifying...");

    canvas_set_font(canvas, FontSecondary);

    /* Progress bar */
    canvas_draw_rframe(canvas, 4, 16, 120, 12, 2);
    uint32_t fill = m->total ? (uint32_t)((uint64_t)m->bytes_done * 116 / m->total) : 0;
    if(fill > 116) fill = 116;
    canvas_draw_rbox(canvas, 6, 18, (uint8_t)fill, 8, 1);

    uint32_t pct = m->total ? (uint32_t)((uint64_t)m->bytes_done * 100 / m->total) : 0;
    char tmp[48];
    snprintf(tmp, sizeof(tmp), "%lu%%", (unsigned long)pct);
    canvas_draw_str_aligned(canvas, 64, 19, AlignCenter, AlignTop, tmp);

    /* Match / mismatch counters */
    char sz_done[16], sz_total[16];
    format_size(m->bytes_done, sz_done, sizeof(sz_done));
    format_size(m->total, sz_total, sizeof(sz_total));
    char vf_line[48];
    snprintf(vf_line, sizeof(vf_line), "%s / %s", sz_done, sz_total);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignTop, vf_line);

    snprintf(
        tmp,
        sizeof(tmp),
        "Match:%lu  Mismatch:%lu",
        (unsigned long)m->match_count,
        (unsigned long)m->mismatch_count);
    canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignTop, tmp);

    if(m->finished) {
        canvas_set_font(canvas, FontPrimary);
        if(m->all_match) {
            canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "VERIFIED OK!");
        } else {
            canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "MISMATCH!");
        }
    }
}

static bool verify_progress_input_cb(InputEvent* event, void* ctx) {
    SpiFlashDumpApp* app = ctx;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk &&
       (app->worker_state == SpiWorkerStateDone || app->worker_state == SpiWorkerStateError)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 101); /* show hex */
        return true;
    }
    if(event->key == InputKeyBack) {
        if(!spi_worker_is_running(app->worker)) {
            view_dispatcher_send_custom_event(app->view_dispatcher, 102); /* back */
        }
        return true;
    }
    return false;
}

/* ================================================================== */
/*  Progress callbacks (called from worker thread)                    */
/* ================================================================== */

static void read_progress_cb(uint32_t bytes_done, uint32_t total, void* ctx) {
    SpiFlashDumpApp* app = ctx;
    app->progress_bytes = bytes_done;
    app->progress_total = total;

    /* Update the view model from the worker thread.  The locking model
       makes this safe. */
    with_view_model(
        app->read_progress_view,
        ReadProgressModel * m,
        {
            m->bytes_done = bytes_done;
            m->total = total;
        },
        true);
}

static void verify_progress_cb(uint32_t bytes_done, uint32_t total, void* ctx) {
    SpiFlashDumpApp* app = ctx;
    app->progress_bytes = bytes_done;
    app->progress_total = total;

    with_view_model(
        app->verify_progress_view,
        VerifyProgressModel * m,
        {
            m->bytes_done = bytes_done;
            m->total = total;
            m->match_count = app->verify_match;
            m->mismatch_count = app->verify_mismatch;
        },
        true);
}

/* ================================================================== */
/*  Navigation callback (ViewDispatcher)                              */
/* ================================================================== */

static bool app_navigation_cb(void* ctx) {
    SpiFlashDumpApp* app = ctx;
    /* If worker is running, block back navigation */
    if(spi_worker_is_running(app->worker)) return true; /* consume = stay */
    return false; /* allow default back behaviour */
}

/* ================================================================== */
/*  Custom event callback                                             */
/* ================================================================== */

static bool app_custom_event_cb(void* ctx, uint32_t event) {
    SpiFlashDumpApp* app = ctx;

    switch(event) {
    case 100: {
        /* Start verify */
        if(app->worker_running) return true; /* ignore if worker still active */
        app->worker_state = SpiWorkerStateVerifying;
        app->worker_running = true;
        app->verify_match = 0;
        app->verify_mismatch = 0;

        with_view_model(
            app->verify_progress_view,
            VerifyProgressModel * m,
            {
                m->bytes_done = 0;
                m->total = app->chip.size_bytes;
                m->match_count = 0;
                m->mismatch_count = 0;
                m->finished = false;
                m->all_match = false;
            },
            true);

        view_dispatcher_switch_to_view(app->view_dispatcher, SpiFlashDumpViewVerifyProgress);

        uint8_t cmd = (app->read_cmd == SpiReadCmdFast) ? CMD_FAST_READ : CMD_READ_DATA;
        spi_worker_start_verify(
            app->worker,
            &app->chip,
            app->dump_path,
            cmd,
            spi_speed_delay(app->spi_speed),
            verify_progress_cb,
            app,
            &app->verify_match,
            &app->verify_mismatch);
        return true;
    }

    case 101: {
        /* Show hex preview */
        hex_viewer_load_file(app->hex_viewer, app->dump_path);
        view_dispatcher_switch_to_view(app->view_dispatcher, SpiFlashDumpViewHexPreview);
        return true;
    }

    case 102: {
        /* Back to chip info */
        view_dispatcher_switch_to_view(app->view_dispatcher, SpiFlashDumpViewChipInfo);
        return true;
    }

    default:
        return false;
    }
}

/* Tracks which view opened Settings so Back returns to the right place. */
static SpiFlashDumpView settings_return_view = SpiFlashDumpViewWiringGuide;

/* ================================================================== */
/*  Scene: Wiring Guide                                               */
/* ================================================================== */

static void wiring_guide_setup(SpiFlashDumpApp* app) {
    widget_reset(app->wiring_guide);
    widget_add_string_element(
        app->wiring_guide, 64, 0, AlignCenter, AlignTop, FontPrimary, "SPI Flash Wiring");
    widget_add_string_element(
        app->wiring_guide, 0, 12, AlignLeft, AlignTop, FontSecondary, "CLK  -> Pin 5 (PB3)");
    widget_add_string_element(
        app->wiring_guide, 0, 21, AlignLeft, AlignTop, FontSecondary, "MISO -> Pin 6 (PA6)");
    widget_add_string_element(
        app->wiring_guide, 0, 30, AlignLeft, AlignTop, FontSecondary, "MOSI -> Pin 7 (PA7)");
    widget_add_string_element(
        app->wiring_guide, 0, 39, AlignLeft, AlignTop, FontSecondary, "CS   -> Pin 4 (PA4)");
    widget_add_string_element(
        app->wiring_guide, 0, 48, AlignLeft, AlignTop, FontSecondary, "VCC:Pin9  GND:Pin8");
    widget_add_string_element(
        app->wiring_guide, 64, 57, AlignCenter, AlignTop, FontSecondary, "OK=Detect  Right=Cfg");
}

static bool wiring_guide_input_cb(InputEvent* event, void* ctx) {
    SpiFlashDumpApp* app = ctx;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        /* Init GPIO & detect chip */
        spi_worker_gpio_init();

        uint32_t delay = spi_speed_delay(app->spi_speed);
        app->chip_detected = chip_detect(&app->chip, delay);
        app->status_reg1 = read_status_register(delay);
        app->status_reg2 = read_status_register2(delay);

        /* Build chip info widget */
        widget_reset(app->chip_info_widget);

        if(app->chip_detected || app->chip.size_bytes > 0) {
            app->chip_detected = true; /* treat unknown-but-sized as detected */

            char line[80];
            char sz[16];
            format_size(app->chip.size_bytes, sz, sizeof(sz));

            snprintf(line, sizeof(line), "Chip: %s", app->chip.part_name);
            widget_add_string_element(
                app->chip_info_widget, 64, 2, AlignCenter, AlignTop, FontPrimary, line);

            snprintf(
                line,
                sizeof(line),
                "Mfr: %s (0x%02X)",
                app->chip.manufacturer_name,
                app->chip.manufacturer_id);
            widget_add_string_element(
                app->chip_info_widget, 0, 15, AlignLeft, AlignTop, FontSecondary, line);

            snprintf(
                line,
                sizeof(line),
                "JEDEC: %02X %02X %02X",
                app->chip.manufacturer_id,
                app->chip.device_id[0],
                app->chip.device_id[1]);
            widget_add_string_element(
                app->chip_info_widget, 0, 26, AlignLeft, AlignTop, FontSecondary, line);

            snprintf(line, sizeof(line), "Size: %s", sz);
            widget_add_string_element(
                app->chip_info_widget, 0, 37, AlignLeft, AlignTop, FontSecondary, line);

            snprintf(
                line, sizeof(line), "SR1:0x%02X SR2:0x%02X", app->status_reg1, app->status_reg2);
            widget_add_string_element(
                app->chip_info_widget, 0, 48, AlignLeft, AlignTop, FontSecondary, line);

            widget_add_string_element(
                app->chip_info_widget,
                64,
                58,
                AlignCenter,
                AlignTop,
                FontSecondary,
                "OK=Read  Right=Cfg");
        } else {
            widget_add_string_element(
                app->chip_info_widget,
                64,
                10,
                AlignCenter,
                AlignTop,
                FontPrimary,
                "No chip detected!");

            char line[48];
            snprintf(
                line,
                sizeof(line),
                "ID: %02X %02X %02X",
                app->chip.manufacturer_id,
                app->chip.device_id[0],
                app->chip.device_id[1]);
            widget_add_string_element(
                app->chip_info_widget, 64, 28, AlignCenter, AlignTop, FontSecondary, line);

            widget_add_string_element(
                app->chip_info_widget,
                64,
                42,
                AlignCenter,
                AlignTop,
                FontSecondary,
                "Check wiring and try again");

            widget_add_string_element(
                app->chip_info_widget, 64, 56, AlignCenter, AlignTop, FontSecondary, "Back=Retry");
        }

        view_dispatcher_switch_to_view(app->view_dispatcher, SpiFlashDumpViewChipInfo);
        return true;
    }

    if(event->key == InputKeyRight && event->type == InputTypeShort) {
        settings_return_view = SpiFlashDumpViewWiringGuide;
        view_dispatcher_switch_to_view(app->view_dispatcher, SpiFlashDumpViewSettings);
        return true;
    }

    return false;
}

/* ================================================================== */
/*  Scene: Chip Info – input                                          */
/* ================================================================== */

static bool chip_info_input_cb(InputEvent* event, void* ctx) {
    SpiFlashDumpApp* app = ctx;

    if(event->key == InputKeyOk && event->type == InputTypeShort && app->chip_detected) {
        /* Ignore if a worker thread is still active */
        if(app->worker_running) return true;

        /* Generate output path */
        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);

        snprintf(
            app->dump_path,
            sizeof(app->dump_path),
            SPI_DUMP_DIR "/%s_%04d%02d%02d_%02d%02d%02d.bin",
            app->chip.part_name,
            dt.year,
            dt.month,
            dt.day,
            dt.hour,
            dt.minute,
            dt.second);

        /* Prepare progress view */
        app->progress_start_tick = furi_get_tick();
        app->worker_state = SpiWorkerStateReading;
        app->worker_running = true;

        with_view_model(
            app->read_progress_view,
            ReadProgressModel * m,
            {
                m->bytes_done = 0;
                m->total = app->chip.size_bytes;
                m->start_tick = app->progress_start_tick;
                m->finished = false;
                m->success = false;
                m->crc32 = 0;
                m->crc32_valid = false;
            },
            true);

        view_dispatcher_switch_to_view(app->view_dispatcher, SpiFlashDumpViewReadProgress);

        /* Start worker */
        uint8_t cmd = (app->read_cmd == SpiReadCmdFast) ? CMD_FAST_READ : CMD_READ_DATA;
        spi_worker_start_read(
            app->worker,
            &app->chip,
            app->dump_path,
            cmd,
            spi_speed_delay(app->spi_speed),
            read_progress_cb,
            app);
        return true;
    }

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        /* Back to wiring guide (will re-detect on next OK) */
        spi_worker_gpio_deinit();
        view_dispatcher_switch_to_view(app->view_dispatcher, SpiFlashDumpViewWiringGuide);
        return true;
    }

    if(event->key == InputKeyRight && event->type == InputTypeShort) {
        settings_return_view = SpiFlashDumpViewChipInfo;
        view_dispatcher_switch_to_view(app->view_dispatcher, SpiFlashDumpViewSettings);
        return true;
    }

    return false;
}

/* ================================================================== */
/*  Settings (VariableItemList)                                       */
/* ================================================================== */

static void settings_speed_cb(VariableItem* item) {
    SpiFlashDumpApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= SpiSpeedCount) idx = 0;
    app->spi_speed = (SpiSpeed)idx;
    variable_item_set_current_value_text(item, spi_speed_label(app->spi_speed));
}

static void settings_verify_cb(VariableItem* item) {
    SpiFlashDumpApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->verify_after_read = (idx == 1);
    variable_item_set_current_value_text(item, idx ? "Yes" : "No");
}

static void settings_readcmd_cb(VariableItem* item) {
    SpiFlashDumpApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= SpiReadCmdCount) idx = 0;
    app->read_cmd = (SpiReadCmd)idx;
    variable_item_set_current_value_text(item, spi_read_cmd_label(app->read_cmd));
}

static void settings_list_setup(SpiFlashDumpApp* app) {
    VariableItem* item;

    item = variable_item_list_add(
        app->settings_list, "SPI Speed", SpiSpeedCount, settings_speed_cb, app);
    variable_item_set_current_value_index(item, (uint8_t)app->spi_speed);
    variable_item_set_current_value_text(item, spi_speed_label(app->spi_speed));

    item = variable_item_list_add(
        app->settings_list, "Verify After Read", 2, settings_verify_cb, app);
    variable_item_set_current_value_index(item, app->verify_after_read ? 1 : 0);
    variable_item_set_current_value_text(item, app->verify_after_read ? "Yes" : "No");

    item = variable_item_list_add(
        app->settings_list, "Read Command", SpiReadCmdCount, settings_readcmd_cb, app);
    variable_item_set_current_value_index(item, (uint8_t)app->read_cmd);
    variable_item_set_current_value_text(item, spi_read_cmd_label(app->read_cmd));
}

static void settings_enter_cb(void* ctx, uint32_t index) {
    UNUSED(index);
    UNUSED(ctx);
    /* No action on enter for these items */
}

static uint32_t settings_back_cb(void* ctx) {
    UNUSED(ctx);
    return settings_return_view;
}

/* ================================================================== */
/*  View enter/exit callbacks for worker completion polling           */
/* ================================================================== */

/* Timer callback to poll worker completion */
static void worker_poll_timer_cb(void* ctx) {
    SpiFlashDumpApp* app = ctx;
    if(!spi_worker_is_running(app->worker) && (app->worker_state == SpiWorkerStateReading ||
                                               app->worker_state == SpiWorkerStateVerifying)) {
        bool result = spi_worker_get_result(app->worker);
        spi_worker_wait(app->worker);
        app->worker_running = false;

        if(app->worker_state == SpiWorkerStateReading) {
            app->worker_state = result ? SpiWorkerStateDone : SpiWorkerStateError;

            /* CRC32 was computed in the worker thread after the read finished,
               so we just retrieve it here instead of blocking the timer daemon
               with a full file re-read. */
            uint32_t crc32 = spi_worker_get_crc32(app->worker);
            bool crc32_valid = spi_worker_has_crc32(app->worker);

            with_view_model(
                app->read_progress_view,
                ReadProgressModel * m,
                {
                    m->finished = true;
                    m->success = result;
                    m->bytes_done = app->progress_bytes;
                    m->crc32 = crc32;
                    m->crc32_valid = crc32_valid;
                },
                true);

            if(result) {
                notification_message(app->notifications, &sequence_success);
            } else {
                notification_message(app->notifications, &sequence_error);
            }
        } else {
            app->worker_state = result ? SpiWorkerStateDone : SpiWorkerStateError;
            with_view_model(
                app->verify_progress_view,
                VerifyProgressModel * m,
                {
                    m->finished = true;
                    m->all_match = result;
                    m->match_count = app->verify_match;
                    m->mismatch_count = app->verify_mismatch;
                    m->bytes_done = app->progress_bytes;
                },
                true);

            if(result) {
                notification_message(app->notifications, &sequence_success);
            } else {
                notification_message(app->notifications, &sequence_error);
            }
        }
    }
}

/* ================================================================== */
/*  Previous view callbacks for ViewDispatcher                        */
/* ================================================================== */

static uint32_t hex_preview_back_cb(void* ctx) {
    UNUSED(ctx);
    return SpiFlashDumpViewChipInfo;
}

static uint32_t progress_back_cb(void* ctx) {
    UNUSED(ctx);
    return SpiFlashDumpViewChipInfo;
}

/* ================================================================== */
/*  App alloc / free / run                                            */
/* ================================================================== */

static SpiFlashDumpApp* spi_flash_dump_app_alloc(void) {
    SpiFlashDumpApp* app = malloc(sizeof(SpiFlashDumpApp));
    furi_assert(app);
    memset(app, 0, sizeof(SpiFlashDumpApp));

    /* Defaults */
    app->spi_speed = SpiSpeedMedium;
    app->verify_after_read = true;
    app->read_cmd = SpiReadCmdNormal;
    app->worker_state = SpiWorkerStateIdle;

    /* Records */
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* SPI worker */
    app->worker = spi_worker_alloc();

    /* ViewDispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    /* view_dispatcher queue enabled by default in SDK 1.4+ */
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_navigation_cb);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_cb);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* ---- Wiring Guide (Widget) ---- */
    app->wiring_guide = widget_alloc();
    wiring_guide_setup(app);
    view_set_input_callback(widget_get_view(app->wiring_guide), wiring_guide_input_cb);
    view_set_context(widget_get_view(app->wiring_guide), app);
    view_dispatcher_add_view(
        app->view_dispatcher, SpiFlashDumpViewWiringGuide, widget_get_view(app->wiring_guide));

    /* ---- Chip Info (Widget) ---- */
    app->chip_info_widget = widget_alloc();
    view_set_input_callback(widget_get_view(app->chip_info_widget), chip_info_input_cb);
    view_set_context(widget_get_view(app->chip_info_widget), app);
    view_dispatcher_add_view(
        app->view_dispatcher, SpiFlashDumpViewChipInfo, widget_get_view(app->chip_info_widget));

    /* ---- Read Progress (custom View) ---- */
    app->read_progress_view = view_alloc();
    view_allocate_model(app->read_progress_view, ViewModelTypeLocking, sizeof(ReadProgressModel));
    view_set_draw_callback(app->read_progress_view, read_progress_draw_cb);
    view_set_input_callback(app->read_progress_view, read_progress_input_cb);
    view_set_context(app->read_progress_view, app);
    view_set_previous_callback(app->read_progress_view, progress_back_cb);
    view_dispatcher_add_view(
        app->view_dispatcher, SpiFlashDumpViewReadProgress, app->read_progress_view);

    /* ---- Verify Progress (custom View) ---- */
    app->verify_progress_view = view_alloc();
    view_allocate_model(
        app->verify_progress_view, ViewModelTypeLocking, sizeof(VerifyProgressModel));
    view_set_draw_callback(app->verify_progress_view, verify_progress_draw_cb);
    view_set_input_callback(app->verify_progress_view, verify_progress_input_cb);
    view_set_context(app->verify_progress_view, app);
    view_set_previous_callback(app->verify_progress_view, progress_back_cb);
    view_dispatcher_add_view(
        app->view_dispatcher, SpiFlashDumpViewVerifyProgress, app->verify_progress_view);

    /* ---- Hex Preview ---- */
    app->hex_viewer = hex_viewer_alloc();
    view_set_previous_callback(hex_viewer_get_view(app->hex_viewer), hex_preview_back_cb);
    view_dispatcher_add_view(
        app->view_dispatcher, SpiFlashDumpViewHexPreview, hex_viewer_get_view(app->hex_viewer));

    /* ---- Settings (VariableItemList) ---- */
    app->settings_list = variable_item_list_alloc();
    settings_list_setup(app);
    variable_item_list_set_enter_callback(app->settings_list, settings_enter_cb, app);
    view_set_previous_callback(variable_item_list_get_view(app->settings_list), settings_back_cb);
    view_dispatcher_add_view(
        app->view_dispatcher,
        SpiFlashDumpViewSettings,
        variable_item_list_get_view(app->settings_list));

    return app;
}

static void spi_flash_dump_app_free(SpiFlashDumpApp* app) {
    /* Worker must be stopped before GPIO deinit to avoid bit-banging
     * analog-mode pins if a read/verify operation is still in flight. */
    spi_worker_free(app->worker);

    /* GPIO cleanup — safe now that worker has stopped */
    spi_worker_gpio_deinit();

    /* Remove views */
    view_dispatcher_remove_view(app->view_dispatcher, SpiFlashDumpViewWiringGuide);
    view_dispatcher_remove_view(app->view_dispatcher, SpiFlashDumpViewChipInfo);
    view_dispatcher_remove_view(app->view_dispatcher, SpiFlashDumpViewReadProgress);
    view_dispatcher_remove_view(app->view_dispatcher, SpiFlashDumpViewVerifyProgress);
    view_dispatcher_remove_view(app->view_dispatcher, SpiFlashDumpViewHexPreview);
    view_dispatcher_remove_view(app->view_dispatcher, SpiFlashDumpViewSettings);

    /* Free views */
    widget_free(app->wiring_guide);
    widget_free(app->chip_info_widget);
    view_free(app->read_progress_view);
    view_free(app->verify_progress_view);
    hex_viewer_free(app->hex_viewer);
    variable_item_list_free(app->settings_list);

    /* ViewDispatcher */
    view_dispatcher_free(app->view_dispatcher);

    /* Records */
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

/* ================================================================== */
/*  Entry point                                                       */
/* ================================================================== */

int32_t spi_flash_dump_app(void* p) {
    UNUSED(p);

    SpiFlashDumpApp* app = spi_flash_dump_app_alloc();

    /* Create a timer to poll worker completion every 250ms */
    FuriTimer* poll_timer = furi_timer_alloc(worker_poll_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(poll_timer, furi_ms_to_ticks(250));

    /* Start on the wiring guide */
    view_dispatcher_switch_to_view(app->view_dispatcher, SpiFlashDumpViewWiringGuide);
    view_dispatcher_run(app->view_dispatcher);

    /* Cleanup */
    furi_timer_stop(poll_timer);
    furi_timer_free(poll_timer);
    spi_flash_dump_app_free(app);

    return 0;
}

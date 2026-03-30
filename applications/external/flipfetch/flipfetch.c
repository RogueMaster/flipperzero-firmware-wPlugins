#include <furi.h>
#include <gui/gui.h>
#include <furi_hal.h>
#include <furi_hal_power.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>

#define TAG        "FlipFetch"
#define REFRESH_MS 2000

typedef struct {
    FuriMutex*        mutex;
    ViewPort*         view_port;
    Gui*              gui;
    FuriMessageQueue* event_queue;
    bool              running;

    char     firmware[24];
    char     build_date[16];
    uint8_t  battery_pct;
    uint16_t battery_mv;
    bool     charging;
    uint32_t uptime_s;
    uint32_t free_heap_k;
    uint32_t sd_free_mb;
} FlipFetchApp;

static void load_sysinfo(FlipFetchApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    const Version* v = furi_hal_version_get_firmware_version();
    if(v) {
        snprintf(app->firmware,    sizeof(app->firmware),    "%s", version_get_version(v));
        snprintf(app->build_date,  sizeof(app->build_date),  "%s", version_get_builddate(v));
    }

    app->battery_pct = furi_hal_power_get_pct();
    app->battery_mv  = (uint16_t)(furi_hal_power_get_battery_voltage(FuriHalPowerICFuelGauge) * 1000);
    app->charging    = furi_hal_power_is_charging();
    app->uptime_s    = furi_get_tick() / 1000;
    app->free_heap_k = memmgr_get_free_heap() / 1024;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    uint64_t total = 0, free_space = 0;
    if(storage_common_fs_info(storage, STORAGE_EXT_PATH_PREFIX, &total, &free_space) == FSE_OK) {
        app->sd_free_mb = (uint32_t)(free_space / (1024 * 1024));
    } else {
        app->sd_free_mb = 0;
    }
    furi_record_close(RECORD_STORAGE);

    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

static void draw_cb(Canvas* canvas, void* ctx) {
    FlipFetchApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);

    // Logo lado izquierdo (40px de ancho)
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1,  9,  "(o_o)");
    canvas_draw_str(canvas, 1, 18,  "/|_|\\");
    canvas_draw_str(canvas, 4, 27,  "| |");
    canvas_draw_str(canvas, 1, 38,  "FLIP");
    canvas_draw_str(canvas, 1, 47,  "PER");

    // Uptime debajo del logo
    uint32_t h = app->uptime_s / 3600;
    uint32_t m = (app->uptime_s % 3600) / 60;
    char upbuf[12];
    snprintf(upbuf, sizeof(upbuf), "%luh%lum", h, m);
    canvas_draw_str(canvas, 1, 58, upbuf);

    // Línea divisoria
    canvas_draw_line(canvas, 41, 0, 41, 64);

    // Título
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 44, 9, "FlipFetch");
    canvas_draw_line(canvas, 41, 11, 128, 11);

    canvas_set_font(canvas, FontSecondary);

    char buf[32];

    // Firmware
    snprintf(buf, sizeof(buf), "FW: %s", app->firmware);
    canvas_draw_str(canvas, 44, 20, buf);

    // Build date
    snprintf(buf, sizeof(buf), "Bld: %s", app->build_date);
    canvas_draw_str(canvas, 44, 29, buf);

    // Batería
    if(app->charging) {
        snprintf(buf, sizeof(buf), "Bat: %d%% +CHG", app->battery_pct);
    } else {
        snprintf(buf, sizeof(buf), "Bat: %d%% %umV", app->battery_pct, app->battery_mv);
    }
    canvas_draw_str(canvas, 44, 38, buf);

    // RAM
    snprintf(buf, sizeof(buf), "RAM: %luk libre", app->free_heap_k);
    canvas_draw_str(canvas, 44, 47, buf);

    // SD
    if(app->sd_free_mb > 0) {
        snprintf(buf, sizeof(buf), "SD:  %luMB libre", app->sd_free_mb);
    } else {
        snprintf(buf, sizeof(buf), "SD:  no SD");
    }
    canvas_draw_str(canvas, 44, 56, buf);

    furi_mutex_release(app->mutex);
}

static void input_cb(InputEvent* event, void* ctx) {
    FlipFetchApp* app = ctx;
    furi_message_queue_put(app->event_queue, event, 0);
}

int32_t flipfetch_app(void* p) {
    UNUSED(p);

    FlipFetchApp* app = malloc(sizeof(FlipFetchApp));
    memset(app, 0, sizeof(FlipFetchApp));
    app->running = true;

    app->mutex       = furi_mutex_alloc(FuriMutexTypeNormal);
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port   = view_port_alloc();

    view_port_draw_callback_set(app->view_port, draw_cb, app);
    view_port_input_callback_set(app->view_port, input_cb, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    load_sysinfo(app);
    uint32_t last_update = furi_get_tick();
    InputEvent event;

    while(app->running) {
        if(furi_get_tick() - last_update >= REFRESH_MS) {
            load_sysinfo(app);
            last_update = furi_get_tick();
        }
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) {
                app->running = false;
            }
        }
    }

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}

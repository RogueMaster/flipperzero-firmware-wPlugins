/*
 * drop_dect_app.c — foreground FAP entry point for the FliPort live drop detector.
 *
 * Loads model.json (auto-deployed to the app's assets dir), starts the shared
 * monitor loop on a worker thread, and shows a small status screen with the live
 * verdict and per-class probabilities. Back exits. The real-time detection +
 * siren logic lives in monitor.c, shared with the background service.
 */
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <storage/storage.h>

#include <stdlib.h>
#include <string.h>

#include "model.h"
#include "monitor.h"

#define TAG            "drop_dect"
#define DROP_THRESHOLD 0.5f

static const char* const CLASS_NAMES[DROP_NUM_CLASSES] = {"idle", "walking", "fidget", "drop"};

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    NotificationApp* notif;
    FuriMessageQueue* input_queue;
    FuriThread* thread;

    DropModel* model;
    bool model_ok;
    MonitorCfg cfg;

    /* Verdict state shared with the draw callback. */
    FuriMutex* mutex;
    int last_cls; /* -1 until the first inference */
    float last_probs[DROP_NUM_CLASSES];
    uint32_t drop_count;
} DropApp;

/* ---------------- UI ---------------- */

static void draw_callback(Canvas* canvas, void* ctx) {
    DropApp* app = ctx;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "FliPort Live");
    canvas_set_font(canvas, FontSecondary);

    if(!app->model_ok) {
        canvas_draw_str(canvas, 2, 26, "model.json missing.");
        canvas_draw_str(canvas, 2, 38, "Place it at:");
        canvas_draw_str(canvas, 2, 50, "apps_assets/drop_detect_live/");
        canvas_draw_str(canvas, 2, 62, "Press Back to exit.");
        return;
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    int cls = app->last_cls;
    float probs[DROP_NUM_CLASSES];
    memcpy(probs, app->last_probs, sizeof(probs));
    uint32_t drops = app->drop_count;
    furi_mutex_release(app->mutex);

    if(cls < 0) {
        canvas_draw_str(canvas, 2, 24, "Monitoring...");
    } else if(cls == DROP_CLASS_DROP) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_box(canvas, 0, 15, 128, 14);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 44, 26, "DROP!");
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
    } else {
        char line[24];
        snprintf(line, sizeof(line), "State: %s", CLASS_NAMES[cls]);
        canvas_draw_str(canvas, 2, 24, line);
    }

    /* Per-class probability bars. */
    for(int i = 0; i < DROP_NUM_CLASSES; i++) {
        int y = 32 + i * 8;
        int w = (int)(probs[i] * 62.0f + 0.5f);
        if(w < 0) w = 0;
        if(w > 62) w = 62;
        canvas_draw_str(canvas, 2, y + 6, CLASS_NAMES[i]);
        canvas_draw_frame(canvas, 46, y, 64, 7);
        if(w > 0) canvas_draw_box(canvas, 46, y, w, 7);
    }

    char footer[24];
    snprintf(footer, sizeof(footer), "drops: %lu", (unsigned long)drops);
    canvas_draw_str(canvas, 2, 62, footer);
}

static void input_callback(InputEvent* event, void* ctx) {
    DropApp* app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

/* Monitor -> UI bridge. Runs on the worker thread. */
static void verdict_callback(int cls, const float probs[4], void* ud) {
    DropApp* app = ud;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->last_cls = cls;
    memcpy(app->last_probs, probs, sizeof(app->last_probs));
    if(cls == DROP_CLASS_DROP && probs[DROP_CLASS_DROP] >= app->cfg.thresh) app->drop_count++;
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

/* ---------------- Alloc / free ---------------- */

static DropApp* drop_app_alloc(void) {
    DropApp* app = malloc(sizeof(DropApp));
    if(!app) return NULL;
    memset(app, 0, sizeof(DropApp));
    app->last_cls = -1;

    app->model = malloc(sizeof(DropModel));
    if(!app->model) {
        free(app);
        return NULL;
    }

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->gui = furi_record_open(RECORD_GUI);
    app->notif = furi_record_open(RECORD_NOTIFICATION);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    return app;
}

static void drop_app_free(DropApp* app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app->model);
    free(app);
}

/* ---------------- Entry point ---------------- */

int32_t drop_dect_app(void* p) {
    UNUSED(p);

    DropApp* app = drop_app_alloc();
    if(!app) return -1;

    app->model_ok = model_load_json(app->model, APP_ASSETS_PATH("model.json"));
    if(!app->model_ok) {
        FURI_LOG_E(TAG, "model load failed");
    } else {
        app->cfg.model = app->model;
        app->cfg.running = true;
        app->cfg.thresh = DROP_THRESHOLD;
        app->cfg.on_verdict = verdict_callback;
        app->cfg.ud = app;
        app->cfg.notif = app->notif;

        app->thread = furi_thread_alloc_ex("DropMonitor", 4096, monitor_run, &app->cfg);
        furi_thread_start(app->thread);
    }
    view_port_update(app->view_port);

    /* Event loop: exit on Back. */
    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app->input_queue, &event, FuriWaitForever) != FuriStatusOk)
            continue;
        if(event.type == InputTypeShort && event.key == InputKeyBack) running = false;
    }

    /* Stop the worker before tearing down anything it touches. */
    if(app->thread) {
        app->cfg.running = false;
        furi_thread_join(app->thread);
        furi_thread_free(app->thread);
        app->thread = NULL;
    }

    drop_app_free(app);
    return 0;
}

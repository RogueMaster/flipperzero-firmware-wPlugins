#include "gta_nfc_plugin.h"
#include <flipper_application/flipper_application.h>
#include "url_nfc.h"
#include "nfc_emulation_bitmap.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <string.h>
#include <stdlib.h>

enum {GtaNfcUrlWidth = 124, GtaNfcScrollPause = 5};
typedef struct {
    const char* url;
    const char* error;
    size_t offset;
    uint8_t pause;
    bool scroll;
    bool at_end;
} GtaNfcViewModel;
typedef struct {
    ViewDispatcher* dispatcher;
    View* view;
    GtaUrlNfc* nfc;
    char url[GTA_URL_MAX];
    FuriEventLoopTimer* timer;
} GtaNfcApp;
static void gta_draw_nfc(Canvas* canvas, void* context) {
    GtaNfcViewModel* model = context;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_xbm(canvas, 0, 0, 128, 64, gta_nfc_emulation_bitmap);
    if(model->error) {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, model->error);
        canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignCenter, "Back: return");
        return;
    }
    if(!model->url) return;

    canvas_set_font(canvas, FontSecondary);
    model->scroll = canvas_string_width(canvas, model->url) > GtaNfcUrlWidth;
    const char* remaining = model->url + model->offset;
    // Fit whole characters into the 124px URL area without allocating on each redraw.
    char line[GtaNfcUrlWidth + 1] = {0};
    size_t length = 0;
    while(length < sizeof(line) - 1 && remaining[length]) {
        line[length] = remaining[length];
        if(canvas_string_width(canvas, line) > GtaNfcUrlWidth) {
            line[length] = '\0';
            break;
        }
        ++length;
    }
    const bool at_end = remaining[length] == '\0';
    if(model->scroll && at_end && !model->at_end) model->pause = GtaNfcScrollPause;
    model->at_end = at_end;
    // Divider is y=52; FontSecondary baseline 61 leaves room for descenders.
    canvas_draw_str(canvas, 2, 61, line);
}

static void gta_nfc_scroll(void* context) {
    GtaNfcApp* app = context;
    if(!app->nfc) return;
    bool redraw = false;
    with_view_model(
        app->view,
        GtaNfcViewModel * model,
        {
            if(model->scroll) {
                if(model->pause) {
                    --model->pause;
                } else if(model->at_end) {
                    model->offset = 0;
                    model->pause = GtaNfcScrollPause;
                    redraw = true;
                } else if(model->url && model->url[model->offset]) {
                    ++model->offset;
                    redraw = true;
                }
            }
        },
        redraw);
}

static bool input(InputEvent* event, void* ctx) {
    GtaNfcApp* app = ctx;
    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        // Only the host may remove this view and unload the executing module.
        view_dispatcher_send_custom_event(
            app->dispatcher,
            event->type == InputTypeShort ? GTA_NFC_BACK_EVENT : GTA_NFC_EXIT_EVENT);
    }
    return true;
}

static void gta_nfc_stop(void* context) {
    GtaNfcApp* app = context;
    if(app->timer) {
        furi_event_loop_timer_stop(app->timer);
        furi_event_loop_timer_free(app->timer);
        app->timer = NULL;
    }
    gta_url_nfc_stop(app->nfc);
    app->nfc = NULL;
}

static void gta_nfc_close(void* context) {
    GtaNfcApp* app = context;
    gta_nfc_stop(app);
    view_dispatcher_remove_view(app->dispatcher, GTA_NFC_VIEW_ID);
    view_free(app->view);
    free(app);
}

static void* gta_nfc_open(ViewDispatcher* dispatcher, const char* url, uint16_t id, GtaUrlKind kind) {
    if(!dispatcher || !url || strlen(url) >= GTA_URL_MAX) return NULL;
    GtaNfcApp* app = calloc(1, sizeof(*app));
    if(!app) return NULL;
    app->dispatcher = dispatcher;
    memcpy(app->url, url, strlen(url) + 1);
    app->nfc = gta_url_nfc_start(app->url, id, kind);
    app->view = view_alloc();
    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(GtaNfcViewModel));
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, gta_draw_nfc);
    view_set_input_callback(app->view, input);
    with_view_model(app->view, GtaNfcViewModel * model, {
        *model = ((GtaNfcViewModel){.url = app->url, .pause = GtaNfcScrollPause,
            .error = app->nfc ? NULL : "Unable to start NFC"});
    }, false);
    app->timer = furi_event_loop_timer_alloc(
        view_dispatcher_get_event_loop(dispatcher), gta_nfc_scroll,
        FuriEventLoopTimerTypePeriodic, app);
    view_dispatcher_add_view(dispatcher, GTA_NFC_VIEW_ID, app->view);
    view_dispatcher_switch_to_view(dispatcher, GTA_NFC_VIEW_ID);
    if(app->nfc) furi_event_loop_timer_start(app->timer, furi_ms_to_ticks(200));
    return app;
}

static const GtaNfcPluginApi api = {sizeof(GtaNfcPluginApi), gta_nfc_open, gta_nfc_stop, gta_nfc_close};
static const FlipperAppPluginDescriptor descriptor = {GTA_NFC_ID, GTA_NFC_API_VERSION, &api};
const FlipperAppPluginDescriptor* gta_nfc_link_ep(void) { return &descriptor; }

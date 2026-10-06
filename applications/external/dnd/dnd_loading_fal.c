#include "dnd_loading_api.h"
#include "dnd_splash_image.h"
#include <assets_icons.h>
#include <flipper_application/flipper_application.h>
#include <gui/icon_animation.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

typedef struct {
    View* view;
    IconAnimation* hourglass;
    DndSplashImage* image;
} DndLoading;
static void
    dnd_loading_paint(Canvas* canvas, IconAnimation* hourglass, const DndSplashImage* image) {
    canvas_clear(canvas);
    dnd_splash_image_draw(canvas, image);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 100, 36, 28, 28);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 100, 36, 28, 28);
    canvas_draw_icon(canvas, 102, 38, &A_Loading_24);
    canvas_draw_icon_animation(canvas, 102, 38, hourglass);
}
static void dnd_loading_draw(Canvas* canvas, void* model) {
    DndLoading* loading = *(DndLoading**)model;
    dnd_loading_paint(canvas, loading->hourglass, loading->image);
}
static bool dnd_loading_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return true;
}
static void dnd_loading_enter(void* context) {
    DndLoading* loading = context;
    view_tie_icon_animation(loading->view, loading->hourglass);
    icon_animation_start(loading->hourglass);
}
static void dnd_loading_exit(void* context) {
    DndLoading* loading = context;
    icon_animation_stop(loading->hourglass);
}
static void dnd_loading_free(void* context) {
    DndLoading* loading = context;
    if(!loading) return;
    if(loading->hourglass) {
        icon_animation_stop(loading->hourglass);
        /* Native free drains the timer queue before its borrowed View is freed. */
        icon_animation_free(loading->hourglass);
    }
    if(loading->view) view_free(loading->view);
    dnd_splash_image_release(loading->image);
    free(loading);
}
static void* dnd_loading_alloc(void) {
    DndLoading* loading = calloc(1, sizeof(DndLoading));
    if(!loading) return NULL;
    loading->view = view_alloc();
    if(!loading->view) goto fail;
    view_allocate_model(loading->view, ViewModelTypeLockFree, sizeof(DndLoading*));
    DndLoading** model = view_get_model(loading->view);
    if(!model) goto fail;
    *model = loading;
    loading->hourglass = icon_animation_alloc(&A_Loading_24);
    if(!loading->hourglass) goto fail;
    loading->image = dnd_splash_image_acquire(DND_SPLASH_IMAGE_FAL_ROOT);
    view_set_context(loading->view, loading);
    view_set_draw_callback(loading->view, dnd_loading_draw);
    view_set_input_callback(loading->view, dnd_loading_input);
    view_set_enter_callback(loading->view, dnd_loading_enter);
    view_set_exit_callback(loading->view, dnd_loading_exit);
    return loading;
fail:
    dnd_loading_free(loading);
    return NULL;
}
static View* dnd_loading_get_view(void* context) {
    DndLoading* loading = context;
    return loading ? loading->view : NULL;
}

#define DND_HANDOFF_TIMEOUT_MS 10000U
typedef struct {
    Gui* gui;
    Canvas* canvas;
    IconAnimation* hourglass;
    DndSplashImage* image;
    FuriMutex* mutex;
    FuriPubSub* events;
    FuriPubSubSubscription* subscription;
    uint32_t started;
    bool active;
    bool incoming;
} DndLoadingHandoff;

/* Called under our mutex. Public direct draw leaves the GUI mutex unlocked,
 * so incoming apps can attach their normal views while this FAL owns drawing. */
static void dnd_loading_handoff_release(DndLoadingHandoff* handoff) {
    if(!handoff->active) return;
    handoff->active = false;
    gui_direct_draw_release(handoff->gui);
    handoff->canvas = NULL;
}
static void dnd_loading_handoff_update(IconAnimation* animation, void* context) {
    DndLoadingHandoff* handoff = context;
    furi_check(furi_mutex_acquire(handoff->mutex, FuriWaitForever) == FuriStatusOk);
    if(handoff->active) {
        if(furi_get_tick() - handoff->started >= furi_ms_to_ticks(DND_HANDOFF_TIMEOUT_MS)) {
            dnd_loading_handoff_release(handoff);
        } else {
            dnd_loading_paint(handoff->canvas, animation, handoff->image);
            canvas_commit(handoff->canvas);
        }
    }
    furi_check(furi_mutex_release(handoff->mutex) == FuriStatusOk);
}
static void dnd_loading_handoff_event(const void* message, void* context) {
    const LoaderEvent* event = message;
    DndLoadingHandoff* handoff = context;
    furi_check(furi_mutex_acquire(handoff->mutex, FuriWaitForever) == FuriStatusOk);
    if(event->type == LoaderEventTypeApplicationBeforeLoad)
        handoff->incoming = true;
    else if(
        event->type == LoaderEventTypeApplicationLoadFailed ||
        event->type == LoaderEventTypeNoMoreAppsInQueue ||
        (event->type == LoaderEventTypeApplicationStopped && handoff->incoming))
        dnd_loading_handoff_release(handoff);
    furi_check(furi_mutex_release(handoff->mutex) == FuriStatusOk);
}
static void dnd_loading_handoff_end(void* context) {
    DndLoadingHandoff* handoff = context;
    if(!handoff) return;
    /* Never free or unmap from a timer/Loader callback. Unsubscribe and native
     * timer delete/flush finish those callbacks before their context is freed. */
    if(handoff->subscription) furi_pubsub_unsubscribe(handoff->events, handoff->subscription);
    if(handoff->hourglass) {
        icon_animation_stop(handoff->hourglass);
        icon_animation_free(handoff->hourglass);
    }
    if(handoff->mutex) {
        furi_check(furi_mutex_acquire(handoff->mutex, FuriWaitForever) == FuriStatusOk);
        dnd_loading_handoff_release(handoff);
        furi_check(furi_mutex_release(handoff->mutex) == FuriStatusOk);
        furi_mutex_free(handoff->mutex);
    }
    dnd_splash_image_release(handoff->image);
    if(handoff->gui) furi_record_close(RECORD_GUI);
    free(handoff);
}
static void* dnd_loading_handoff_begin(Loader* loader) {
    DndLoadingHandoff* handoff = calloc(1, sizeof(DndLoadingHandoff));
    if(!handoff) return NULL;
    handoff->gui = furi_record_open(RECORD_GUI);
    if(!handoff->gui) goto fail;
    handoff->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!handoff->mutex) goto fail;
    handoff->hourglass = icon_animation_alloc(&A_Loading_24);
    if(!handoff->hourglass) goto fail;
    handoff->image = dnd_splash_image_acquire(DND_SPLASH_IMAGE_FAL_ROOT);
    handoff->events = loader_get_pubsub(loader);
    handoff->subscription =
        furi_pubsub_subscribe(handoff->events, dnd_loading_handoff_event, handoff);
    if(!handoff->subscription) goto fail;
    icon_animation_set_update_callback(handoff->hourglass, dnd_loading_handoff_update, handoff);
    /* Publish active state and the Canvas while callbacks are excluded. */
    furi_check(furi_mutex_acquire(handoff->mutex, FuriWaitForever) == FuriStatusOk);
    handoff->canvas = gui_direct_draw_acquire(handoff->gui);
    handoff->started = furi_get_tick();
    handoff->active = true;
    dnd_loading_paint(handoff->canvas, handoff->hourglass, handoff->image);
    canvas_commit(handoff->canvas);
    furi_check(furi_mutex_release(handoff->mutex) == FuriStatusOk);
    icon_animation_start(handoff->hourglass);
    return handoff;
fail:
    dnd_loading_handoff_end(handoff);
    return NULL;
}
static bool dnd_loading_handoff_active(void* context) {
    DndLoadingHandoff* handoff = context;
    furi_check(furi_mutex_acquire(handoff->mutex, FuriWaitForever) == FuriStatusOk);
    bool active = handoff->active;
    furi_check(furi_mutex_release(handoff->mutex) == FuriStatusOk);
    return active;
}
static const DndLoadingApi dnd_loading_api = {
    sizeof(DndLoadingApi),
    dnd_loading_alloc,
    dnd_loading_get_view,
    dnd_loading_free,
    dnd_loading_handoff_begin,
    dnd_loading_handoff_active,
    dnd_loading_handoff_end,
};
static const FlipperAppPluginDescriptor dnd_loading_descriptor = {
    DND_LOADING_API_ID,
    DND_LOADING_API_VERSION,
    &dnd_loading_api};
const FlipperAppPluginDescriptor* dnd_loading_ep(void) {
    return &dnd_loading_descriptor;
}

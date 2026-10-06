#include "dndolphins_splash.h"
#include "dnd_splash_image.h"
#include <assets_icons.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/icon_animation.h>
#include <stdlib.h>
#define DNDOLPHINS_SPLASH_DURATION_MS 2000U
struct DndSplash {
    Gui* gui;
    ViewPort* view_port;
    IconAnimation* hourglass;
    DndSplashImage* image;
    uint32_t started;
    bool introduction;
};
static void dndolphins_splash_draw(Canvas* canvas, void* context) {
    DndSplash* splash = context;
    canvas_clear(canvas);
    dnd_splash_image_draw(canvas, splash->image);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 100, 36, 28, 28);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 100, 36, 28, 28);
    canvas_draw_icon(canvas, 102, 38, &A_Loading_24);
    canvas_draw_icon_animation(canvas, 102, 38, splash->hourglass);
}
static void dndolphins_splash_update(IconAnimation* animation, void* context) {
    UNUSED(animation);
    DndSplash* splash = context;
    view_port_update(splash->view_port);
}
DndSplash* dndolphins_splash_begin(bool introduction) {
    DndSplash* splash = calloc(1, sizeof(DndSplash));
    if(!splash) return NULL;
    splash->gui = furi_record_open(RECORD_GUI);
    if(!splash->gui) goto fail;
    splash->view_port = view_port_alloc();
    if(!splash->view_port) goto fail;
    splash->hourglass = icon_animation_alloc(&A_Loading_24);
    if(!splash->hourglass) goto fail;
    splash->image = dnd_splash_image_acquire(DND_SPLASH_IMAGE_HUB_ROOT);
    splash->started = furi_get_tick();
    splash->introduction = introduction;
    view_port_draw_callback_set(splash->view_port, dndolphins_splash_draw, splash);
    icon_animation_set_update_callback(splash->hourglass, dndolphins_splash_update, splash);
    gui_add_view_port(splash->gui, splash->view_port, GuiLayerFullscreen);
    icon_animation_start(splash->hourglass);
    view_port_update(splash->view_port);
    return splash;
fail:
    dndolphins_splash_end(splash);
    return NULL;
}
void dndolphins_splash_wait(DndSplash* splash) {
    if(!splash || !splash->introduction) return;
    uint32_t elapsed = furi_get_tick() - splash->started;
    uint32_t minimum = furi_ms_to_ticks(DNDOLPHINS_SPLASH_DURATION_MS);
    if(elapsed < minimum) furi_delay_tick(minimum - elapsed);
}
void dndolphins_splash_end(DndSplash* splash) {
    if(!splash) return;
    if(splash->hourglass) {
        icon_animation_stop(splash->hourglass);
    }
    if(splash->view_port && splash->gui) gui_remove_view_port(splash->gui, splash->view_port);
    /* Keep the callback and its context alive until native free drains the timer queue. */
    if(splash->hourglass) icon_animation_free(splash->hourglass);
    if(splash->view_port) view_port_free(splash->view_port);
    dnd_splash_image_release(splash->image);
    if(splash->gui) furi_record_close(RECORD_GUI);
    free(splash);
}

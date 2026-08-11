/*
 * scene_splash — the animated intro.
 *
 * A node pulses signal rings outward while the name fades in, for a couple of
 * seconds, then it hands off to the menu. Any key skips it. The standard GUI
 * modules redraw on a 500 ms scene tick, too slow for animation, so this owns a
 * hand-drawn View and a ~30 fps timer that nudges the frame and asks for a
 * redraw.
 *
 * Only shown on a normal launch: a launch argument (headless testing) skips
 * straight to its scene, so the splash never gets in the way of a script.
 */
#include <gui/canvas.h>

#include "../meshcore_cfg.h"

#define MESHCORE_SPLASH_FPS_MS 33u
/* ~2 s at 33 ms a frame. */
#define MESHCORE_SPLASH_FRAMES 60u
#define MESHCORE_SPLASH_EVENT_DONE 0x470u

typedef struct {
    uint32_t frame;
} MeshCoreSplashModel;

/* The drawing. Centre a filled node, expand two phase-shifted rings out of it,
 * and title underneath. Monochrome: a ring is just a circle that grows and
 * wraps, which reads as a pulse. */
static void meshcore_splash_draw(Canvas* canvas, void* model) {
    MeshCoreSplashModel* m = model;
    canvas_clear(canvas);

    const int cx = 64;
    const int cy = 24;

    /* Two rings, half a cycle apart, so there is always one on the way out. */
    for(uint32_t phase = 0; phase < 24; phase += 12) {
        int r = (int)(((m->frame + phase) % 24u) * 11u / 10u); /* 0..~25 */
        if(r > 4 && r < 26) canvas_draw_circle(canvas, cx, cy, r);
    }

    /* The node: a filled disc with a hole, so it reads as a ring node rather
     * than a blob, matching the app icon. */
    canvas_draw_disc(canvas, cx, cy, 3);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, cx, cy);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, cx, 46, AlignCenter, AlignTop, "MeshCore");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, cx, 57, AlignCenter, AlignTop, "configurator");
}

/* Any press skips to the menu. */
static bool meshcore_splash_input(InputEvent* event, void* context) {
    MeshCoreApp* app = context;
    if(event->type == InputTypeShort || event->type == InputTypePress) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_SPLASH_EVENT_DONE);
        return true;
    }
    return false;
}

/* Timer thread: advance one frame, ask for a redraw, and end the splash once it
 * has run its length. */
static void meshcore_splash_tick(void* context) {
    MeshCoreApp* app = context;
    bool done = false;

    with_view_model(
        app->splash_view,
        MeshCoreSplashModel * model,
        {
            model->frame++;
            done = model->frame >= MESHCORE_SPLASH_FRAMES;
        },
        true);

    if(done) view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_SPLASH_EVENT_DONE);
}

View* meshcore_scene_splash_view_alloc(MeshCoreApp* app) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(MeshCoreSplashModel));
    view_set_context(view, app);
    view_set_draw_callback(view, meshcore_splash_draw);
    view_set_input_callback(view, meshcore_splash_input);
    return view;
}

void meshcore_scene_splash_on_enter(void* context) {
    MeshCoreApp* app = context;

    with_view_model(
        app->splash_view, MeshCoreSplashModel * model, { model->frame = 0; }, false);

    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewSplash);

    app->splash_timer =
        furi_timer_alloc(meshcore_splash_tick, FuriTimerTypePeriodic, app);
    furi_timer_start(app->splash_timer, furi_ms_to_ticks(MESHCORE_SPLASH_FPS_MS));
}

bool meshcore_scene_splash_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == MESHCORE_SPLASH_EVENT_DONE) {
        /* Pop the splash; the menu it was pushed on top of becomes active. */
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void meshcore_scene_splash_on_exit(void* context) {
    MeshCoreApp* app = context;

    if(app->splash_timer) {
        furi_timer_stop(app->splash_timer);
        furi_timer_free(app->splash_timer);
        app->splash_timer = NULL;
    }
}

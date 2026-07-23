#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define IMAGE_STRIDE  (SCREEN_WIDTH / 8)
#define IMAGE_SIZE    (IMAGE_STRIDE * SCREEN_HEIGHT)

// The picture is computed in two passes: one sample per COARSE_STEP x COARSE_STEP block
// first, so something appears almost immediately, then at full resolution, published
// every REFINE_ROWS scanlines so the refinement is visible as it happens.
#define COARSE_STEP 4
#define REFINE_ROWS 16

// Home view: the complete set, filling the height of the 2:1 screen.
#define HOME_CENTER_RE (-0.7f)
#define HOME_CENTER_IM (0.0f)
#define HOME_SPAN      (4.2f)

// One zoom step halves the visible span. Past 2^14 the float mantissa runs out:
// neighbouring pixels start landing on the same complex number and the picture breaks
// up into blocks instead of showing more detail.
#define ZOOM_LEVEL_MAX 14

// Deeper views need a bigger iteration budget, otherwise everything inside the set
// merges into one solid blob.
#define ITERATIONS_BASE      40
#define ITERATIONS_PER_LEVEL 12

// An eighth of the screen per step, so panning stays proportional at every zoom level.
#define PAN_STEP_X (SCREEN_WIDTH / 8)
#define PAN_STEP_Y (SCREEN_HEIGHT / 8)

// Keep the view within reach of the set, so panning cannot strand the user in an
// endless empty plane with no way back.
#define CENTER_RE_MIN (-2.5f)
#define CENTER_RE_MAX (1.5f)
#define CENTER_IM_MIN (-1.5f)
#define CENTER_IM_MAX (1.5f)

#define HINT_DURATION_MS 2000
#define HINT_STARTUP     "Hold OK to zoom out"

typedef struct {
    FuriMessageQueue* queue;
    ViewPort* view_port;
    FuriMutex* mutex;

    // Owned by the app thread
    float center_re;
    float center_im;
    uint8_t zoom_level;
    bool redraw;
    bool running;
    bool hint_shown;
    uint32_t hint_expiry;
    uint8_t work[IMAGE_SIZE];

    // Read by the draw callback on the GUI thread, guarded by mutex
    uint8_t image[IMAGE_SIZE];
    char hint[24];
} Mandelbrot;

static uint16_t mandelbrot_iterations(const Mandelbrot* app) {
    return ITERATIONS_BASE + app->zoom_level * ITERATIONS_PER_LEVEL;
}

/** Size of one pixel in the complex plane. Equal on both axes, so the picture keeps its
 * shape instead of being stretched to the screen aspect ratio. */
static float mandelbrot_scale(const Mandelbrot* app) {
    return HOME_SPAN / (float)((uint32_t)SCREEN_WIDTH << app->zoom_level);
}

/** Escape time test for a single point: true when the orbit is still bounded after the
 * whole iteration budget, which is what gets painted black. */
static bool mandelbrot_inside(float re, float im, uint16_t max_iterations) {
    // The main cardioid and the period-2 bulb are the two largest solid regions and
    // always cost the full budget, so test for them directly instead of iterating.
    const float dre = re - 0.25f;
    const float q = dre * dre + im * im;
    if(q * (q + dre) <= 0.25f * im * im) return true;
    const float bulb = re + 1.0f;
    if(bulb * bulb + im * im <= 0.0625f) return true;

    float zr = 0.0f;
    float zi = 0.0f;
    float zr2 = 0.0f;
    float zi2 = 0.0f;
    uint16_t iteration = 0;

    while(zr2 + zi2 <= 4.0f && iteration < max_iterations) {
        zi = 2.0f * zr * zi + im;
        zr = zr2 - zi2 + re;
        zr2 = zr * zr;
        zi2 = zi * zi;
        iteration++;
    }

    return iteration == max_iterations;
}

/** Set or clear a COARSE_STEP wide square of the coarse pass. Blocks are aligned to
 * their own width, so a row of one never straddles a byte boundary. */
static void mandelbrot_fill_block(uint8_t* image, uint8_t x, uint8_t y, bool inside) {
    const uint8_t mask = (uint8_t)(((1u << COARSE_STEP) - 1u) << (x % 8));

    for(uint8_t row = 0; row < COARSE_STEP; row++) {
        uint8_t* byte = image + (size_t)(y + row) * IMAGE_STRIDE + x / 8;
        if(inside) {
            *byte |= mask;
        } else {
            *byte &= (uint8_t)~mask;
        }
    }
}

/** Hand the work buffer to the draw callback and ask for a repaint. */
static void mandelbrot_publish(Mandelbrot* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    memcpy(app->image, app->work, IMAGE_SIZE);
    furi_mutex_release(app->mutex);
    view_port_update(app->view_port);
}

/** Show a message at the bottom of the screen for a moment. */
static void mandelbrot_hint(Mandelbrot* app, const char* text) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    snprintf(app->hint, sizeof(app->hint), "%s", text);
    furi_mutex_release(app->mutex);

    app->hint_shown = true;
    app->hint_expiry = furi_get_tick() + furi_ms_to_ticks(HINT_DURATION_MS);
    view_port_update(app->view_port);
}

static void mandelbrot_hint_hide(Mandelbrot* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->hint[0] = '\0';
    furi_mutex_release(app->mutex);

    app->hint_shown = false;
    view_port_update(app->view_port);
}

static void mandelbrot_pan(Mandelbrot* app, int8_t steps_re, int8_t steps_im) {
    const float scale = mandelbrot_scale(app);
    const float re =
        CLAMP(app->center_re + steps_re * (PAN_STEP_X * scale), CENTER_RE_MAX, CENTER_RE_MIN);
    const float im =
        CLAMP(app->center_im + steps_im * (PAN_STEP_Y * scale), CENTER_IM_MAX, CENTER_IM_MIN);

    if(re != app->center_re || im != app->center_im) {
        app->center_re = re;
        app->center_im = im;
        app->redraw = true;
    }
}

/** Zoom one step around the middle of the screen, which is the point the user is
 * looking at. The centre never moves, only the span around it changes. */
static void mandelbrot_zoom(Mandelbrot* app, int8_t direction) {
    if(direction > 0 && app->zoom_level < ZOOM_LEVEL_MAX) {
        app->zoom_level++;
        app->redraw = true;
    } else if(direction < 0 && app->zoom_level > 0) {
        app->zoom_level--;
        app->redraw = true;
    }

    // Also reported when the step was refused, so that hitting either limit is visible
    // instead of looking like the app stopped reacting.
    char text[sizeof(app->hint)];
    snprintf(
        text,
        sizeof(text),
        "x%lu%s",
        (unsigned long)(1UL << app->zoom_level),
        app->zoom_level == ZOOM_LEVEL_MAX ? " max" : "");
    mandelbrot_hint(app, text);
}

static void mandelbrot_handle_input(Mandelbrot* app, const InputEvent* event) {
    // Arrows react to press, long and repeat, so holding a direction keeps panning.
    const bool held = event->type == InputTypePress || event->type == InputTypeLong ||
                      event->type == InputTypeRepeat;

    switch(event->key) {
    case InputKeyUp:
        if(held) mandelbrot_pan(app, 0, 1);
        break;
    case InputKeyDown:
        if(held) mandelbrot_pan(app, 0, -1);
        break;
    case InputKeyLeft:
        if(held) mandelbrot_pan(app, -1, 0);
        break;
    case InputKeyRight:
        if(held) mandelbrot_pan(app, 1, 0);
        break;
    case InputKeyOk:
        // A tap zooms in, holding Ok zooms back out.
        if(event->type == InputTypeShort) {
            mandelbrot_zoom(app, 1);
        } else if(event->type == InputTypeLong || event->type == InputTypeRepeat) {
            mandelbrot_zoom(app, -1);
        }
        break;
    case InputKeyBack:
        if(event->type == InputTypePress) app->running = false;
        break;
    default:
        break;
    }
}

/** Apply everything the user has pressed so far. Returns true once the picture being
 * computed is out of date, which is the signal to abandon it and start over. */
static bool mandelbrot_poll_input(Mandelbrot* app) {
    InputEvent event;

    while(furi_message_queue_get(app->queue, &event, 0) == FuriStatusOk) {
        mandelbrot_handle_input(app, &event);
    }

    return app->redraw || !app->running;
}

/** Compute the visible window on the app thread, checking for input between scanlines
 * so that a slow picture can never hold up the GUI service or the event queue. */
static void mandelbrot_render(Mandelbrot* app) {
    const uint16_t max_iterations = mandelbrot_iterations(app);
    const float scale = mandelbrot_scale(app);
    const float re_origin = app->center_re - (SCREEN_WIDTH / 2 - 0.5f) * scale;
    const float im_origin = app->center_im + (SCREEN_HEIGHT / 2 - 0.5f) * scale;

    for(uint8_t y = 0; y < SCREEN_HEIGHT; y += COARSE_STEP) {
        const float im = im_origin - (y + COARSE_STEP / 2) * scale;
        for(uint8_t x = 0; x < SCREEN_WIDTH; x += COARSE_STEP) {
            const float re = re_origin + (x + COARSE_STEP / 2) * scale;
            mandelbrot_fill_block(app->work, x, y, mandelbrot_inside(re, im, max_iterations));
        }
        if(mandelbrot_poll_input(app)) return;
    }
    mandelbrot_publish(app);

    for(uint8_t y = 0; y < SCREEN_HEIGHT; y++) {
        uint8_t* row = app->work + (size_t)y * IMAGE_STRIDE;
        const float im = im_origin - y * scale;

        for(uint8_t byte = 0; byte < IMAGE_STRIDE; byte++) {
            uint8_t bits = 0;
            for(uint8_t bit = 0; bit < 8; bit++) {
                const float re = re_origin + (byte * 8 + bit) * scale;
                if(mandelbrot_inside(re, im, max_iterations)) bits |= 1u << bit;
            }
            row[byte] = bits;
        }

        if(mandelbrot_poll_input(app)) return;
        if((y + 1) % REFINE_ROWS == 0 || y + 1 == SCREEN_HEIGHT) mandelbrot_publish(app);
    }
}

static void mandelbrot_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    Mandelbrot* app = context;
    char hint[sizeof(app->hint)];

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_draw_xbm(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, app->image);
    memcpy(hint, app->hint, sizeof(hint));
    furi_mutex_release(app->mutex);

    if(hint[0] != '\0') {
        canvas_set_font(canvas, FontSecondary);
        const size_t box_height = canvas_current_font_height(canvas) + 3;
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(
            canvas,
            0,
            SCREEN_HEIGHT - box_height,
            canvas_string_width(canvas, hint) + 4,
            box_height);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 2, SCREEN_HEIGHT - 3, hint);
    }
}

static void mandelbrot_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
    FuriMessageQueue* queue = context;

    // Runs on the GUI service thread, so it must never wait: a backlog of pan steps is
    // worth far less than keeping the rest of the system responsive. Nothing here tracks
    // which keys are down, so a dropped event only costs one step.
    furi_message_queue_put(queue, input_event, 0);
}

int32_t mandelbrot_app(void* p) {
    UNUSED(p);

    Mandelbrot* app = malloc(sizeof(Mandelbrot));
    memset(app, 0, sizeof(Mandelbrot));
    app->center_re = HOME_CENTER_RE;
    app->center_im = HOME_CENTER_IM;
    app->running = true;
    app->redraw = true;
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    ViewPort* view_port = view_port_alloc();
    app->view_port = view_port;
    view_port_draw_callback_set(view_port, mandelbrot_draw_callback, app);
    view_port_input_callback_set(view_port, mandelbrot_input_callback, app->queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    mandelbrot_hint(app, HINT_STARTUP);

    while(app->running) {
        if(app->redraw) {
            app->redraw = false;
            mandelbrot_render(app);
            continue;
        }

        // Sleep until the next key press, or until the hint on screen has had its time.
        uint32_t timeout = FuriWaitForever;
        if(app->hint_shown) {
            const int32_t remaining = (int32_t)(app->hint_expiry - furi_get_tick());
            timeout = remaining > 0 ? (uint32_t)remaining : 0;
        }

        InputEvent event;
        if(furi_message_queue_get(app->queue, &event, timeout) == FuriStatusOk) {
            mandelbrot_handle_input(app, &event);
        }

        if(app->hint_shown && (int32_t)(furi_get_tick() - app->hint_expiry) >= 0) {
            mandelbrot_hint_hide(app);
        }
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}

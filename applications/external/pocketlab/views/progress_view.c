#include "progress_view.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <input/input.h>
#include <string.h>

#include "../helpers/pocketlab_fonts.h"
#include "../helpers/pocketlab_i18n.h"
#include "../helpers/pocketlab_sound.h"

#define PROGRESS_ANIM_MS    40 // matches the coffee page so the glitch feels the same
#define POCKETLAB_XP_MAX_XP 9999 // must match POCKETLAB_XP_MAX
#define PROGRESS_EGG_TEST   0 // TEST: unlock the XP glitch at any XP. Set 0 for production.

// Per-character glitch, exactly like the coffee page: up to 4 chars scrambled at
// once into these glyphs, in bursts, with the same glitch sounds.
#define PROGRESS_GLITCH_SLOTS 4
static const char progress_glitch_glyphs[] = "#@%&$*?<>+=/\\~";

typedef struct {
    uint8_t pos; // digit index being scrambled
    char ch; // glyph shown in its place
    uint8_t ttl; // frames remaining
} ProgressGlitch;

struct ProgressView {
    View* view;
    FuriTimer* timer;
    ProgressViewCallback callback; // Right -> badges
    void* context;
    NotificationApp* notifications;
    bool sound_enabled;
};

typedef struct {
    uint32_t level;
    uint32_t xp;
    uint32_t labs_done;
    uint32_t labs_total;
    uint32_t streak;
    const char* level_title;
    uint32_t anim;
    ProgressGlitch glitch[PROGRESS_GLITCH_SLOTS];
    uint8_t glitch_burst; // frames left in the current glitch burst
} ProgressModel;

// A stat card: the value on a filled black panel, the label in an outlined
// panel below it, both sharing one rounded card.
static void progress_view_draw_card(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t num_h,
    const char* value,
    const char* label) {
    const uint8_t cx = x + w / 2;
    const uint8_t lbl_h = 14; // taller light panel; the text stays vertically centred
    const uint8_t r = 4; // match the home menu tile radius for a consistent style

    // Value: filled black panel, rounded only on the TOP corners.
    canvas_draw_rbox(canvas, x, y, w, num_h, r);
    canvas_draw_box(canvas, x, y + num_h - r, w, r); // square off the bottom
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, cx, y + num_h / 2, AlignCenter, AlignCenter, value);
    canvas_set_color(canvas, ColorBlack);

    // Label: outlined panel butted underneath, rounded only on the BOTTOM corners.
    const uint8_t ly = y + num_h;
    canvas_draw_rframe(canvas, x, ly, w, lbl_h, r);
    canvas_draw_line(canvas, x, ly, x + w - 1, ly); // straight top edge
    canvas_draw_line(canvas, x, ly, x, ly + r); // square top-left corner
    canvas_draw_line(canvas, x + w - 1, ly, x + w - 1, ly + r); // square top-right corner
    pocketlab_font_apply_small(canvas);
    canvas_draw_str_aligned(canvas, cx, ly + lbl_h / 2, AlignCenter, AlignCenter, label);
}

// The XP counter goes glitchy once XP is maxed out.
static bool progress_xp_glitch_active(uint32_t xp) {
    return PROGRESS_EGG_TEST || xp >= POCKETLAB_XP_MAX_XP;
}

// Draw the XP value on its panel with the coffee-page glitch: individual digits
// briefly replaced by glitch glyphs. Uses the same big FontBigNumbers as the
// level number (profont22, which carries the symbol glyphs), so the size is
// unchanged.
static void progress_draw_xp_glitched(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t num_h,
    const char* value,
    const ProgressGlitch* glitch) {
    char g[12];
    strncpy(g, value, sizeof(g) - 1);
    g[sizeof(g) - 1] = '\0';
    const size_t len = strlen(g);
    for(uint8_t i = 0; i < PROGRESS_GLITCH_SLOTS; i++) {
        if(glitch[i].ttl > 0 && glitch[i].pos < len) g[glitch[i].pos] = glitch[i].ch;
    }
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, x + w / 2, y + num_h / 2, AlignCenter, AlignCenter, g);
    canvas_set_color(canvas, ColorBlack);
}

static void progress_view_draw_callback(Canvas* canvas, void* context) {
    ProgressModel* model = context;
    canvas_clear(canvas);

    pocketlab_font_apply(canvas, true);
    canvas_draw_str(canvas, 0, 10, pocketlab_text(PocketLabTextMenuProgress));

    if(model->streak > 0) {
        char streak[32];
        snprintf(
            streak,
            sizeof(streak),
            "%s: %lu",
            pocketlab_text(PocketLabTextStreak),
            (unsigned long)model->streak);
        pocketlab_font_apply_small(canvas);
        canvas_draw_str_aligned(canvas, 127, 9, AlignRight, AlignBottom, streak);
    }

    char value[12];
    snprintf(value, sizeof(value), "%lu", (unsigned long)model->level);
    progress_view_draw_card(canvas, 0, 13, 61, 22, value, model->level_title);

    snprintf(value, sizeof(value), "%lu", (unsigned long)model->xp);
    if(progress_xp_glitch_active(model->xp)) {
        progress_view_draw_card(canvas, 67, 13, 61, 22, "", "XP"); // panel + label
        progress_draw_xp_glitched(canvas, 67, 13, 61, 22, value, model->glitch);
    } else {
        progress_view_draw_card(canvas, 67, 13, 61, 22, value, "XP");
    }

    pocketlab_font_apply_small(canvas);
    char line[28];
    snprintf(
        line,
        sizeof(line),
        "%s: %lu/%lu",
        pocketlab_text(PocketLabTextLabsWord),
        (unsigned long)model->labs_done,
        (unsigned long)model->labs_total);
    canvas_draw_str(canvas, 0, 61, line);

    // Blinking hint that Right opens the badge gallery.
    if((model->anim / 13) % 2 == 0) { // ~0.5s blink at the 40ms tick
        char badges[20];
        snprintf(badges, sizeof(badges), "%s >", pocketlab_text(PocketLabTextBadges));
        canvas_draw_str_aligned(canvas, 127, 61, AlignRight, AlignBottom, badges);
    }
}

static bool progress_view_input_callback(InputEvent* event, void* context) {
    ProgressView* instance = context;
    if(event->type == InputTypeShort && event->key == InputKeyRight) {
        if(instance->callback) {
            instance->callback(instance->context);
        }
        return true;
    }

    return false;
}

static void progress_view_timer_callback(void* context) {
    ProgressView* instance = context;
    bool play_glitch = false;
    uint8_t glitch_variant = 0;
    with_view_model(
        instance->view,
        ProgressModel * model,
        {
            model->anim++;

            // Per-character glitch, identical to the coffee page: age the active
            // slots, then in bursts fill a free slot with a scrambled digit.
            for(uint8_t i = 0; i < PROGRESS_GLITCH_SLOTS; i++) {
                if(model->glitch[i].ttl > 0) model->glitch[i].ttl--;
            }
            bool spawn = false;
            if(progress_xp_glitch_active(model->xp)) {
                if(model->glitch_burst > 0) {
                    model->glitch_burst--;
                    spawn = (furi_hal_random_get() % 2) == 0; // dense during a burst
                } else if((furi_hal_random_get() % 25) == 0) {
                    model->glitch_burst = (uint8_t)(6 + furi_hal_random_get() % 18);
                }
            }
            if(spawn) {
                uint32_t v = model->xp;
                uint8_t len = 1;
                while(v >= 10) {
                    v /= 10;
                    len++;
                }
                for(uint8_t i = 0; i < PROGRESS_GLITCH_SLOTS; i++) {
                    if(model->glitch[i].ttl != 0) continue;
                    model->glitch[i].pos = (uint8_t)(furi_hal_random_get() % len);
                    model->glitch[i].ch = progress_glitch_glyphs
                        [furi_hal_random_get() % (sizeof(progress_glitch_glyphs) - 1)];
                    model->glitch[i].ttl = (uint8_t)(2 + furi_hal_random_get() % 5);
                    play_glitch = true;
                    glitch_variant = (uint8_t)(furi_hal_random_get() % 3);
                    break;
                }
            }
        },
        true);

    if(play_glitch) {
        // Same varied glitch clicks as the coffee page.
        static const PocketLabSoundId glitch_snd[3] = {
            PocketLabSoundGlitch, PocketLabSoundType, PocketLabSoundGeiger};
        pocketlab_sound_play(
            instance->notifications, instance->sound_enabled, glitch_snd[glitch_variant]);
    }
}

static void progress_view_enter_callback(void* context) {
    ProgressView* instance = context;
    furi_timer_start(instance->timer, furi_ms_to_ticks(PROGRESS_ANIM_MS));
}

static void progress_view_exit_callback(void* context) {
    ProgressView* instance = context;
    furi_timer_stop(instance->timer);
}

ProgressView* progress_view_alloc(void) {
    ProgressView* instance = malloc(sizeof(ProgressView));
    instance->view = view_alloc();
    instance->callback = NULL;
    instance->context = NULL;
    instance->notifications = NULL;
    instance->sound_enabled = false;

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(ProgressModel));
    view_set_draw_callback(instance->view, progress_view_draw_callback);
    view_set_input_callback(instance->view, progress_view_input_callback);
    view_set_enter_callback(instance->view, progress_view_enter_callback);
    view_set_exit_callback(instance->view, progress_view_exit_callback);

    instance->timer =
        furi_timer_alloc(progress_view_timer_callback, FuriTimerTypePeriodic, instance);

    return instance;
}

void progress_view_free(ProgressView* instance) {
    furi_assert(instance);
    furi_timer_stop(instance->timer);
    furi_timer_free(instance->timer);
    view_free(instance->view);
    free(instance);
}

View* progress_view_get_view(ProgressView* instance) {
    furi_assert(instance);
    return instance->view;
}

void progress_view_set_callback(
    ProgressView* instance,
    ProgressViewCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void progress_view_configure(
    ProgressView* instance,
    uint32_t level,
    uint32_t xp,
    uint32_t labs_done,
    uint32_t labs_total,
    uint32_t streak,
    const char* level_title,
    NotificationApp* notifications,
    bool sound_enabled) {
    furi_assert(instance);
    instance->notifications = notifications;
    instance->sound_enabled = sound_enabled;
    with_view_model(
        instance->view,
        ProgressModel * model,
        {
            model->level = level;
            model->xp = xp;
            model->labs_done = labs_done;
            model->labs_total = labs_total;
            model->streak = streak;
            model->level_title = level_title;
            model->anim = 0;
            model->glitch_burst = 0;
            for(uint8_t i = 0; i < PROGRESS_GLITCH_SLOTS; i++)
                model->glitch[i].ttl = 0;
        },
        true);
}

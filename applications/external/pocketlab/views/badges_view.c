#include "badges_view.h"

#include <furi.h>
#include <gui/elements.h>

#include "../helpers/pocketlab_content.h"
#include "../helpers/pocketlab_fonts.h"
#include "../helpers/pocketlab_i18n.h"
#include "../helpers/pocketlab_labtext.h"
#include "../helpers/pocketlab_sound.h"

#define BADGES_COLS         5
#define BADGES_CELL_W       24
#define BADGES_MARGIN_X     4
#define BADGES_CELL_H       25
#define BADGES_TOP          2
#define BADGES_VISIBLE_ROWS 2
#define BADGES_ANIM_MS      90

struct BadgesView {
    View* view;
    FuriTimer* timer;
    NotificationApp* notifications;
    bool sound_enabled;
};

typedef struct {
    uint64_t completed_mask;
    size_t selected;
    size_t offset; // first visible grid row
    uint32_t anim;
    uint8_t press; // countdown for the tap feedback animation
    bool press_err; // true = locked badge shake, false = earned badge ripple
} BadgesModel;

#define BADGES_PRESS_FRAMES 6

// Draws the 12x12 glyph so its filled content is centered on (cx, cy).
static void badges_view_draw_glyph(Canvas* canvas, const uint16_t* rows, uint8_t cx, uint8_t cy) {
    uint8_t min_c = 11, max_c = 0, min_r = 11, max_r = 0;
    bool any = false;
    for(uint8_t r = 0; r < 12; r++) {
        for(uint8_t c = 0; c < 12; c++) {
            if(rows[r] & (1u << c)) {
                any = true;
                if(c < min_c) min_c = c;
                if(c > max_c) max_c = c;
                if(r < min_r) min_r = r;
                if(r > max_r) max_r = r;
            }
        }
    }
    if(!any) return;

    const int ox = (int)cx - (min_c + max_c) / 2;
    const int oy = (int)cy - (min_r + max_r) / 2;
    for(uint8_t r = 0; r < 12; r++) {
        for(uint8_t c = 0; c < 12; c++) {
            if(rows[r] & (1u << c)) {
                canvas_draw_dot(canvas, ox + c, oy + r);
            }
        }
    }
}

static void badges_view_draw_down_arrow(Canvas* canvas, uint8_t x, uint8_t y) {
    canvas_draw_line(canvas, x - 2, y, x + 2, y);
    canvas_draw_line(canvas, x - 1, y + 1, x + 1, y + 1);
    canvas_draw_dot(canvas, x, y + 2);
}

// Targeting reticle: four corner brackets with a chamfered (diagonal) corner,
// like a camera focus mark. Each corner is a vertical arm, a horizontal arm
// offset by 2px, and a short diagonal joining them.
static void badges_view_draw_reticle(Canvas* canvas, uint8_t cx, uint8_t cy, uint8_t size) {
    const uint8_t h = size / 2;
    const uint8_t l = cx - h;
    const uint8_t r = cx + h;
    const uint8_t t = cy - h;
    const uint8_t b = cy + h;

    // top-left
    canvas_draw_line(canvas, l, t + 2, l, t + 4);
    canvas_draw_line(canvas, l + 2, t, l + 4, t);
    canvas_draw_line(canvas, l, t + 2, l + 2, t);
    // top-right
    canvas_draw_line(canvas, r, t + 2, r, t + 4);
    canvas_draw_line(canvas, r - 2, t, r - 4, t);
    canvas_draw_line(canvas, r, t + 2, r - 2, t);
    // bottom-left
    canvas_draw_line(canvas, l, b - 2, l, b - 4);
    canvas_draw_line(canvas, l + 2, b, l + 4, b);
    canvas_draw_line(canvas, l, b - 2, l + 2, b);
    // bottom-right
    canvas_draw_line(canvas, r, b - 2, r, b - 4);
    canvas_draw_line(canvas, r - 2, b, r - 4, b);
    canvas_draw_line(canvas, r, b - 2, r - 2, b);
}

static void badges_view_draw_up_arrow(Canvas* canvas, uint8_t x, uint8_t y) {
    canvas_draw_dot(canvas, x, y);
    canvas_draw_line(canvas, x - 1, y + 1, x + 1, y + 1);
    canvas_draw_line(canvas, x - 2, y + 2, x + 2, y + 2);
}

// A small 6x7 padlock, drawn to the left of a locked badge's name.
static void badges_view_draw_lock(Canvas* canvas, uint8_t x, uint8_t y) {
    static const uint8_t rows[7] = {0x0C, 0x12, 0x12, 0x3F, 0x3F, 0x33, 0x3F};
    for(uint8_t r = 0; r < 7; r++) {
        for(uint8_t c = 0; c < 6; c++) {
            if(rows[r] & (1u << c)) canvas_draw_dot(canvas, x + c, y + r);
        }
    }
}

static void badges_view_draw_callback(Canvas* canvas, void* context) {
    BadgesModel* model = context;
    canvas_clear(canvas);

    const size_t count = pocketlab_labs_count;
    for(size_t i = 0; i < count; i++) {
        const size_t row = i / BADGES_COLS;
        const size_t col = i % BADGES_COLS;
        if(row < model->offset || row >= model->offset + BADGES_VISIBLE_ROWS) {
            continue;
        }

        const uint8_t x = BADGES_MARGIN_X + col * BADGES_CELL_W;
        const uint8_t y = BADGES_TOP + (row - model->offset) * BADGES_CELL_H;
        const uint8_t cx = x + BADGES_CELL_W / 2;
        const uint8_t cy = y + 12;
        const bool earned = (model->completed_mask & (1ULL << i)) != 0;

        if(earned) {
            canvas_draw_disc(canvas, cx, cy, 9);
            canvas_set_color(canvas, ColorWhite);
            badges_view_draw_glyph(canvas, pocketlab_labs[i].icon, cx, cy);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_circle(canvas, cx, cy, 9);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, cx, cy + 1, AlignCenter, AlignCenter, "?");
        }

        if(i == model->selected) {
            // On an error press the whole reticle shakes left/right.
            const int8_t dx =
                (model->press > 0 && model->press_err) ? ((model->press & 1) ? 3 : -3) : 0;
            // Reticle that gently pulses in one step: normal, bigger, normal.
            const uint8_t size = (model->anim / 7) % 2 ? 25 : 23;
            badges_view_draw_reticle(canvas, cx + dx, cy, size);

            // Earned badge: a ring rippling outward from the badge on press.
            if(model->press > 0 && !model->press_err) {
                const uint8_t rr = 9 + (BADGES_PRESS_FRAMES - model->press) * 2;
                canvas_draw_circle(canvas, cx, cy, rr);
            }
        }
    }

    // Blinking scroll hints when there are more rows above or below.
    const size_t total_rows = (count + BADGES_COLS - 1) / BADGES_COLS;
    const bool blink = (model->anim / 6) % 2 == 0;
    if(blink && model->offset > 0) {
        badges_view_draw_up_arrow(canvas, 124, 2);
    }
    if(blink && model->offset + BADGES_VISIBLE_ROWS < total_rows) {
        badges_view_draw_down_arrow(canvas, 124, 59);
    }

    const PocketLabLab* lab = &pocketlab_labs[model->selected];
    const bool selected_earned = (model->completed_mask & (1ULL << model->selected)) != 0;
    pocketlab_font_apply_small(canvas);
    const char* name = pocketlab_tr(lab->badge);
    if(selected_earned) {
        canvas_draw_str_aligned(canvas, 64, 61, AlignCenter, AlignBottom, name);
    } else {
        // A padlock to the left of the name marks a badge that is still locked.
        const uint8_t lock_w = 6;
        const uint8_t gap = 3;
        const uint16_t tw = canvas_string_width(canvas, name);
        const uint8_t total = (uint8_t)(lock_w + gap + tw);
        const uint8_t sx = (uint8_t)(64 - total / 2);
        badges_view_draw_lock(canvas, sx, 54);
        canvas_draw_str_aligned(canvas, sx + lock_w + gap, 61, AlignLeft, AlignBottom, name);
    }
}

static bool badges_view_input_callback(InputEvent* event, void* context) {
    BadgesView* instance = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;
    bool play_coin = false;
    bool play_jingle = false;
    bool play_error = false;
    uint32_t jingle_index = 0;

    with_view_model(
        instance->view,
        BadgesModel * model,
        {
            const size_t count = pocketlab_labs_count;
            size_t sel = model->selected;
            bool moved = false;
            if(event->key == InputKeyRight) {
                if(sel + 1 < count) sel++;
                moved = true;
            } else if(event->key == InputKeyLeft) {
                if(sel > 0) sel--;
                moved = true;
            } else if(event->key == InputKeyDown) {
                if(sel + BADGES_COLS < count) sel += BADGES_COLS;
                moved = true;
            } else if(event->key == InputKeyUp) {
                if(sel >= BADGES_COLS) sel -= BADGES_COLS;
                moved = true;
            } else if(event->key == InputKeyOk) {
                consumed = true;
                model->press = BADGES_PRESS_FRAMES; // kick off the tap feedback
                if(model->completed_mask & (1ULL << model->selected)) {
                    play_jingle = true;
                    jingle_index = model->selected;
                    model->press_err = false; // earned: ripple + jingle
                } else {
                    play_error = true;
                    model->press_err = true; // locked: shake + error tone
                }
            }

            if(moved) {
                consumed = true;
                play_coin = true;
                model->selected = sel;
                const size_t row = sel / BADGES_COLS;
                if(row < model->offset) {
                    model->offset = row;
                } else if(row >= model->offset + BADGES_VISIBLE_ROWS) {
                    model->offset = row - BADGES_VISIBLE_ROWS + 1;
                }
            }
        },
        true);

    if(play_coin) {
        pocketlab_sound_play(instance->notifications, instance->sound_enabled, PocketLabSoundCoin);
    }
    if(play_jingle) {
        pocketlab_sound_play_jingle(
            instance->notifications, instance->sound_enabled, jingle_index);
    }
    if(play_error) {
        pocketlab_sound_play(
            instance->notifications, instance->sound_enabled, PocketLabSoundWrong);
    }

    return consumed;
}

static void badges_view_timer_callback(void* context) {
    BadgesView* instance = context;
    with_view_model(
        instance->view,
        BadgesModel * model,
        {
            model->anim++;
            if(model->press > 0) model->press--;
        },
        true);
}

static void badges_view_enter_callback(void* context) {
    BadgesView* instance = context;
    furi_timer_start(instance->timer, furi_ms_to_ticks(BADGES_ANIM_MS));
}

static void badges_view_exit_callback(void* context) {
    BadgesView* instance = context;
    furi_timer_stop(instance->timer);
}

BadgesView* badges_view_alloc(void) {
    BadgesView* instance = malloc(sizeof(BadgesView));
    instance->view = view_alloc();

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(BadgesModel));
    view_set_draw_callback(instance->view, badges_view_draw_callback);
    view_set_input_callback(instance->view, badges_view_input_callback);
    view_set_enter_callback(instance->view, badges_view_enter_callback);
    view_set_exit_callback(instance->view, badges_view_exit_callback);

    instance->timer =
        furi_timer_alloc(badges_view_timer_callback, FuriTimerTypePeriodic, instance);

    return instance;
}

void badges_view_free(BadgesView* instance) {
    furi_assert(instance);
    furi_timer_stop(instance->timer);
    furi_timer_free(instance->timer);
    view_free(instance->view);
    free(instance);
}

View* badges_view_get_view(BadgesView* instance) {
    furi_assert(instance);
    return instance->view;
}

void badges_view_configure(
    BadgesView* instance,
    uint64_t completed_mask,
    NotificationApp* notifications,
    bool sound_enabled) {
    furi_assert(instance);
    instance->notifications = notifications;
    instance->sound_enabled = sound_enabled;
    with_view_model(
        instance->view,
        BadgesModel * model,
        {
            model->completed_mask = completed_mask;
            model->selected = 0;
            model->offset = 0;
            model->anim = 0;
            model->press = 0;
            model->press_err = false;
        },
        true);
}

#include "polk_remote.h"
#include "polk_magnifimini_remote_icons.h"
#include "scenes.h"

// Portrait canvas: 64 wide × 128 tall. Title bar (16 px) + scrollable rows (18 px each).
#define TITLE_H      16
#define ROW_H        18
#define SCREEN_W     64
#define SCREEN_H     128
#define VISIBLE_ROWS ((SCREEN_H - TITLE_H) / ROW_H)   // 6 rows visible at once

// ── Icon lookup ───────────────────────────────────────────────────────────────

typedef struct {
    const Icon* left;
    const Icon* center;
    const Icon* right;
    const Icon* row_icon;
} RowIcons;

static const RowIcons ROW_ICONS[ROWS_COUNT] = {
    [0] = {&I_night,  &I_power, &I_mute,  NULL},      // POWER (Night | Power | Mute)
    [1] = {&I_vol_dn, NULL,     &I_vol_up, NULL},      // VOL
    [2] = {&I_prev,   &I_play,  &I_next,  NULL},      // PLAYBACK
    [3] = {&I_minus,  NULL,     &I_plus,  NULL},      // VOICE
    [4] = {&I_minus,  NULL,     &I_plus,  NULL},      // BASS
    [5] = {NULL, NULL, NULL, NULL},                    // INPUT
    [6] = {NULL, NULL, NULL, NULL},                    // AUDIO PROFILE
};

// ── Draw helpers ──────────────────────────────────────────────────────────────

// cx thresholds carve 64 px into equal thirds: 0-20 | 21-42 | 43-63
static void draw_cell(
    Canvas* canvas, int32_t cx, int32_t row_y,
    const Icon* icon, const char* fallback, bool inverted) {
    if(inverted) {
        int32_t cell_x = (cx <= 16) ? 0 : (cx <= 40) ? 21 : 43;
        int32_t cell_w = (cx <= 16) ? 21 : (cx <= 40) ? 22 : 21;
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, cell_x, row_y, cell_w, ROW_H);
        canvas_set_color(canvas, ColorWhite);
    }
    if(icon) {
        canvas_draw_icon(canvas, cx - 8, row_y + 1, icon);
    } else if(fallback) {
        canvas_draw_str_aligned(canvas, cx, row_y + ROW_H - 4, AlignCenter, AlignBottom, fallback);
    }
    if(inverted) {
        canvas_set_color(canvas, ColorBlack);
    }
}

// ── Draw callback ─────────────────────────────────────────────────────────────

void scene_main_draw_cb(Canvas* canvas, void* model) {
    MainViewModel* m = model;
    uint8_t sel = m->selected_row;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    // Title bar
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, TITLE_H - 4, AlignCenter, AlignBottom, "Polk Magnifi");
    canvas_draw_line(canvas, 0, TITLE_H - 1, SCREEN_W - 1, TITLE_H - 1);

    uint8_t scroll = m->scroll_offset;
    for(uint8_t i = scroll; i < scroll + VISIBLE_ROWS && i < ROWS_COUNT; i++) {
        const RemoteRow* row = &ROWS[i];
        const RowIcons*  ico = &ROW_ICONS[i];
        int32_t y = TITLE_H + (i - scroll) * ROW_H;
        bool highlighted = (i == sel);
        bool pr = m->is_pressed && (i == m->pressed_row);

        canvas_set_color(canvas, ColorBlack);
        if(highlighted) {
            canvas_draw_frame(canvas, 0, y, SCREEN_W, ROW_H);
        }

        int32_t baseline = y + ROW_H - 4;

        switch(row->type) {
        case RowSingle:
            if(ico->row_icon) {
                canvas_draw_icon(canvas, SCREEN_W / 2 - 8, y + 1, ico->row_icon);
            } else {
                canvas_draw_str_aligned(
                    canvas, SCREEN_W / 2, baseline, AlignCenter, AlignBottom, row->label);
            }
            break;

        case RowLR:
            draw_cell(canvas, 10, y, ico->left,  "-", pr && m->pressed_cell == 0);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, baseline, AlignCenter, AlignBottom, row->label);
            draw_cell(canvas, 54, y, ico->right, "+", pr && m->pressed_cell == 2);
            break;

        case RowLOR:
            draw_cell(canvas, 10, y, ico->left,   "|<",       pr && m->pressed_cell == 0);
            draw_cell(canvas, 32, y, ico->center, row->label, pr && m->pressed_cell == 1);
            draw_cell(canvas, 54, y, ico->right,  ">|",       pr && m->pressed_cell == 2);
            break;

        case RowSubmenu:
            if(pr && m->pressed_cell == 1) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_box(canvas, 0, y, SCREEN_W, ROW_H);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, baseline, AlignCenter, AlignBottom, row->label);
            canvas_set_color(canvas, ColorBlack);
            break;
        }
    }

    canvas_set_color(canvas, ColorBlack);
}

// ── Input callback ────────────────────────────────────────────────────────────

bool scene_main_input_cb(InputEvent* event, void* ctx) {
    PolkRemoteApp* app = ctx;

    // Per-cell press highlight: set on Press, clear on Release
    if(event->type == InputTypePress &&
       (event->key == InputKeyLeft || event->key == InputKeyOk || event->key == InputKeyRight)) {
        const RemoteRow* pr_row = &ROWS[app->selected_row];
        bool ok_active = (pr_row->cmd_ok != NULL) || (pr_row->type == RowSubmenu);
        if(event->key == InputKeyOk && !ok_active) return false;
        uint8_t cell = (event->key == InputKeyLeft) ? 0 : (event->key == InputKeyOk) ? 1 : 2;
        with_view_model(
            app->main_view, MainViewModel * m,
            { m->is_pressed = true; m->pressed_row = app->selected_row; m->pressed_cell = cell; },
            true);
        return false;
    }

    if(event->type == InputTypeRelease) {
        with_view_model(
            app->main_view, MainViewModel * m,
            { m->is_pressed = false; },
            true);
        return false;
    }

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    if(event->key == InputKeyUp) {
        if(app->selected_row > 0) {
            app->selected_row--;
            with_view_model(
                app->main_view, MainViewModel * m,
                {
                    m->selected_row = app->selected_row;
                    if(m->selected_row < m->scroll_offset)
                        m->scroll_offset = m->selected_row;
                },
                true);
        }
        return true;
    }

    if(event->key == InputKeyDown) {
        if(app->selected_row < ROWS_COUNT - 1) {
            app->selected_row++;
            with_view_model(
                app->main_view, MainViewModel * m,
                {
                    m->selected_row = app->selected_row;
                    if(m->selected_row >= m->scroll_offset + VISIBLE_ROWS)
                        m->scroll_offset = m->selected_row - VISIBLE_ROWS + 1;
                },
                true);
        }
        return true;
    }

    const RemoteRow* row = &ROWS[app->selected_row];

    if(event->key == InputKeyLeft) {
        polk_send(row->cmd_left);
        return true;
    }

    if(event->key == InputKeyRight) {
        polk_send(row->cmd_right);
        return true;
    }

    if(event->key == InputKeyOk) {
        if(row->type == RowSubmenu) {
            app->active_row = app->selected_row;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, PolkEventOpenSubmenu);
        } else {
            polk_send(row->cmd_ok);
        }
        return true;
    }

    return false;
}

// ── Scene callbacks ───────────────────────────────────────────────────────────

void scene_main_on_enter(void* ctx) {
    PolkRemoteApp* app = ctx;
    with_view_model(
        app->main_view, MainViewModel * m,
        { m->selected_row = app->selected_row; },
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, SceneMain);
}

bool scene_main_on_event(void* ctx, SceneManagerEvent event) {
    PolkRemoteApp* app = ctx;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == PolkEventOpenSubmenu) {
        scene_manager_next_scene(app->scene_manager, SceneSubMenu);
        return true;
    }
    return false;
}

void scene_main_on_exit(void* ctx) {
    UNUSED(ctx);
}

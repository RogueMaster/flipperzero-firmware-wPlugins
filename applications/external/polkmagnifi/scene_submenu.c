#include "polk_remote.h"
#include "scenes.h"
#include <string.h>

#define HEADER_H 16
#define ITEM_H   16
#define SCREEN_W 64

// ── Draw callback ─────────────────────────────────────────────────────────────

void scene_submenu_draw_cb(Canvas* canvas, void* model) {
    SubMenuViewModel* m = model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    // Title — bottom separator line only
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, HEADER_H - 1, SCREEN_W - 1, HEADER_H - 1);
    const char* space = strchr(m->title, ' ');
    if(space && canvas_string_width(canvas, m->title) > SCREEN_W) {
        char word1[16];
        size_t len = (size_t)(space - m->title);
        if(len >= sizeof(word1)) len = sizeof(word1) - 1;
        memcpy(word1, m->title, len);
        word1[len] = '\0';
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, 7,  AlignCenter, AlignBottom, word1);
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, 15, AlignCenter, AlignBottom, space + 1);
    } else {
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, 12, AlignCenter, AlignBottom, m->title);
    }

    // Menu items
    for(uint8_t i = 0; i < m->count; i++) {
        int32_t y = HEADER_H + i * ITEM_H;
        bool selected = (i == m->selected);
        bool pressed  = selected && m->is_pressed;

        canvas_set_color(canvas, ColorBlack);
        if(pressed) {
            canvas_draw_box(canvas, 0, y, SCREEN_W, ITEM_H);
            canvas_set_color(canvas, ColorWhite);
        } else if(selected) {
            canvas_draw_frame(canvas, 0, y, SCREEN_W, ITEM_H);
        }

        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, y + ITEM_H - 4, AlignCenter, AlignBottom, m->labels[i]);

        canvas_set_color(canvas, ColorBlack);
    }

    canvas_set_color(canvas, ColorBlack);
}

// ── Input callback ────────────────────────────────────────────────────────────

bool scene_submenu_input_cb(InputEvent* event, void* ctx) {
    PolkRemoteApp* app = ctx;

    if(event->type == InputTypePress && event->key == InputKeyOk) {
        with_view_model(app->submenu_view, SubMenuViewModel * m, { m->is_pressed = true; }, true);
        return false;
    }

    if(event->type == InputTypeRelease) {
        with_view_model(app->submenu_view, SubMenuViewModel * m, { m->is_pressed = false; }, true);
        return false;
    }

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    if(event->key == InputKeyBack) {
        return false;
    }

    const RemoteRow* row = &ROWS[app->active_row];

    if(event->key == InputKeyUp) {
        with_view_model(
            app->submenu_view, SubMenuViewModel * m,
            { if(m->selected > 0) m->selected--; },
            true);
        return true;
    }

    if(event->key == InputKeyDown) {
        with_view_model(
            app->submenu_view, SubMenuViewModel * m,
            { if(m->selected < m->count - 1) m->selected++; },
            true);
        return true;
    }

    if(event->key == InputKeyOk) {
        uint8_t sel = 0;
        with_view_model(
            app->submenu_view, SubMenuViewModel * m,
            { sel = m->selected; },
            false);
        if(sel < row->sub_count) {
            polk_send(row->sub_cmds[sel]);
        }
        return true;
    }

    return false;
}

// ── Scene callbacks ───────────────────────────────────────────────────────────

void scene_submenu_on_enter(void* ctx) {
    PolkRemoteApp* app = ctx;
    const RemoteRow* row = &ROWS[app->active_row];

    with_view_model(
        app->submenu_view, SubMenuViewModel * m,
        {
            m->selected = 0;
            m->count    = row->sub_count;
            m->title    = row->label;
            m->labels   = row->sub_labels;
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, SceneSubMenu);
}

bool scene_submenu_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void scene_submenu_on_exit(void* ctx) {
    UNUSED(ctx);
}

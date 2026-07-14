#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "utils/draw.h"
#include "../iconedit.h"
#include <iconedit_icons.h>

static struct {
    SettingType selected_item;
} settingsModel = {.selected_item = Setting_Canvas_Scale};

#define MAX_CANVAS_SCALE 10

void settings_draw(Canvas* canvas, void* context) {
    UNUSED(canvas);
    UNUSED(context);
    IconEdit* app = context;

    int y = 18;
    // Canvas scale: Auto, 1x, 2x, 3x, .. 10x
    canvas_draw_str_aligned(canvas, 3, y, AlignLeft, AlignTop, "Canvas Scale");
    size_t cw = canvas_width(canvas);
    char buf[8] = {0};
    if(app->settings.canvas_scale == 0) {
        snprintf(buf, sizeof(buf), "%s", "Auto");
    } else {
        snprintf(buf, sizeof(buf), "%dx", app->settings.canvas_scale);
    }
    canvas_draw_str_aligned(canvas, cw - (cw / 4), y, AlignCenter, AlignTop, buf);

    if(app->panel == Panel_Settings && settingsModel.selected_item == Setting_Canvas_Scale) {
        if(app->settings.canvas_scale > 0) {
            canvas_draw_icon(canvas, cw - (cw / 4) - 16 - 5, y + 1, &I_iet_smArrowL);
        }
        if(app->settings.canvas_scale < MAX_CANVAS_SCALE) {
            canvas_draw_icon(canvas, cw - (cw / 4) + 16, y + 1, &I_iet_smArrowR);
        }
        canvas_set_color(canvas, ColorXOR);
        canvas_draw_frame(canvas, 0, y - 2, cw, 11);
        canvas_set_color(canvas, ColorBlack);
    }

    y += 13;
    // Draw Cursor Guides: On / Off
    canvas_draw_str_aligned(canvas, 3, y, AlignLeft, AlignTop, "Cursor Guides");
    canvas_draw_str_aligned(
        canvas,
        cw - (cw / 4),
        y,
        AlignCenter,
        AlignTop,
        app->settings.draw_cursor_guides ? "On" : "Off");

    if(app->panel == Panel_Settings && settingsModel.selected_item == Setting_Draw_Cursor_Guides) {
        if(app->settings.draw_cursor_guides) {
            canvas_draw_icon(canvas, cw - (cw / 4) - 16 - 5, y + 1, &I_iet_smArrowL);
        } else {
            canvas_draw_icon(canvas, cw - (cw / 4) + 16, y + 1, &I_iet_smArrowR);
        }
        canvas_set_color(canvas, ColorXOR);
        canvas_draw_frame(canvas, 0, y - 2, cw, 11);
        canvas_set_color(canvas, ColorBlack);
    }
}

bool settings_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp: {
            if(settingsModel.selected_item == Setting_Canvas_Scale) {
                app->panel = Panel_TabBar;
                iconedit_save_settings(app);
            }
            if(settingsModel.selected_item == Setting_Draw_Cursor_Guides) {
                settingsModel.selected_item = Setting_Canvas_Scale;
            }
            break;
        }
        case InputKeyLeft:
            if(settingsModel.selected_item == Setting_Canvas_Scale) {
                if(app->settings.canvas_scale > 0) {
                    app->settings.canvas_scale--;
                }
            }
            if(settingsModel.selected_item == Setting_Draw_Cursor_Guides) {
                if(app->settings.draw_cursor_guides) {
                    app->settings.draw_cursor_guides = false;
                }
            }
            break;
        case InputKeyRight:
            if(settingsModel.selected_item == Setting_Canvas_Scale) {
                if(app->settings.canvas_scale < MAX_CANVAS_SCALE) {
                    app->settings.canvas_scale++;
                }
            }
            if(settingsModel.selected_item == Setting_Draw_Cursor_Guides) {
                if(!app->settings.draw_cursor_guides) {
                    app->settings.draw_cursor_guides = true;
                }
            }
            break;
        case InputKeyDown:
            if(settingsModel.selected_item == Setting_Canvas_Scale) {
                settingsModel.selected_item = Setting_Draw_Cursor_Guides;
            }
            break;
        case InputKeyBack:
            app->panel = Panel_TabBar;
            iconedit_save_settings(app);
            break;
        default:
            break;
        }
    }
    return consumed;
}

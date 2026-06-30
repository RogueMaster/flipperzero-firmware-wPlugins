#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "../iconedit.h"
#include "../utils/file_utils.h"
#include "../utils/draw.h"

typedef enum {
    Save_START,
    Save_PNG = Save_START,
    Save_C,
#ifdef ENABLE_XBM_SAVE
    Save_XBM,
#endif
    Save_BMX, // used for Asset Packs
    Save_COUNT,
    Save_NONE
} SaveMode;

typedef enum {
    Save_Frame,
    Save_Anim,
} SaveType;

const char* SaveModeStr[Save_COUNT] = {
    "PNG",
    ".C",
#ifdef ENABLE_XBM_SAVE
    "XBM",
#endif
    "BMX",
};
const SaveMode Save_UP[Save_COUNT] = {
    Save_NONE,
    Save_PNG,
    Save_C,
#ifdef ENABLE_XBM_SAVE
    Save_XBM,
#endif
};
const SaveMode Save_DOWN[Save_COUNT] = {
    Save_C,
#ifdef ENABLE_XBM_SAVE
    Save_XBM,
#endif
    Save_BMX,
    Save_NONE};

static struct {
    SaveMode save_mode;
    SaveType save_type;
} saveModel = {.save_mode = Save_PNG, .save_type = Save_Frame};

bool save_as_anim_capable(SaveMode mode) {
    return (mode == Save_PNG || mode == Save_C);
}

void save_as_draw(Canvas* canvas, void* context) {
    IconEdit* app = context;
    bool is_anim = app->icon->frame_count > 1;

    int pad = 2; // outside padding between frame and item
    int elem_h = is_anim ? 9 : 7;
    int elem_w = is_anim ? 76 : 22;
    int elem_pad = 2; // the space between

    // draw an empty panel frame
    int rw = elem_w + (pad * 2) + (elem_pad * 2);
    int rh = (elem_h + (elem_pad * 2)) * Save_COUNT + (pad * 2);
    int x = (canvas_width(canvas) - rw) / 2;
    int y = (canvas_height(canvas) - rh) / 2;
    ie_draw_modal_panel_frame(canvas, x, y, rw, rh);

    // draw save menu items
    for(SaveMode mode = Save_START; mode < Save_COUNT; mode++) {
        // draw the mode name (PNG, XBM, etc)
        canvas_draw_str_aligned(
            canvas,
            x + pad + elem_pad + 1,
            y + pad + elem_pad + mode * (elem_h + elem_pad * 2) + is_anim,
            AlignLeft,
            AlignTop,
            SaveModeStr[mode]);

        if(mode == saveModel.save_mode) {
            // hightlight the selected item - but differently if anim enabled
            if(save_as_anim_capable(mode) && is_anim) {
                canvas_draw_rframe(
                    canvas,
                    x + pad,
                    y + pad + (elem_h + (elem_pad * 2)) * mode,
                    elem_w + elem_pad * 2,
                    elem_h + elem_pad * 2,
                    1);
                canvas_draw_rbox(
                    canvas,
                    x + pad + elem_pad + 1 + 49 - 25,
                    y + pad + (elem_h + (elem_pad * 2)) * mode +
                        (saveModel.save_type == Save_Frame ? 0 : 6),
                    50,
                    elem_h / 2 + elem_pad * 2 - 1,
                    1);
                canvas_set_color(canvas, ColorXOR);
                int32_t curr_y = y + pad + elem_pad + mode * (elem_h + elem_pad * 2);
                char frame_str[16] = {};
                snprintf(
                    frame_str,
                    sizeof(frame_str),
                    "%s FRAME %d %s",
                    app->icon->current_frame == 0 ? " " : "<",
                    app->icon->current_frame + 1,
                    app->icon->current_frame == app->icon->frame_count - 1 ? " " : ">");
                ie_draw_str(
                    canvas,
                    x + pad + elem_pad + 1 + 49,
                    curr_y - 1,
                    AlignCenter,
                    AlignTop,
                    FontMicro,
                    frame_str);
                ie_draw_str(
                    canvas,
                    x + pad + elem_pad + 1 + 49,
                    curr_y + 5,
                    AlignCenter,
                    AlignTop,
                    FontMicro,
                    "ANIM");
                canvas_set_color(canvas, ColorBlack);

            } else {
                // single frame, highlight the entire item
                canvas_set_color(canvas, ColorXOR);
                canvas_draw_rbox(
                    canvas,
                    x + pad,
                    y + pad + (elem_h + (elem_pad * 2)) * mode,
                    elem_w + elem_pad * 2,
                    elem_h + elem_pad * 2,
                    0);
                canvas_set_color(canvas, ColorBlack);
            }
        }
    }
}

bool save_as_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    bool is_anim = app->icon->frame_count > 1;

    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp: {
            if(save_as_anim_capable(saveModel.save_mode) && is_anim &&
               saveModel.save_type == Save_Anim) {
                saveModel.save_type = Save_Frame;
                break;
            }
            SaveMode next_save = Save_UP[saveModel.save_mode];
            if(next_save != Save_NONE) {
                saveModel.save_mode = next_save;
                if(save_as_anim_capable(saveModel.save_mode) && is_anim) {
                    saveModel.save_type = Save_Anim;
                }
            }
            break;
        }
        case InputKeyDown: {
            if(save_as_anim_capable(saveModel.save_mode) && is_anim &&
               saveModel.save_type == Save_Frame) {
                saveModel.save_type = Save_Anim;
                break;
            }
            SaveMode next_save = Save_DOWN[saveModel.save_mode];
            if(next_save != Save_NONE) {
                saveModel.save_mode = next_save;
                if(save_as_anim_capable(saveModel.save_mode) && is_anim) {
                    saveModel.save_type = Save_Frame;
                }
            }
            break;
        }
        case InputKeyLeft: {
            if(is_anim && save_as_anim_capable(saveModel.save_mode)) {
                app->icon->current_frame -= app->icon->current_frame > 0;
            }
            break;
        }
        case InputKeyRight: {
            if(is_anim && save_as_anim_capable(saveModel.save_mode)) {
                app->icon->current_frame += app->icon->current_frame < app->icon->frame_count - 1;
            }
            break;
        }
        case InputKeyBack: {
            consumed = false; // send control back to File panel
            app->panel = Panel_File;
            break;
        }
        case InputKeyOk: {
            bool save_successful = false;
            switch(saveModel.save_mode) {
            case Save_PNG: {
                if(!is_anim || saveModel.save_type == Save_Frame) {
                    save_successful = png_file_save(app->icon, true);
                } else {
                    save_successful = png_file_save(app->icon, false);
                }
                break;
            }
            case Save_BMX: {
                save_successful = bmx_file_save(app->icon);
                break;
            }
            case Save_C: {
                if(!is_anim || saveModel.save_type == Save_Frame) {
                    save_successful = c_file_save(app->icon, true);
                } else {
                    save_successful = c_file_save(app->icon, false);
                }
                break;
            }
            case Save_XBM: {
                save_successful = xbm_file_save(app->icon);
                break;
            }
            default:
                break;
            }
            if(save_successful) {
                dialog_info_dialog(app, "File saved!", Panel_File);
                app->dirty = false;
            } else {
                dialog_info_dialog(app, "Failed to save!", Panel_File);
            }
            consumed = false;
            break;
        }
        default:
            break;
        }
    }
    return consumed;
}

#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "../iconedit.h"
#include "../utils/draw.h"

#define CANVAS_DIM 60u // same for width and height

typedef enum {
    Draw_NONE,
    Draw_Point1_Set,
} DrawState;

static struct {
    IEIcon* icon;

    // cursor in absolute icon coordinates
    size_t cursor_x;
    size_t cursor_y;

    // view port
    size_t scale;
    size_t vpx, vpy; // top-left of our view
    size_t vpw, vph; // view port size
    size_t vps; // size of each drawn pixel
    bool scroll;

    DrawState draw_state;
    size_t p1x, p1y;
    uint8_t* double_buffer;
} canvasModel = {0};

void canvas_initialize(IEIcon* icon, size_t scale_setting) {
    canvas_free_canvas();

    canvasModel.icon = icon;
    canvasModel.double_buffer = malloc(icon->width * icon->height * sizeof(uint8_t));

    canvas_set_scale(scale_setting);

    canvasModel.draw_state = Draw_NONE;
    canvasModel.p1x = 0;
    canvasModel.p1y = 0;
}

void canvas_free_canvas() {
    if(canvasModel.double_buffer) {
        free(canvasModel.double_buffer);
        canvasModel.double_buffer = NULL;
    }
}

// Sets up the view port size based on the scale setting.
// Repositions the cursor to the middle of the viewport
void canvas_set_scale(size_t scale_setting) {
    if(scale_setting == 0) {
        FURI_LOG_E(TAG, "Can not set canvas scale to 0");
        return;
    }
    // Auto minimum scale is always 2x - so we can actually SEE the pixels
    // also, let's constrain the canvas to the 64x64 area - always
    // but our drawing area needs to be a little less - like 60x60? <-- do we even do this?
    // anything larger needs a scrolling canvas!
    // Hitting Play will show it fullscreen too - even if 1 frame
    canvasModel.scale = scale_setting == SETTING_SCALE_AUTO ? SETTING_SCALE_AUTO_MIN :
                                                              scale_setting;
    size_t max_dim = MAX(canvasModel.icon->width, canvasModel.icon->height);
    if(scale_setting == SETTING_SCALE_AUTO && max_dim <= (CANVAS_DIM / SETTING_SCALE_AUTO_MIN)) {
        canvasModel.scale = CANVAS_DIM / max_dim;
    }

    // recompute our viewport size, width, and height
    canvasModel.vpx = 0;
    canvasModel.vpy = 0;

    canvasModel.vps = CANVAS_DIM / canvasModel.scale;
    canvasModel.vpw = MIN(canvasModel.vps, canvasModel.icon->width);
    canvasModel.vph = MIN(canvasModel.vps, canvasModel.icon->height);

    canvasModel.scroll = canvasModel.vpw < canvasModel.icon->width ||
                         canvasModel.vph < canvasModel.icon->height;

    // place the cursor in the middle-ish of view port
    canvasModel.cursor_x = canvasModel.vpw > 1 ? (canvasModel.vpw / 2) - 1 : 0;
    canvasModel.cursor_y = canvasModel.vph > 1 ? (canvasModel.vph / 2) - 1 : 0;
}

void canvas_draw(Canvas* canvas, void* context) {
    IconEdit* app = context;
    IEIcon* icon = app->icon;

    // Determine where we're drawing our canvas, based on icon size
    // Default to the top-left corner of our canvas area, with a 2-pixel
    // padding for grid dots, cursor guides and scrollbars
    // Assign offsets so that our viewport is centered in our canvas
    int x_offset = 64 + 2 + (CANVAS_DIM - (canvasModel.vpw * canvasModel.scale)) / 2;
    int y_offset = 2 + (CANVAS_DIM - (canvasModel.vph * canvasModel.scale)) / 2;
    // clear the entire canvas area
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 64, 0, 64, 64);
    canvas_set_color(canvas, ColorBlack);

    // draw editor grid - the guide dots around the canvas border
    // we should change the grid size based on scale - meaning, if scale is 1 or 2, drawing dots every
    // 1 or 2 pixels doesn't look good, so bump that up a bit?
    for(size_t i = 0; i < (canvasModel.vpw + 1); ++i) {
        // top and bottom frame dots
        canvas_draw_dot(canvas, x_offset + i * canvasModel.scale, y_offset - 1);
        canvas_draw_dot(
            canvas,
            x_offset + i * canvasModel.scale,
            y_offset + (canvasModel.vph * canvasModel.scale));
    }
    for(size_t i = 0; i < (canvasModel.vph + 1); ++i) {
        // left and right frame dots
        canvas_draw_dot(canvas, x_offset - 1, y_offset + i * canvasModel.scale);
        canvas_draw_dot(
            canvas,
            x_offset + (canvasModel.vpw * canvasModel.scale) + 1,
            y_offset + i * canvasModel.scale);
    }

    // draw scroll bars!
    if(canvasModel.scroll) {
        // X axis scrollbar
        if(canvasModel.vpw < icon->width) {
            // scrollbar width in screen pixels
            int sw =
                ((1.0 * canvasModel.vpw) / icon->width) * (canvasModel.vpw * canvasModel.scale);
            // X position as a percentage
            float xp = (canvasModel.vpx * 1.0) / icon->width;
            // scrollbar offset in screen pixels
            int so = (canvasModel.vpw * canvasModel.scale) * xp;
            int ys = y_offset + (canvasModel.vph * canvasModel.scale) + 1;
            canvas_draw_line(canvas, x_offset + so, ys, x_offset + so + sw, ys);
        }
        // Y axis scrollbar
        if(canvasModel.vph < icon->height) {
            // scrollbar height in screen pixels
            int sh =
                ((1.0 * canvasModel.vph) / icon->height) * (canvasModel.vph * canvasModel.scale);
            // Y position as a percentage
            float yp = (canvasModel.vpy * 1.0) / icon->height;
            // scrollbar offset in screen pixels
            int so = (canvasModel.vph * canvasModel.scale) * yp;
            canvas_draw_line(
                canvas, x_offset - 2, y_offset + so, x_offset - 2, y_offset + so + sh);
        }
    }

    // set display buffer
    Frame* f = ie_icon_get_current_frame(icon);
    for(size_t i = 0; i < icon->width * icon->height; ++i) {
        canvasModel.double_buffer[i] = f->data[i];
    }

    // if we're drawing a circle/rect/line, add that to our view
    // this is independent of our current view port
    if(canvasModel.draw_state == Draw_Point1_Set) {
        uint8_t x1 = canvasModel.p1x;
        uint8_t y1 = canvasModel.p1y;
        uint8_t x2 = canvasModel.cursor_x;
        uint8_t y2 = canvasModel.cursor_y;
        int bw = icon->width;
        int bh = icon->height;
        switch(tools_get_selected_tool()) {
        case Tool_Line:
            // draw line between pt1 and cursor
            ie_draw_line(canvasModel.double_buffer, x1, y1, x2, y2, bw, bh);
            break;
        case Tool_Circle:
            ie_draw_circle(canvasModel.double_buffer, x1, y1, x2, y2, bw, bh);
            break;
        case Tool_Rect:
            ie_draw_line(canvasModel.double_buffer, x1, y1, x2, y1, bw, bh);
            ie_draw_line(canvasModel.double_buffer, x2, y1, x2, y2, bw, bh);
            ie_draw_line(canvasModel.double_buffer, x2, y2, x1, y2, bw, bh);
            ie_draw_line(canvasModel.double_buffer, x1, y1, x1, y2, bw, bh);
            break;
        default:
            break;
        }
    }

    // Draw the icon: scaled editor view, taking into account the scrolled viewport
    for(size_t y = canvasModel.vpy; y < canvasModel.vpy + canvasModel.vph; ++y) {
        for(size_t x = canvasModel.vpx; x < canvasModel.vpx + canvasModel.vpw; ++x) {
            if(canvasModel.double_buffer[y * icon->width + x]) {
                canvas_draw_box(
                    canvas,
                    x_offset + ((x - canvasModel.vpx) * canvasModel.scale),
                    y_offset + ((y - canvasModel.vpy) * canvasModel.scale),
                    canvasModel.scale,
                    canvasModel.scale);
            }
        }
    }

    // Draw the cursor - one dot per corner
    int cx = canvasModel.cursor_x - canvasModel.vpx;
    int cy = canvasModel.cursor_y - canvasModel.vpy;

    if(app->panel == Panel_Canvas) {
        canvas_set_color(canvas, ColorXOR);
        canvas_draw_dot(
            canvas, x_offset + cx * canvasModel.scale, y_offset + cy * canvasModel.scale);
        canvas_draw_dot(
            canvas,
            x_offset + cx * canvasModel.scale + (canvasModel.scale - 1),
            y_offset + cy * canvasModel.scale);
        canvas_draw_dot(
            canvas,
            x_offset + cx * canvasModel.scale + (canvasModel.scale - 1),
            y_offset + cy * canvasModel.scale + (canvasModel.scale - 1));
        canvas_draw_dot(
            canvas,
            x_offset + cx * canvasModel.scale,
            y_offset + cy * canvasModel.scale + (canvasModel.scale - 1));
        canvas_set_color(canvas, ColorBlack);
    }

    // Draw cursor guides around border - amidst the grid dots
    if(app->panel == Panel_Canvas && app->settings.draw_cursor_guides) {
        // top
        canvas_draw_line(
            canvas,
            x_offset + cx * canvasModel.scale,
            y_offset - 1,
            x_offset + cx * canvasModel.scale + (canvasModel.scale - 1),
            y_offset - 1);
        // bottom
        canvas_draw_line(
            canvas,
            x_offset + cx * canvasModel.scale,
            y_offset + (canvasModel.vph * canvasModel.scale),
            x_offset + cx * canvasModel.scale + (canvasModel.scale - 1),
            y_offset + (canvasModel.vph * canvasModel.scale));
        // left
        canvas_draw_line(
            canvas,
            x_offset - 1,
            y_offset + cy * canvasModel.scale,
            x_offset - 1,
            y_offset + cy * canvasModel.scale + (canvasModel.scale - 1));
        // right
        canvas_draw_line(
            canvas,
            x_offset + (canvasModel.vpw * canvasModel.scale) + 1,
            y_offset + cy * canvasModel.scale,
            x_offset + (canvasModel.vpw * canvasModel.scale) + 1,
            y_offset + cy * canvasModel.scale + (canvasModel.scale - 1));
    }
}

bool canvas_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    bool consumed = true;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyUp: {
            canvasModel.cursor_y -= canvasModel.cursor_y > 0;
            if(canvasModel.cursor_y < canvasModel.vpy + 3 && canvasModel.vpy > 0) {
                canvasModel.vpy--;
            }
            break;
        }
        case InputKeyDown: {
            canvasModel.cursor_y += canvasModel.cursor_y < (app->icon->height - 1);
            if(canvasModel.cursor_y > canvasModel.vpy + canvasModel.vph - 3 &&
               canvasModel.vpy < app->icon->height - canvasModel.vph) {
                canvasModel.vpy++;
            }
            break;
        }
        case InputKeyLeft: {
            canvasModel.cursor_x -= canvasModel.cursor_x > 0;
            if(canvasModel.cursor_x > canvasModel.vpx &&
               canvasModel.cursor_x < canvasModel.vpx + 3 && canvasModel.vpx > 0) {
                canvasModel.vpx--;
            }
            break;
        }
        case InputKeyRight: {
            canvasModel.cursor_x += canvasModel.cursor_x < (app->icon->width - 1);
            if(canvasModel.cursor_x > canvasModel.vpx + canvasModel.vpw - 3 &&
               canvasModel.vpx < app->icon->width - canvasModel.vpw) {
                canvasModel.vpx++;
            }
            break;
        }
        case InputKeyBack: {
            if(canvasModel.draw_state == Draw_Point1_Set) {
                canvasModel.draw_state = Draw_NONE;
            } else {
                consumed = false; // send control back to the Tools panel
                app->panel = Panel_Tools;
            }
            break;
        }
        case InputKeyOk: {
            EditorTool tool = tools_get_selected_tool();
            Frame* f = ie_icon_get_current_frame(app->icon);
            switch(tool) {
            case Tool_Draw: {
                size_t offset = canvasModel.cursor_y * app->icon->width + canvasModel.cursor_x;
                f->data[offset] = f->data[offset] ? 0 : 1;
                app->dirty = true;
                break;
            }
            case Tool_Line:
            case Tool_Circle:
            case Tool_Rect: {
                switch(canvasModel.draw_state) {
                case Draw_NONE:
                    canvasModel.p1x = canvasModel.cursor_x;
                    canvasModel.p1y = canvasModel.cursor_y;
                    canvasModel.draw_state = Draw_Point1_Set;
                    break;
                case Draw_Point1_Set: {
                    uint8_t x2 = canvasModel.cursor_x;
                    uint8_t y2 = canvasModel.cursor_y;
                    int bw = app->icon->width;
                    int bh = app->icon->height;
                    // actually draw the line to icon image data
                    if(tool == Tool_Line) {
                        ie_draw_line(f->data, canvasModel.p1x, canvasModel.p1y, x2, y2, bw, bh);
                    } else if(tool == Tool_Circle) {
                        ie_draw_circle(f->data, canvasModel.p1x, canvasModel.p1y, x2, y2, bw, bh);
                    } else if(tool == Tool_Rect) {
                        ie_draw_line(
                            f->data, canvasModel.p1x, canvasModel.p1y, x2, canvasModel.p1y, bw, bh);
                        ie_draw_line(f->data, x2, canvasModel.p1y, x2, y2, bw, bh);
                        ie_draw_line(f->data, x2, y2, canvasModel.p1x, y2, bw, bh);
                        ie_draw_line(
                            f->data, canvasModel.p1x, canvasModel.p1y, canvasModel.p1x, y2, bw, bh);
                    }
                    // reset
                    canvasModel.draw_state = Draw_NONE;
                    app->dirty = true;
                    break;
                }
                default:
                    break;
                }
                break;
            }
            default:
                break;
            }
            break;
        }
        default:
            break;
        }
    }
    return consumed;
}

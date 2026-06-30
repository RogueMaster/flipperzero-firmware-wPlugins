#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "../iconedit.h"

#define LINE_COUNT 5 // number of lines visible in the panel

typedef enum {
    Normal,
    Bold,
    HorizontalRule,
} LineType;

typedef struct {
    LineType type;
    char* text;
} Line;

const Line BlankLine = {Normal, ""};
const Line Separator = {HorizontalRule, ""};

const Line help_text[] = {
    {Bold, "Draw"},
    {Normal, "Pen: OK to toggle pixel"},
    {Normal, "Line/Circle/Rect:"},
    {Normal, "OK to start, then U/D/L/R"},
    {Normal, "OK to end. Back to Cancel"},
    BlankLine,
    {Bold, "View / Play"},
    {Normal, "OK: Pause/Resume"},
    {Normal, "U/D: Zoom In/Out"},
    {Normal, "L/R: Prev/Next Frame"},
    BlankLine,
    {Bold, "Open Icon"},
    {Normal, "Only PNG and BMX"},
    {Normal, "files are supported"},
    BlankLine,
    {Bold, "Save to"},
    {Normal, "Saves files to SDCard"},
    {Normal, "in /apps_data/iconedit"},
    BlankLine,
    {Bold, "Send to PC"},
    {Normal, "Flipper must be connected"},
    {Normal, "to PC via USB."},
    BlankLine,
    {Normal, ".C will send C source code"},
    {Normal, "to the focused app."},
    BlankLine,
    {Normal, "PNG and BMX require use of"},
    {Normal, "image_receive.py script"},
    BlankLine,
    {Normal, "More details on github:"},
    {Normal, "github.com/rdefeo/iconedit"},
};

const int help_text_num_lines = sizeof(help_text) / sizeof(help_text[0]);
int top_line = 0;

void help_draw(Canvas* canvas, void* context) {
    IconEdit* app = context;

    const int x = 0;
    const int y = 7;
    const int pad = 2; // outside padding between frame and line
    const int line_h = 7;
    // int line_w = 90;
    const int line_pad = 2; // the space between

    for(int l = top_line; l < top_line + LINE_COUNT; l++) {
        Line line = help_text[l];
        switch(line.type) {
        case Normal:
            canvas_set_font(canvas, FontSecondary);
            break;
        case Bold:
            canvas_set_font(canvas, FontPrimary);
            break;
        case HorizontalRule:
            canvas_draw_line(
                canvas,
                x + pad + line_pad + 1,
                y + pad + line_pad + (l - top_line) * (line_h + line_pad * 2) + (line_h / 2),
                x + 90,
                y + pad + line_pad + (l - top_line) * (line_h + line_pad * 2) + (line_h / 2));
            break;
        }
        canvas_draw_str_aligned(
            canvas,
            x + pad + 1,
            y + pad + line_pad + (l - top_line) * (line_h + line_pad * 2),
            AlignLeft,
            AlignTop,
            line.text);
    }

    // scrollbar
    const int cw = canvas_width(canvas);
    const int ch = canvas_height(canvas);

    for(int dy = y; dy < ch; dy += 3) {
        canvas_draw_dot(canvas, cw - 1, dy);
    }
    int sh = (LINE_COUNT * (ch - y)) / help_text_num_lines;
    float yp = (top_line * 1.0) / (help_text_num_lines - LINE_COUNT);
    int so = (ch - y - sh) * yp;
    canvas_draw_line(canvas, cw - 1, y + so, cw - 1, y + so + sh);
    if(app->panel == Panel_Help) {
        canvas_draw_line(canvas, cw - 1 - 1, y + so, cw - 1 - 1, y + so + sh);
    }
}

bool help_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp: {
            if(top_line == 0) {
                app->panel = Panel_TabBar;
            } else {
                top_line--;
            }
            break;
        }
        case InputKeyDown: {
            if(top_line < help_text_num_lines - LINE_COUNT) {
                top_line++;
            }
            break;
        }
        case InputKeyBack:
            app->panel = Panel_TabBar;
            break;
        default:
            break;
        }
    }
    return consumed;
}

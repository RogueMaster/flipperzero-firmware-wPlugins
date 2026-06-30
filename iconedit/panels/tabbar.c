#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "../iconedit.h"
#include "../utils/draw.h"

static const char* TAB_NAMES[] = {
    "File",
    "Tools",
    "Settings",
    "Help",
    "About",
};

const Tab Tab_LEFT[Tab_COUNT] = {Tab_NONE, TabFile, TabTools, TabSettings, TabHelp};
const Tab Tab_RIGHT[Tab_COUNT] = {TabTools, TabSettings, TabHelp, TabAbout, Tab_NONE};

const PanelType Tab2Panel[Tab_COUNT] = {
    [TabFile] = Panel_File,
    [TabTools] = Panel_Tools,
    [TabSettings] = Panel_Settings,
    [TabHelp] = Panel_Help,
    [TabAbout] = Panel_About};

static struct {
    Tab tab;
} tabModel = {.tab = TabTools};

Tab tabbar_get_selected_tab() {
    return tabModel.tab;
}

void tabbar_set_selected_tab(Tab tab) {
    tabModel.tab = tab;
}

void tabbar_draw(Canvas* canvas, void* context) {
    IconEdit* app = context;

    // clear the tab strip first? only if we're the selected panel?
    if(app->panel == Panel_TabBar) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 0, 128, 6);
        canvas_set_color(canvas, ColorBlack);
    }

    int widths[Tab_COUNT] = {};
    IEFont fonts[Tab_COUNT] = {};
    int xleft = 0;
    for(Tab t = Tab_START; t < Tab_COUNT; t++) {
        fonts[t] = (tabModel.tab == t) ? Font5x7 : FontMicro;
        widths[t] = ie_draw_get_str_width(canvas, fonts[t], TAB_NAMES[t]);
        widths[t] += 4; // 2px padding on each side
        if(t < tabModel.tab) {
            xleft += widths[t];
        }
    }

    int xorigin = 0;
    if(tabModel.tab > TabFile) {
        xorigin = widths[TabFile];
    }
    int x = -xleft;
    for(Tab t = Tab_START; t < Tab_COUNT; t++) {
        ie_draw_str(canvas, xorigin + x, 0, AlignLeft, AlignTop, fonts[t], TAB_NAMES[t]);
        if(app->panel == Panel_TabBar && tabModel.tab == t) {
            canvas_draw_line(canvas, xorigin + x, 7, xorigin + x + widths[t] - 4, 7);
        }
        x += widths[t];
    }

    // Always clear the toolbar area where the canvas (might) be
    // if(app->panel == Panel_TabBar) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 64, 0, 64, 6);
    canvas_set_color(canvas, ColorBlack);
    // }
}

bool tabbar_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    UNUSED(app);
    bool handled = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyLeft: {
            Tab new_tab = Tab_LEFT[tabModel.tab];
            if(new_tab != Tab_NONE) {
                tabModel.tab = new_tab;
            }
            break;
        }
        case InputKeyRight: {
            Tab new_tab = Tab_RIGHT[tabModel.tab];
            if(new_tab != Tab_NONE) {
                tabModel.tab = new_tab;
            }
            break;
        }
        case InputKeyOk:
        case InputKeyDown:
            app->panel = Tab2Panel[tabModel.tab];
            break;
        case InputKeyBack:
            handled = false;
            break;
        default:
            break;
        }
    }
    return handled;
}

#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "../iconedit.h"

void view_draw(Canvas* canvas, void* context) {
    UNUSED(canvas);
    UNUSED(context);
    // IconEdit* app = context;
}

bool view_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        // case InputKeyLeft: {
        //     Tab new_tab = (Tab)(app->tab - 1);
        //     if(new_tab < 0) new_tab = TabFile;
        //     break;
        // }
        // case InputKeyRight: {
        //     Tab new_tab = (Tab)(app->tab + 1);
        //     if(new_tab > TabAbout) new_tab = TabAbout;
        //     break;
        // }
        // case InputKeyDown:
        //     consumed = false; // input is leaving the tab bar
        //     break;
        case InputKeyUp:
        case InputKeyBack:
            app->panel = Panel_TabBar;
            break;
        default:
            break;
        }
    }
    return consumed;
}

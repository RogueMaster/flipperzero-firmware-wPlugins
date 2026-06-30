#include "../flipcheckers.h"

#define ABOUT_TEXT                                    \
    "\e#Flip Checkers\e# " FLIPCHECKERS_VERSION "\n" \
    "\n"                                              \
    "Classic checkers game for\n"                     \
    "Flipper Zero.\n"                                 \
    "\n"                                              \
    "Features:\n"                                     \
    "- Multi-jump captures\n"                         \
    "- CPU opponents (levels 1-3)\n"                  \
    "- Mandatory jump rule\n"                         \
    "- Save & resume games\n"                         \
    "\n"                                              \
    "Controls:\n"                                     \
    "- D-pad: move cursor\n"                          \
    "- OK: select / confirm\n"                        \
    "- Back: cancel / menu"

void flipcheckers_scene_about_on_enter(void* context) {
    furi_assert(context);
    FlipCheckers* app = context;

    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, ABOUT_TEXT);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCheckersViewIdAbout);
}

bool flipcheckers_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void flipcheckers_scene_about_on_exit(void* context) {
    furi_assert(context);
    FlipCheckers* app = context;
    text_box_reset(app->text_box);
}

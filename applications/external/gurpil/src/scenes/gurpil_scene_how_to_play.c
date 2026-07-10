#include "include/app/gurpil_app.h"

#include "include/ui/render_map.h"

// Kept in one place: the mechanic + the D-pad-to-shape legend, as plain text (no i18n). Must
// stay in sync with shape_for_input_key (include/ui/render_map.h) if the mapping ever changes.
static const char *const GURPIL_HOW_TO_PLAY_TEXT = "\e#How to play\n"
                                                   "Match your wheel to\n"
                                                   "the ground to go\n"
                                                   "faster. Reach check-\n"
                                                   "points for more time.\n"
                                                   "\n"
                                                   "Up: circle\n"
                                                   "Right: line\n"
                                                   "Down: square\n"
                                                   "Left: triangle\n";

void gurpil_scene_how_to_play_on_enter(void *context) {
    GurpilApp *app = context;

    widget_add_text_scroll_element(app->how_to_play, 0, 0, GURPIL_SCREEN_WIDTH,
                                   GURPIL_SCREEN_HEIGHT, GURPIL_HOW_TO_PLAY_TEXT);

    view_dispatcher_switch_to_view(app->view_dispatcher, GurpilViewIdHowToPlay);
}

bool gurpil_scene_how_to_play_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    // Nothing to consume: Back falls through to SceneManager's default (pop to the menu).
    return false;
}

void gurpil_scene_how_to_play_on_exit(void *context) {
    GurpilApp *app = context;
    widget_reset(app->how_to_play);
}

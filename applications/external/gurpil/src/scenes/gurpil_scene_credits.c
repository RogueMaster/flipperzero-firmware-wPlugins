#include "include/app/gurpil_app.h"

#include "include/ui/render_map.h"
#include "include/version.h"

// Kept in one place: author, provenance and license, as plain text (no i18n).
static const char *const GURPIL_CREDITS_TEXT = "\e#Gurpil\n"
                                               "v" GURPIL_VERSION " by Endika\n"
                                               "\n"
                                               "Adaptation of the\n"
                                               "web game Gurpil\n"
                                               "\n"
                                               "License: GPLv3\n"
                                               "github.com/Endika\n"
                                               "/flipper-gurpil\n";

void gurpil_scene_credits_on_enter(void *context) {
    GurpilApp *app = context;

    widget_add_text_scroll_element(app->credits, 0, 0, GURPIL_SCREEN_WIDTH, GURPIL_SCREEN_HEIGHT,
                                   GURPIL_CREDITS_TEXT);

    view_dispatcher_switch_to_view(app->view_dispatcher, GurpilViewIdCredits);
}

bool gurpil_scene_credits_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    // Nothing to consume: Back falls through to SceneManager's default (pop to the menu).
    return false;
}

void gurpil_scene_credits_on_exit(void *context) {
    GurpilApp *app = context;
    widget_reset(app->credits);
}

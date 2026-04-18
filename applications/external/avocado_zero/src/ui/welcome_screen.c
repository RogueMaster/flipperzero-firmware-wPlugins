#include "include/ui/welcome_screen.h"
#include "include/platform/feedback_helper.h"
#include "include/platform/storage_helper.h"
#include <furi.h>
#include <gui/modules/widget.h>

struct WelcomeScreen {
    Widget *widget;
    WelcomeScreenContext ctx;
};

static void welcome_on_start(GuiButtonType button, InputType type, void *context) {
    UNUSED(button);
    UNUSED(type);
    WelcomeScreen *screen = context;

    avocado_onboarding_complete();
    if (screen->ctx.feedback) {
        avocado_feedback_play(screen->ctx.feedback, false);
    }
    view_dispatcher_switch_to_view(screen->ctx.vd, screen->ctx.view_game_index);
}

static void welcome_screen_build(WelcomeScreen *screen) {
    widget_reset(screen->widget);

    const char *text = "\e#Avocado Zero\e#\n\n"
                       "Ready to nurture\n"
                       "your avocado pit\n"
                       "in the glass?\n\n"
                       "Press Start";

    widget_add_text_box_element(screen->widget, 0, 0, 128, 48, AlignCenter, AlignCenter, text,
                                false);
    widget_add_button_element(screen->widget, GuiButtonTypeRight, "Start", welcome_on_start,
                              screen);
}

WelcomeScreen *welcome_screen_alloc(const WelcomeScreenContext *ctx) {
    furi_check(ctx);
    furi_check(ctx->vd);

    WelcomeScreen *screen = malloc(sizeof(WelcomeScreen));
    if (!screen) {
        return NULL;
    }
    screen->widget = widget_alloc();
    if (!screen->widget) {
        free(screen);
        return NULL;
    }
    screen->ctx = *ctx;
    welcome_screen_build(screen);
    return screen;
}

void welcome_screen_free(WelcomeScreen *screen) {
    if (!screen) {
        return;
    }
    widget_free(screen->widget);
    free(screen);
}

View *welcome_screen_get_view(WelcomeScreen *screen) {
    furi_assert(screen);
    return widget_get_view(screen->widget);
}

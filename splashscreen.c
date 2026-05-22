#include <gui/modules/widget.h>

#include "splashscreen.h"
#include <stratahero_icons.h>

struct StrataHeroSplashScreen {
    Widget* widget;

    StrataHeroSplashScreenAdvanceCallback advance_callback;
    void* advance_callback_context;
};

static void stratahero_splash_screen_advance(StrataHeroSplashScreen* screen);

static bool splash_screen_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    StrataHeroSplashScreen* screen = context;

    bool handled = false;
    if (event->type == InputTypePress) {
        switch (event->key) {
            case InputKeyLeft:
            case InputKeyRight:
            case InputKeyUp:
            case InputKeyDown:
            case InputKeyOk:
                stratahero_splash_screen_advance(screen);
                handled = true;
                break;

            default:
                break;
        }
    }

    return handled;
}

StrataHeroSplashScreen* stratahero_splash_screen_alloc() {
    StrataHeroSplashScreen* screen = malloc(sizeof(StrataHeroSplashScreen));
    screen->widget = widget_alloc();
    screen->advance_callback = NULL;
    screen->advance_callback_context = NULL;

    widget_add_icon_element(screen->widget, 35, 5, &I_logo);
    widget_add_string_element(screen->widget, 64, 30, AlignCenter, AlignTop, FontPrimary, "Helldivers");
    widget_add_string_element(screen->widget, 64, 40, AlignCenter, AlignTop, FontPrimary, "Stratagem Hero");

    widget_add_string_element(screen->widget, 64, 55, AlignCenter, AlignTop, FontSecondary, "Press any key");
    View* view = widget_get_view(screen->widget);
    view_set_context(view, screen);
    view_set_input_callback(view, splash_screen_input_callback);
    return screen;
}

void stratahero_splash_screen_free(StrataHeroSplashScreen* screen) {
    widget_free(screen->widget);
    free(screen);
}

View* stratahero_splash_screen_get_view(StrataHeroSplashScreen* screen) {
    return widget_get_view(screen->widget);
}

static void stratahero_splash_screen_advance(StrataHeroSplashScreen* screen) {
    furi_check(screen);
    if (screen->advance_callback) {
        screen->advance_callback(screen->advance_callback_context);
    }
}

void stratahero_splash_screen_set_advance_callback(
    StrataHeroSplashScreen* screen,
    StrataHeroSplashScreenAdvanceCallback callback,
    void* context
) {
    furi_check(screen);
    screen->advance_callback = callback;
    screen->advance_callback_context = context;
}

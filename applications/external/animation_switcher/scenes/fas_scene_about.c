#include "../animation_switcher.h"
#include "fas_scene.h"

#define FAS_VERSION   "1.1"
#define FAS_AUTHOR    "SLK"
#define FAS_GH_AUTHOR "github.com/lsalik2/"
#define FAS_REPO      "FlipperAnimationSwitcher"

void fas_scene_about_on_enter(void* context) {
    FasApp* app = context;
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Animation Switcher");

    widget_add_string_element(
        app->widget,
        64,
        16,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "v" FAS_VERSION " by " FAS_AUTHOR);

    widget_add_string_element(
        app->widget, 64, 26, AlignCenter, AlignTop, FontSecondary, "Source code:");

    widget_add_string_element(
        app->widget, 64, 36, AlignCenter, AlignTop, FontSecondary, FAS_GH_AUTHOR);

    widget_add_string_element(app->widget, 64, 46, AlignCenter, AlignTop, FontSecondary, FAS_REPO);

    view_dispatcher_switch_to_view(app->view_dispatcher, FasViewWidget);
}

bool fas_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void fas_scene_about_on_exit(void* context) {
    FasApp* app = context;
    widget_reset(app->widget);
}

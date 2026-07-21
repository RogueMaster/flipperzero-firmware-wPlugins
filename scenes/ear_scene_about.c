#include "../ear_trainer_i.h"
#include "ear_scene.h"

void ear_scene_about_on_enter(void* context) {
    EarTrainerApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_text_scroll_element(
        widget,
        0,
        0,
        128,
        64,
        "\e#Ear Trainer\e#\n"
        "Recognise musical intervals by ear.\n\n"
        "\e#How it works\e#\n"
        "You hear two notes and name the gap between them. Levels start with "
        "the widest, most distinct intervals and add the close ones later, so "
        "you are never guessing between things that sound alike.\n\n"
        "Each mode keeps its own progress:\n"
        "Ascending - low note first\n"
        "Descending - high note first\n"
        "Mixed - either, at random\n\n"
        "\e#In a quiz\e#\n"
        "Left/Right pick an answer, OK confirms. Up replays the notes, Down "
        "spends a hint to strike out two wrong answers. Back twice leaves.\n\n"
        "\e#Tips\e#\n"
        "Every interval is tagged with a tune that opens with it - hum the "
        "tune, then check whether it matches what you heard.\n\n"
        "Random root is on by default. That is deliberate: it trains the "
        "distance between notes rather than memorising two fixed pitches.\n\n"
        "MIT licensed.");

    view_dispatcher_switch_to_view(app->view_dispatcher, EarViewWidget);
}

bool ear_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ear_scene_about_on_exit(void* context) {
    EarTrainerApp* app = context;
    widget_reset(app->widget);
}

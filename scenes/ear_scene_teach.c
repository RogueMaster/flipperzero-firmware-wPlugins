#include "../ear_trainer_i.h"
#include "ear_scene.h"

/* Introduces each new interval before the quiz: name, size, and a tune that
 * opens with it. OK plays it, Right moves on. */

#define TEACH_ROOT 60 /* C4, fixed here so the shape is what changes, not the pitch */

typedef enum {
    TeachPlay = 10,
    TeachNext,
} TeachEvent;

static void teach_button_callback(GuiButtonType result, InputType type, void* context) {
    EarTrainerApp* app = context;
    if(type != InputTypeShort) return;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, result == GuiButtonTypeCenter ? TeachPlay : TeachNext);
}

static void teach_draw(EarTrainerApp* app) {
    const EarLevel* level = curriculum_get(app->level);
    uint8_t semitones = level->new_intervals[app->teach_index];
    const IntervalInfo* info = interval_get(semitones);
    Widget* widget = app->widget;

    widget_reset(widget);

    char header[32];
    snprintf(header, sizeof(header), "New: %s", info->shortname);
    widget_add_string_element(widget, 64, 11, AlignCenter, AlignBottom, FontPrimary, header);

    widget_add_string_element(widget, 64, 24, AlignCenter, AlignCenter, FontSecondary, info->name);

    char detail[40];
    snprintf(detail, sizeof(detail), "%u semitone%s apart", semitones, semitones == 1 ? "" : "s");
    widget_add_string_element(widget, 64, 36, AlignCenter, AlignCenter, FontSecondary, detail);

    widget_add_string_element(
        widget, 64, 48, AlignCenter, AlignCenter, FontSecondary, info->mnemonic);

    widget_add_button_element(widget, GuiButtonTypeCenter, "Play", teach_button_callback, app);
    bool last = (app->teach_index + 1 >= level->new_count);
    widget_add_button_element(
        widget, GuiButtonTypeRight, last ? "Start" : "Next", teach_button_callback, app);
}

static void teach_play(EarTrainerApp* app) {
    const EarLevel* level = curriculum_get(app->level);
    uint8_t semitones = level->new_intervals[app->teach_index];
    tone_player_play_interval(app->player, TEACH_ROOT, TEACH_ROOT + semitones);
}

void ear_scene_teach_on_enter(void* context) {
    EarTrainerApp* app = context;
    teach_draw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, EarViewWidget);
    teach_play(app); /* hear it immediately, that is the point of the screen */
}

bool ear_scene_teach_on_event(void* context, SceneManagerEvent event) {
    EarTrainerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == TeachPlay) {
        teach_play(app);
        return true;
    }
    if(event.event == TeachNext) {
        const EarLevel* level = curriculum_get(app->level);
        if(app->teach_index + 1 < level->new_count) {
            app->teach_index++;
            teach_draw(app);
            teach_play(app);
        } else {
            scene_manager_next_scene(app->scene_manager, EarSceneQuiz);
        }
        return true;
    }
    return false;
}

void ear_scene_teach_on_exit(void* context) {
    EarTrainerApp* app = context;
    tone_player_stop(app->player);
    widget_reset(app->widget);
}

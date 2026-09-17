#include "../sigroam.h"

static const char k_raw_empty[] = "No unknown lines yet.";

static void sigroam_scene_raw_copy_empty(char* dst, size_t cap) {
    size_t i;

    if(dst == NULL || cap == 0) {
        return;
    }
    for(i = 0; k_raw_empty[i] != '\0' && (i + 1u) < cap; i++) {
        dst[i] = k_raw_empty[i];
    }
    dst[i] = '\0';
}

/* Re-rendering raw_text and calling text_box_set_text must happen as a pair (text_box stores the
 * pointer, it does not copy). */
static void sigroam_scene_raw_commit_text(SigRoamApp* app) {
    if(app->rawlog.count == 0) {
        sigroam_scene_raw_copy_empty(app->raw_text, sizeof(app->raw_text));
    } else {
        sr_rawlog_render(&app->rawlog, app->raw_text, sizeof(app->raw_text));
    }
    text_box_set_text(app->text_box, app->raw_text);
    app->raw_pushed_shown = app->rawlog.pushed;
}

void sigroam_scene_raw_on_enter(void* context) {
    SigRoamApp* app = context;

    /* Entered via the custom callback, so app->mtx is already held. Do not acquire again. */
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusEnd);
    sigroam_scene_raw_commit_text(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewTextBox);
}

bool sigroam_scene_raw_on_event(void* context, SceneManagerEvent event) {
    SigRoamApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        /* The tick callback is already inside app->mtx (sigroam.c takes the lock with zero wait
         * before dispatching). Do not acquire again. */
        if(sr_rawlog_should_render(app->rawlog.pushed, app->raw_pushed_shown)) {
            sigroam_scene_raw_commit_text(app);
        }
        return true;
    }
    /* Up/Down are handled by text_box itself; Back goes to the ViewDispatcher. */
    return false;
}

void sigroam_scene_raw_on_exit(void* context) {
    SigRoamApp* app = context;
    text_box_reset(app->text_box);
}

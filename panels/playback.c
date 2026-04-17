#include <gui/canvas.h>
#include <input/input.h>

#include "panels.h"
#include "../iconedit.h"
#include "../icon.h"
#include "utils/draw.h"
#include <iconedit_icons.h>

static struct {
    IEIconAnimation* anim;
    size_t scale;
    IEIconAnimationCallback callback;
    void* callback_context;
} playbackModel = {.anim = NULL, .scale = 5, .callback = NULL, .callback_context = NULL};

void playback_start(IEIcon* icon) {
    if(!playbackModel.callback) {
        FURI_LOG_E(TAG, "Calback not set for playback!");
        return;
    }

    // wipe out any previous animation data
    if(playbackModel.anim) {
        FURI_LOG_W(TAG, "Playback animation already exists, cleaning up");
        ie_icon_animation_stop(playbackModel.anim);
        ie_icon_animation_free(playbackModel.anim);
    }
    playbackModel.anim = ie_icon_animation_alloc(icon);
    ie_icon_animation_set_update_callback(
        playbackModel.anim, playbackModel.callback, playbackModel.callback_context);
    ie_icon_animation_start(playbackModel.anim);
}

void playback_pause_resume() {
    playbackModel.anim->animating = !playbackModel.anim->animating;
}

void playback_stop() {
    ie_icon_animation_stop(playbackModel.anim);
    ie_icon_animation_free(playbackModel.anim);
    playbackModel.anim = NULL;
}

void playback_set_update_callback(IEIconAnimationCallback callback, void* context) {
    playbackModel.callback = callback;
    playbackModel.callback_context = context;
}

void playback_draw(Canvas* canvas, void* context) {
    UNUSED(context);
    // IconEdit* app = context;

    // center the icon on screen
    IEIcon* icon = playbackModel.anim->icon;

    int x = (128 - (icon->width * playbackModel.scale)) / 2;
    int y = (64 - (icon->height * playbackModel.scale)) / 2;

    Frame* frame = ie_icon_animation_get_current_frame(playbackModel.anim);
    for(size_t i = 0; i < icon->width * icon->height; i++) {
        if(frame->data[i]) {
            canvas_draw_box(
                canvas,
                x + (i % icon->width) * playbackModel.scale,
                y + (i / icon->width) * playbackModel.scale,
                playbackModel.scale,
                playbackModel.scale);
        }
    }
    if(icon->frame_count == 1 || !playbackModel.anim->animating) {
        char scale[6];
        snprintf(scale, sizeof(scale), "%dx", playbackModel.scale);
        ie_draw_str(canvas, 0, 64 - 7, AlignLeft, AlignTop, Font5x7, scale);
        if(!playbackModel.anim->animating) {
            canvas_draw_icon(canvas, 10, 64 - 10, &I_iet_Pause);
        }
    }
}

bool playback_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp: {
            playbackModel.scale += playbackModel.scale + 1 <= 10;
            break;
        }
        case InputKeyDown: {
            playbackModel.scale -= playbackModel.scale - 1 > 0;
            break;
        }
        case InputKeyLeft: {
            if(!playbackModel.anim->animating) {
                ie_icon_animation_prev_frame(playbackModel.anim);
            }
            break;
        }
        case InputKeyRight: {
            if(!playbackModel.anim->animating) {
                ie_icon_animation_next_frame(playbackModel.anim);
            }
            break;
        }
        case InputKeyOk:
            if(playbackModel.anim->icon->frame_count > 1) {
                playback_pause_resume();
            }
            break;
        case InputKeyBack:
            app->panel = Panel_Tools;
            playback_stop();
            break;
        default:
            break;
        }
    }
    return consumed;
}

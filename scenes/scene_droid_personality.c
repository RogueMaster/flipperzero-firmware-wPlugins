#include <gui/elements.h>

#include "../disney_toolbox_app.h"
#include "disney_toolbox_icons.h"

#define DROID_ANIMATION_FRAMES 5
#define DROID_ANIMATION_MOD 2

static const Icon* droid_animation[] = {
    &I_personality_frame_0,  //
    &I_personality_frame_1,  //
    &I_personality_frame_2,  //
    &I_personality_frame_3,  //
    &I_personality_frame_4,  //
};

enum {
    DroidViewEventPrev,
    DroidViewEventNext,
    DroidViewEventToggleBroadcast,
    DroidViewEventTogglePaired,
};

static void droid_personality_view_draw(Canvas* canvas, void* model) {
    DisneyToolboxApp* app = *(DisneyToolboxApp**)model;
    if (!app) return;

    const DroidPersonalityInfo* info = &droid_personality_info[app->selected_droid_personality];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "Personality Broadcaster");

    canvas_set_font(canvas, FontSecondary);
    char buf[40];
    uint8_t y = 19;

    if (info->chip_color) {
        snprintf(buf, sizeof(buf), "Chip Color: %s", info->chip_color);
        canvas_draw_str(canvas, 0, y, buf);
        y += 10;
    }
    if (info->droid) {
        snprintf(buf, sizeof(buf), "Droid: %s", info->droid);
        canvas_draw_str(canvas, 0, y, buf);
        y += 10;
    }

    snprintf(buf, sizeof(buf), "Affiliation: %s", droid_affiliation_names[info->affiliation]);
    canvas_draw_str(canvas, 0, y, buf);
    y += 10;

    snprintf(buf, sizeof(buf), "Paired: %s", app->droid_paired ? "Yes" : "No");
    canvas_draw_str(canvas, 0, y, buf);
    // Filled up triangle (left), slash, filled down triangle (right)
    uint8_t ax = 49;
    uint8_t ay = y - 5;  // vertical center of arrow area
    // Up triangle (filled) - 7px wide, 4px tall
    canvas_draw_line(canvas, ax, ay + 3, ax + 6, ay + 3);
    canvas_draw_line(canvas, ax + 1, ay + 2, ax + 5, ay + 2);
    canvas_draw_line(canvas, ax + 2, ay + 1, ax + 4, ay + 1);
    canvas_draw_dot(canvas, ax + 3, ay);
    // Diagonal slash between triangles
    canvas_draw_line(canvas, ax + 8, ay + 3, ax + 11, ay);
    // Down triangle (filled) - 7px wide, 4px tall
    uint8_t dx = ax + 13;
    canvas_draw_line(canvas, dx, ay, dx + 6, ay);
    canvas_draw_line(canvas, dx + 1, ay + 1, dx + 5, ay + 1);
    canvas_draw_line(canvas, dx + 2, ay + 2, dx + 4, ay + 2);
    canvas_draw_dot(canvas, dx + 3, ay + 3);

    // Broadcast animation
    if (app->is_beacon_active) {
        uint8_t frame = app->animation_counter / DROID_ANIMATION_MOD;
        if (frame >= DROID_ANIMATION_FRAMES) frame = DROID_ANIMATION_FRAMES - 1;
        canvas_draw_icon(canvas, 93, 15, droid_animation[frame]);
    } else {
        canvas_draw_icon(canvas, 93, 15, droid_animation[0]);
    }

    // Button bar
    if (app->selected_droid_personality > 0) {
        elements_button_left(canvas, "Prev");
    }
    if (app->selected_droid_personality < DroidPersonalityCount - 1) {
        elements_button_right(canvas, "Next");
    }
    elements_button_center(canvas, app->is_beacon_active ? "Stop" : "Start");
}

static bool droid_personality_view_input(InputEvent* event, void* context) {
    DisneyToolboxApp* app = context;

    if (event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    switch (event->key) {
        case InputKeyLeft:
            view_dispatcher_send_custom_event(app->view_dispatcher, DroidViewEventPrev);
            return true;
        case InputKeyRight:
            view_dispatcher_send_custom_event(app->view_dispatcher, DroidViewEventNext);
            return true;
        case InputKeyOk:
            view_dispatcher_send_custom_event(app->view_dispatcher, DroidViewEventToggleBroadcast);
            return true;
        case InputKeyUp:
        case InputKeyDown:
            view_dispatcher_send_custom_event(app->view_dispatcher, DroidViewEventTogglePaired);
            return true;
        default:
            return false;
    }
}

static void droid_personality_apply_beacon(DisneyToolboxApp* app) {
    furi_hal_bt_extra_beacon_stop();

    app->beacon_data_len =
        droid_beacon_generate(app->selected_droid_personality, app->droid_paired, app->beacon_data);

    furi_check(furi_hal_bt_extra_beacon_set_config(&app->beacon_config));
    furi_check(furi_hal_bt_extra_beacon_set_data(app->beacon_data, app->beacon_data_len));

    if (app->is_beacon_active) {
        furi_check(furi_hal_bt_extra_beacon_start());
    }
}

static void droid_personality_redraw(DisneyToolboxApp* app) {
    view_commit_model(app->droid_view, true);
}

void disney_toolbox_app_scene_droid_personality_on_enter(void* context) {
    DisneyToolboxApp* app = context;

    view_set_draw_callback(app->droid_view, droid_personality_view_draw);
    view_set_input_callback(app->droid_view, droid_personality_view_input);
    view_set_context(app->droid_view, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyToolboxAppViewDroid);
}

bool disney_toolbox_app_scene_droid_personality_on_event(void* context, SceneManagerEvent event) {
    DisneyToolboxApp* app = context;

    if (event.type == SceneManagerEventTypeCustom) {
        switch (event.event) {
            case DroidViewEventPrev:
                if (app->selected_droid_personality > 0) {
                    app->selected_droid_personality--;
                    if (app->is_beacon_active) droid_personality_apply_beacon(app);
                    droid_personality_redraw(app);
                }
                return true;
            case DroidViewEventNext:
                if (app->selected_droid_personality < DroidPersonalityCount - 1) {
                    app->selected_droid_personality++;
                    if (app->is_beacon_active) droid_personality_apply_beacon(app);
                    droid_personality_redraw(app);
                }
                return true;
            case DroidViewEventToggleBroadcast:
                app->is_beacon_active = !app->is_beacon_active;
                droid_personality_apply_beacon(app);
                if (app->is_beacon_active) {
                    notification_message_block(app->notifications, &sequence_set_blue_255);
                } else {
                    notification_message_block(app->notifications, &sequence_reset_blue);
                    app->animation_counter = 0;
                }
                droid_personality_redraw(app);
                return true;
            case DroidViewEventTogglePaired:
                app->droid_paired = !app->droid_paired;
                if (app->is_beacon_active) droid_personality_apply_beacon(app);
                droid_personality_redraw(app);
                return true;
        }
    } else if (event.type == SceneManagerEventTypeTick) {
        if (app->is_beacon_active) {
            uint8_t max = (DROID_ANIMATION_FRAMES * DROID_ANIMATION_MOD) - 1;
            app->animation_counter = (app->animation_counter + 1) % (max + 1);
            droid_personality_redraw(app);
        }
        return true;
    }
    return false;
}

void disney_toolbox_app_scene_droid_personality_on_exit(void* context) {
    DisneyToolboxApp* app = context;

    if (app->is_beacon_active) {
        furi_hal_bt_extra_beacon_stop();
        app->is_beacon_active = false;
        notification_message_block(app->notifications, &sequence_reset_blue);
    }

    app->animation_counter = 0;
}

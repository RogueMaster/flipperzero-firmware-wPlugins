#include "../switch_controller_app.h"

typedef struct {
    bool playing;
    bool loop;
    uint32_t current_event;
    uint32_t total_events;
    SwitchControllerState state;
} PlayingViewModel;

static void switch_controller_view_playing_draw_callback(Canvas* canvas, void* model) {
    PlayingViewModel* m = model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Playing Macro");

    canvas_set_font(canvas, FontSecondary);

    if(m->playing) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Event: %lu / %lu", m->current_event, m->total_events);
        canvas_draw_str(canvas, 2, 25, buf);

        if(m->loop) {
            canvas_draw_str(canvas, 2, 35, "Mode: Loop");
        } else {
            canvas_draw_str(canvas, 2, 35, "Mode: Once");
        }

        canvas_draw_str(canvas, 2, 50, "Playing...");
        canvas_draw_str(canvas, 2, 60, "Press Back to stop");
    } else {
        canvas_draw_str(canvas, 2, 25, "Finished playing");
        canvas_draw_str(canvas, 2, 35, "Press Back to exit");
    }
}

static bool switch_controller_view_playing_input_callback(InputEvent* event, void* context) {
    SwitchControllerApp* app = context;
    bool consumed = false;

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        // Stop playing and go back
        macro_player_stop(app->player);
        view_dispatcher_send_custom_event(app->view_dispatcher, 0);
        consumed = true;
    }

    return consumed;
}

static void switch_controller_view_playing_timer_callback(void* context) {
    SwitchControllerApp* app = context;

    with_view_model(
        app->playing_view,
        PlayingViewModel * model,
        {
            if(model->playing) {
                // Update macro player
                bool still_playing = macro_player_update(app->player, &model->state);
                model->current_event = app->player->current_event_index;

                if(!still_playing) {
                    model->playing = false;
                }

                // Send USB report
                if(app->usb_connected) {
                    usb_hid_switch_send_report(&model->state);
                }
            }
        },
        true);
}

void switch_controller_scene_on_enter_playing(void* context) {
    SwitchControllerApp* app = context;

    // Create playing view if it doesn't exist
    if(!app->playing_view) {
        app->playing_view = view_alloc();
        view_set_context(app->playing_view, app);
        view_allocate_model(app->playing_view, ViewModelTypeLocking, sizeof(PlayingViewModel));
        view_set_draw_callback(app->playing_view, switch_controller_view_playing_draw_callback);
        view_set_input_callback(app->playing_view, switch_controller_view_playing_input_callback);
        view_dispatcher_add_view(
            app->view_dispatcher, SwitchControllerViewPlaying, app->playing_view);
    }

    // Initialize USB
    app->usb_mode_prev = furi_hal_usb_get_config();
    if(usb_hid_switch_init()) {
        app->usb_connected = true;
    }

    // Start playing macro (loop by default)
    bool loop = true; // TODO: make this configurable
    macro_player_start(app->player, app->current_macro, loop);

    // Initialize model
    with_view_model(
        app->playing_view,
        PlayingViewModel * model,
        {
            model->playing = true;
            model->loop = loop;
            model->current_event = 0;
            model->total_events = app->current_macro->event_count;
            usb_hid_switch_reset_state(&model->state);
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, SwitchControllerViewPlaying);

    // Set up timer for periodic updates
    if(!app->timer) {
        app->timer = furi_timer_alloc(
            switch_controller_view_playing_timer_callback, FuriTimerTypePeriodic, app);
    }
    furi_timer_start(app->timer, 10); // 10ms = ~100Hz update rate
}

bool switch_controller_scene_on_event_playing(void* context, SceneManagerEvent event) {
    SwitchControllerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Playing stopped, go back
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SwitchControllerSceneMenu);
        consumed = true;
    }

    return consumed;
}

void switch_controller_scene_on_exit_playing(void* context) {
    SwitchControllerApp* app = context;

    // Stop timer
    if(app->timer) {
        furi_timer_stop(app->timer);
    }

    // Deinitialize USB
    if(app->usb_connected) {
        usb_hid_switch_deinit();
        if(app->usb_mode_prev) {
            furi_hal_usb_set_config(app->usb_mode_prev, NULL);
        }
        app->usb_connected = false;
    }
}

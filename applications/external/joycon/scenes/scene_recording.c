#include "../switch_controller_app.h"
#include <input/input.h>

// Control modes (same as controller)
typedef enum {
    ControlModeDPad = 0,
    ControlModeLeftStick,
    ControlModeRightStick,
    ControlModeCount,
} ControlMode;

typedef struct {
    bool recording;
    uint32_t event_count;
    uint32_t duration_ms;
    SwitchControllerState state;
    ControlMode control_mode;
} RecordingViewModel;

static void switch_controller_view_recording_draw_callback(Canvas* canvas, void* model) {
    RecordingViewModel* m = model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Recording Macro");

    canvas_set_font(canvas, FontSecondary);

    // Show control mode
    switch(m->control_mode) {
    case ControlModeDPad:
        canvas_draw_str(canvas, 2, 20, "Mode: D-Pad");
        break;
    case ControlModeLeftStick:
        canvas_draw_str(canvas, 2, 20, "Mode: Left Stick");
        break;
    case ControlModeRightStick:
        canvas_draw_str(canvas, 2, 20, "Mode: Right Stick");
        break;
    default:
        break;
    }

    // Show recording status
    if(m->recording) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Events: %lu", m->event_count);
        canvas_draw_str(canvas, 2, 32, buf);

        snprintf(
            buf,
            sizeof(buf),
            "Time: %lu.%lus",
            m->duration_ms / 1000,
            (m->duration_ms % 1000) / 100);
        canvas_draw_str(canvas, 2, 42, buf);

        canvas_draw_str(canvas, 2, 54, "Recording...");
        canvas_draw_str(canvas, 2, 63, "Back: Stop");
    } else {
        canvas_draw_str(canvas, 2, 32, "Ready to record");
        canvas_draw_str(canvas, 2, 42, "OK: Start");
        canvas_draw_str(canvas, 2, 52, "Long Back: Mode");
    }
}

static bool switch_controller_view_recording_input_callback(InputEvent* event, void* context) {
    SwitchControllerApp* app = context;
    bool consumed = false;

    with_view_model(
        app->recording_view,
        RecordingViewModel * model,
        {
            if(!model->recording) {
                // Not recording yet - wait for OK to start
                if(event->key == InputKeyOk && event->type == InputTypePress) {
                    model->recording = true;
                    macro_recorder_start(
                        app->recorder, app->current_macro, app->macro_name_buffer);
                    consumed = true;
                } else if(event->key == InputKeyBack && event->type == InputTypeLong) {
                    // Toggle control mode before recording
                    model->control_mode = (model->control_mode + 1) % ControlModeCount;
                    consumed = true;
                }
            } else {
                // Recording - capture inputs
                bool state_changed = false;

                if(event->key == InputKeyBack && event->type == InputTypeShort) {
                    // Stop recording
                    macro_recorder_stop(app->recorder);
                    model->recording = false;

                    // Save macro
                    FuriString* path = furi_string_alloc();
                    furi_string_printf(
                        path,
                        "%s/%s%s",
                        SWITCH_CONTROLLER_APP_PATH_FOLDER,
                        app->macro_name_buffer,
                        SWITCH_CONTROLLER_MACRO_EXTENSION);
                    macro_save(app->current_macro, furi_string_get_cstr(path));
                    furi_string_free(path);

                    // Go back to menu
                    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
                    consumed = true;
                } else {
                    // Update controller state based on input and control mode
                    if(event->key == InputKeyUp) {
                        if(event->type == InputTypePress) {
                            if(model->control_mode == ControlModeDPad) {
                                model->state.hat = SWITCH_HAT_UP;
                            } else if(model->control_mode == ControlModeLeftStick) {
                                model->state.ly = 0; // Full up (8-bit)
                            } else if(model->control_mode == ControlModeRightStick) {
                                model->state.ry = 0; // Full up (8-bit)
                            }
                            state_changed = true;
                        } else if(event->type == InputTypeRelease) {
                            if(model->control_mode == ControlModeDPad) {
                                model->state.hat = SWITCH_HAT_NEUTRAL;
                            } else if(model->control_mode == ControlModeLeftStick) {
                                model->state.ly = STICK_CENTER; // 128
                            } else if(model->control_mode == ControlModeRightStick) {
                                model->state.ry = STICK_CENTER; // 128
                            }
                            state_changed = true;
                        }
                    } else if(event->key == InputKeyDown) {
                        if(event->type == InputTypePress) {
                            if(model->control_mode == ControlModeDPad) {
                                model->state.hat = SWITCH_HAT_DOWN;
                            } else if(model->control_mode == ControlModeLeftStick) {
                                model->state.ly = 255; // Full down (8-bit)
                            } else if(model->control_mode == ControlModeRightStick) {
                                model->state.ry = 255; // Full down (8-bit)
                            }
                            state_changed = true;
                        } else if(event->type == InputTypeRelease) {
                            if(model->control_mode == ControlModeDPad) {
                                model->state.hat = SWITCH_HAT_NEUTRAL;
                            } else if(model->control_mode == ControlModeLeftStick) {
                                model->state.ly = STICK_CENTER; // 128
                            } else if(model->control_mode == ControlModeRightStick) {
                                model->state.ry = STICK_CENTER; // 128
                            }
                            state_changed = true;
                        }
                    } else if(event->key == InputKeyLeft) {
                        if(event->type == InputTypePress) {
                            if(model->control_mode == ControlModeDPad) {
                                model->state.hat = SWITCH_HAT_LEFT;
                            } else if(model->control_mode == ControlModeLeftStick) {
                                model->state.lx = 0; // Full left (8-bit)
                            } else if(model->control_mode == ControlModeRightStick) {
                                model->state.rx = 0; // Full left (8-bit)
                            }
                            state_changed = true;
                        } else if(event->type == InputTypeRelease) {
                            if(model->control_mode == ControlModeDPad) {
                                model->state.hat = SWITCH_HAT_NEUTRAL;
                            } else if(model->control_mode == ControlModeLeftStick) {
                                model->state.lx = STICK_CENTER; // 128
                            } else if(model->control_mode == ControlModeRightStick) {
                                model->state.rx = STICK_CENTER; // 128
                            }
                            state_changed = true;
                        }
                    } else if(event->key == InputKeyRight) {
                        if(event->type == InputTypePress) {
                            if(model->control_mode == ControlModeDPad) {
                                model->state.hat = SWITCH_HAT_RIGHT;
                            } else if(model->control_mode == ControlModeLeftStick) {
                                model->state.lx = 255; // Full right (8-bit)
                            } else if(model->control_mode == ControlModeRightStick) {
                                model->state.rx = 255; // Full right (8-bit)
                            }
                            state_changed = true;
                        } else if(event->type == InputTypeRelease) {
                            if(model->control_mode == ControlModeDPad) {
                                model->state.hat = SWITCH_HAT_NEUTRAL;
                            } else if(model->control_mode == ControlModeLeftStick) {
                                model->state.lx = STICK_CENTER; // 128
                            } else if(model->control_mode == ControlModeRightStick) {
                                model->state.rx = STICK_CENTER; // 128
                            }
                            state_changed = true;
                        }
                    } else if(event->key == InputKeyOk) {
                        if(event->type == InputTypePress) {
                            model->state.buttons |= SWITCH_BTN_A;
                            state_changed = true;
                        } else if(event->type == InputTypeRelease) {
                            model->state.buttons &= ~SWITCH_BTN_A;
                            state_changed = true;
                        }
                    }

                    if(state_changed) {
                        // Record the state change
                        macro_recorder_update(app->recorder, &model->state);
                        model->event_count = app->current_macro->event_count;
                        model->duration_ms = furi_get_tick() - app->recorder->start_time;
                    }

                    consumed = true;
                }
            }
        },
        consumed);

    return consumed;
}

void switch_controller_scene_on_enter_recording(void* context) {
    SwitchControllerApp* app = context;

    // Create recording view if it doesn't exist
    if(!app->recording_view) {
        app->recording_view = view_alloc();
        view_set_context(app->recording_view, app);
        view_allocate_model(app->recording_view, ViewModelTypeLocking, sizeof(RecordingViewModel));
        view_set_draw_callback(
            app->recording_view, switch_controller_view_recording_draw_callback);
        view_set_input_callback(
            app->recording_view, switch_controller_view_recording_input_callback);
        view_dispatcher_add_view(
            app->view_dispatcher, SwitchControllerViewRecording, app->recording_view);
    }

    // Initialize model
    with_view_model(
        app->recording_view,
        RecordingViewModel * model,
        {
            model->recording = false;
            model->event_count = 0;
            model->duration_ms = 0;
            model->control_mode = ControlModeDPad;
            usb_hid_switch_reset_state(&model->state);
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, SwitchControllerViewRecording);
}

bool switch_controller_scene_on_event_recording(void* context, SceneManagerEvent event) {
    SwitchControllerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Recording stopped, go back to menu
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SwitchControllerSceneMenu);
        consumed = true;
    }

    return consumed;
}

void switch_controller_scene_on_exit_recording(void* context) {
    UNUSED(context);
}

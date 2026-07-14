#include "../switch_controller_app.h"
#include <input/input.h>

// Control modes
typedef enum {
    ControlModeDPad = 0,
    ControlModeLeftStick,
    ControlModeRightStick,
    ControlModeCount,
} ControlMode;

// Controller view model
typedef struct {
    SwitchControllerState state;
    bool connected;
    uint8_t selected_button; // For button menu (0 = none, 1-9 = button index)
    ControlMode control_mode; // Current control mode
    bool exit_confirm; // Exit confirmation dialog
    uint32_t last_ok_press_time; // For double-click detection
    uint16_t button_to_release; // Button that needs to be auto-released
    uint32_t button_release_time; // When to release the button
} ControllerViewModel;

// Button menu items (for long press OK)
static const char* button_menu_items[] = {
    "X",
    "Y",
    "L",
    "R",
    "ZL",
    "ZR",
    "Plus",
    "Minus",
    "Home",
};

static const uint16_t button_menu_flags[] = {
    SWITCH_BTN_X,
    SWITCH_BTN_Y,
    SWITCH_BTN_L,
    SWITCH_BTN_R,
    SWITCH_BTN_ZL,
    SWITCH_BTN_ZR,
    SWITCH_BTN_PLUS,
    SWITCH_BTN_MINUS,
    SWITCH_BTN_HOME,
};

// Draw callback
static void switch_controller_view_controller_draw_callback(Canvas* canvas, void* model) {
    ControllerViewModel* m = model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    // Show exit confirmation dialog
    if(m->exit_confirm) {
        canvas_draw_frame(canvas, 5, 10, 118, 44);
        canvas_draw_box(canvas, 6, 11, 116, 42);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, "Exit App?");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignCenter, "Press LEFT to confirm");
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, "Press BACK to cancel");
        canvas_set_color(canvas, ColorBlack);
        return;
    }

    if(m->connected) {
        canvas_draw_str(canvas, 2, 10, "Switch Controller");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 20, "Connected (POKKEN)");
    } else {
        canvas_draw_str(canvas, 2, 10, "Switch Controller");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 20, "Not Connected");
    }

    // Draw control mode
    canvas_set_font(canvas, FontSecondary);
    switch(m->control_mode) {
    case ControlModeDPad:
        canvas_draw_str(canvas, 2, 30, "Mode: D-Pad");
        break;
    case ControlModeLeftStick:
        canvas_draw_str(canvas, 2, 30, "Mode: Left Stick");
        break;
    case ControlModeRightStick:
        canvas_draw_str(canvas, 2, 30, "Mode: Right Stick");
        break;
    default:
        break;
    }

    // Debug: Show current state
    char debug_buf[32];
    snprintf(debug_buf, sizeof(debug_buf), "B:%04X H:%X", m->state.buttons, m->state.hat);
    canvas_draw_str(canvas, 2, 40, debug_buf);

    snprintf(
        debug_buf,
        sizeof(debug_buf),
        "L:%d,%d R:%d,%d",
        m->state.lx,
        m->state.ly,
        m->state.rx,
        m->state.ry);
    canvas_draw_str(canvas, 2, 50, debug_buf);

    // Draw instructions
    canvas_draw_str(canvas, 2, 60, "OK:A Back:B LongOK:Menu");

    // Show button menu if active
    if(m->selected_button > 0) {
        // Draw filled background
        canvas_draw_box(canvas, 8, 13, 112, 42);
        canvas_set_color(canvas, ColorWhite);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 12, 22, "Select Button:");

        canvas_set_font(canvas, FontSecondary);
        uint8_t idx = m->selected_button - 1;

        // Draw previous item (if exists)
        if(idx > 0) {
            canvas_draw_str(canvas, 20, 32, button_menu_items[idx - 1]);
        }

        // Draw current item (highlighted)
        canvas_draw_str(canvas, 12, 40, ">");
        canvas_draw_str(canvas, 20, 40, button_menu_items[idx]);

        // Draw next item (if exists)
        if(idx < 8) {
            canvas_draw_str(canvas, 20, 48, button_menu_items[idx + 1]);
        }

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 8, 13, 112, 42);
    }
}

// Timer callback for continuous USB updates
static void switch_controller_timer_callback(void* context) {
    SwitchControllerApp* app = context;

    // Safety checks
    if(!app || !app->controller_view) {
        return;
    }

    with_view_model(
        app->controller_view,
        ControllerViewModel * model,
        {
            if(model && model->connected) {
                // Check if we need to auto-release a button
                if(model->button_to_release != 0) {
                    uint32_t current_time = furi_get_tick();
                    if(current_time >= model->button_release_time) {
                        model->state.buttons &= ~model->button_to_release;
                        model->button_to_release = 0;
                    }
                }

                usb_hid_switch_send_report(&model->state);
            }
        },
        false);
}

// Input callback
static bool switch_controller_view_controller_input_callback(InputEvent* event, void* context) {
    SwitchControllerApp* app = context;
    bool consumed = false;

    // Handle exit confirmation OUTSIDE of with_view_model
    bool exit_confirm_shown = false;
    with_view_model(
        app->controller_view,
        ControllerViewModel * model,
        { exit_confirm_shown = model->exit_confirm; },
        false);

    if(exit_confirm_shown) {
        if(event->type == InputTypeShort) {
            if(event->key == InputKeyLeft) {
                // Confirm exit - stop the app
                view_dispatcher_stop(app->view_dispatcher);
                return true;
            } else if(event->key == InputKeyBack) {
                // Cancel exit
                with_view_model(
                    app->controller_view,
                    ControllerViewModel * model,
                    { model->exit_confirm = false; },
                    true);
                return true;
            }
        }
        return true; // Consume all events when exit dialog is shown
    }

    with_view_model(
        app->controller_view,
        ControllerViewModel * model,
        {
            if(model->selected_button > 0) {
                // In button menu - consume ALL events to prevent dismissal
                if(event->type == InputTypePress || event->type == InputTypeRepeat) {
                    if(event->key == InputKeyUp) {
                        if(model->selected_button > 1) {
                            model->selected_button--;
                        }
                        consumed = true;
                    } else if(event->key == InputKeyDown) {
                        if(model->selected_button < 9) {
                            model->selected_button++;
                        }
                        consumed = true;
                    } else if(event->key == InputKeyOk) {
                        // Press selected button and schedule auto-release after 100ms
                        uint8_t idx = model->selected_button - 1;
                        uint16_t button_flag = button_menu_flags[idx];
                        model->state.buttons |= button_flag;
                        model->button_to_release = button_flag;
                        model->button_release_time = furi_get_tick() + 100; // 100ms press
                        model->selected_button = 0; // Close menu
                        consumed = true;
                    } else if(event->key == InputKeyBack) {
                        model->selected_button = 0; // Close menu
                        consumed = true;
                    }
                } else if(event->type == InputTypeRelease) {
                    // Consume release events for buttons that were pressed
                    if(event->key == InputKeyUp || event->key == InputKeyDown ||
                       event->key == InputKeyOk || event->key == InputKeyBack) {
                        consumed = true;
                    }
                } else {
                    // Consume all other event types when menu is active
                    consumed = true;
                }
            } else {
                // Normal controller mode
                // Handle directional inputs based on control mode
                if(event->key == InputKeyUp) {
                    if(event->type == InputTypePress) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_UP;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.ly = 0; // Full up (8-bit: 0 = top)
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.ry = 0; // Full up (8-bit: 0 = top)
                        }
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_NEUTRAL;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.ly = STICK_CENTER; // 128
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.ry = STICK_CENTER; // 128
                        }
                        consumed = true;
                    }
                } else if(event->key == InputKeyDown) {
                    if(event->type == InputTypePress) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_DOWN;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.ly = 255; // Full down (8-bit: 255 = bottom)
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.ry = 255; // Full down (8-bit: 255 = bottom)
                        }
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_NEUTRAL;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.ly = STICK_CENTER; // 128
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.ry = STICK_CENTER; // 128
                        }
                        consumed = true;
                    }
                } else if(event->key == InputKeyLeft) {
                    if(event->type == InputTypePress) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_LEFT;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.lx = 0; // Full left (8-bit: 0 = left)
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.rx = 0; // Full left (8-bit: 0 = left)
                        }
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_NEUTRAL;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.lx = STICK_CENTER; // 128
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.rx = STICK_CENTER; // 128
                        }
                        consumed = true;
                    }
                } else if(event->key == InputKeyRight) {
                    if(event->type == InputTypePress) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_RIGHT;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.lx = 255; // Full right (8-bit: 255 = right)
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.rx = 255; // Full right (8-bit: 255 = right)
                        }
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_NEUTRAL;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.lx = STICK_CENTER; // 128
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.rx = STICK_CENTER; // 128
                        }
                        consumed = true;
                    }
                } else if(event->key == InputKeyOk) {
                    if(event->type == InputTypePress) {
                        model->state.buttons |= SWITCH_BTN_A;
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        model->state.buttons &= ~SWITCH_BTN_A;
                        consumed = true;
                    } else if(event->type == InputTypeLong) {
                        // Open button menu
                        model->selected_button = 1;
                        // Release A button when menu opens
                        model->state.buttons &= ~SWITCH_BTN_A;
                        consumed = true;
                    } else if(event->type == InputTypeShort) {
                        // Check for double-click to toggle mode
                        uint32_t current_time = furi_get_tick();
                        uint32_t time_since_last = current_time - model->last_ok_press_time;

                        if(time_since_last < 500) { // 500ms window for double-click
                            // Double click detected - toggle mode
                            model->control_mode = (model->control_mode + 1) % ControlModeCount;
                            // Reset stick positions when switching modes
                            if(model->control_mode != ControlModeDPad) {
                                model->state.hat = SWITCH_HAT_NEUTRAL;
                            }
                            model->last_ok_press_time = 0; // Reset to prevent triple-click
                        } else {
                            model->last_ok_press_time = current_time;
                        }
                        consumed = true;
                    }
                } else if(event->key == InputKeyBack) {
                    if(event->type == InputTypePress) {
                        model->state.buttons |= SWITCH_BTN_B;
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        model->state.buttons &= ~SWITCH_BTN_B;
                        consumed = true; // Don't let it exit immediately
                    } else if(event->type == InputTypeShort) {
                        // Consume short press event to prevent navigation callback
                        consumed = true;
                    } else if(event->type == InputTypeLong) {
                        // Show exit confirmation dialog
                        model->exit_confirm = true;
                        // Release B button
                        model->state.buttons &= ~SWITCH_BTN_B;
                        consumed = true;
                    }
                }
            }
        },
        consumed);

    return consumed;
}

void switch_controller_scene_on_enter_controller(void* context) {
    SwitchControllerApp* app = context;

    // Create controller view if it doesn't exist
    if(!app->controller_view) {
        app->controller_view = view_alloc();
        view_set_context(app->controller_view, app);
        view_allocate_model(
            app->controller_view, ViewModelTypeLocking, sizeof(ControllerViewModel));
        view_set_draw_callback(
            app->controller_view, switch_controller_view_controller_draw_callback);
        view_set_input_callback(
            app->controller_view, switch_controller_view_controller_input_callback);
        view_dispatcher_add_view(
            app->view_dispatcher, SwitchControllerViewController, app->controller_view);
    }

    // Initialize model FIRST before USB/timer
    with_view_model(
        app->controller_view,
        ControllerViewModel * model,
        {
            usb_hid_switch_reset_state(&model->state);
            model->connected = false; // Start disconnected
            model->selected_button = 0;
            model->control_mode = ControlModeDPad;
            model->exit_confirm = false;
            model->last_ok_press_time = 0;
            model->button_to_release = 0;
            model->button_release_time = 0;
        },
        true);

    // Switch to view before initializing USB
    view_dispatcher_switch_to_view(app->view_dispatcher, SwitchControllerViewController);

    // Small delay to let view stabilize
    furi_delay_ms(100);

    // Initialize USB
    app->usb_mode_prev = furi_hal_usb_get_config();
    app->usb_connected = false;

    if(usb_hid_switch_init()) {
        furi_delay_ms(50); // Let USB stabilize
        app->usb_connected = true;

        // Update connection status
        with_view_model(
            app->controller_view, ControllerViewModel * model, { model->connected = true; }, true);
    }

    // Allocate timer if needed
    if(app->timer == NULL) {
        app->timer =
            furi_timer_alloc(switch_controller_timer_callback, FuriTimerTypePeriodic, app);
    }

    // Start timer LAST, after everything is initialized (100Hz = 10ms)
    furi_timer_start(app->timer, 10);
}

bool switch_controller_scene_on_event_controller(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void switch_controller_scene_on_exit_controller(void* context) {
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

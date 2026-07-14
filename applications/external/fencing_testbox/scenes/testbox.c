// Copyright (c) 2024 Aaron Janeiro Stone
// Licensed under the MIT license.
// See the LICENSE.txt file in the project root for details.

#include "../fencing_testbox.h"

void testbox_redraw(LightState light_state, FencingTestboxApp* app) {
    furi_assert(app);
    FURI_LOG_D(TAG, "Redrawing testbox");

    // Redraw the widget
    widget_reset(app->widget);
    widget_add_icon_element(
        app->widget, 25, 20, light_state.red_emitting ? &I_letter_r_lit : &I_letter_r_unlit);
    widget_add_icon_element(
        app->widget, 85, 20, light_state.green_emitting ? &I_letter_g_lit : &I_letter_g_unlit);
    widget_add_string_element(
        app->widget, 64, 60, AlignCenter, AlignBottom, FontPrimary, "Press back to exit");
}

void testbox_scene_on_tick(void* context) {
    FencingTestboxApp* app = (FencingTestboxApp*)context;
    furi_assert(app);
    if(!app->gpio_initialized) {
        return;
    }

    // Check the gpio
    bool red = furi_hal_gpio_read(&RED_PIN);
    bool green = furi_hal_gpio_read(&GREEN_PIN);
    if(red != app->light_state.red_emitting || green != app->light_state.green_emitting) {
        app->light_state.red_emitting = red;
        app->light_state.green_emitting = green;
        testbox_redraw(app->light_state, app);
    }
}

bool testbox_scene_on_event(void* context, SceneManagerEvent index) {
    FencingTestboxApp* app = (FencingTestboxApp*)context;
    furi_assert(app);

    if(index.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(app->scene_manager, FencingTestboxSceneMainMenu);
        return true;
    }

    return false;
}

void testbox_scene_on_exit(void* context) {
    FURI_LOG_D(TAG, "Testbox exit");
    FencingTestboxApp* app = (FencingTestboxApp*)context;
    furi_assert(app);

    // Deprioritize the gpio
    if(app->gpio_initialized) {
        FURI_LOG_D(TAG, "Deprioritizing gpio");
        furi_hal_gpio_init(&RED_PIN, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_init(&GREEN_PIN, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        app->gpio_initialized = false;
        app->light_state.green_emitting = false;
        app->light_state.red_emitting = false;
    }
}

void testbox_scene_on_enter(void* context) {
    FURI_LOG_D(TAG, "Testbox enter");
    FencingTestboxApp* app = (FencingTestboxApp*)context;
    furi_assert(app);

    // GPIO init
    if(app->gpio_initialized) {
        FURI_LOG_E(TAG, "Gpio state improperly initialized!");
        furi_crash();
    }

    FURI_LOG_D(TAG, "Initializing gpio");
    furi_hal_gpio_init(&RED_PIN, GpioModeInput, GpioPullDown, GpioSpeedMedium);
    furi_hal_gpio_init(&GREEN_PIN, GpioModeInput, GpioPullDown, GpioSpeedMedium);
    app->gpio_initialized = true;

    // Set up the view
    widget_reset(app->widget);
    testbox_redraw(app->light_state, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FencingTestboxSceneTestbox);
}

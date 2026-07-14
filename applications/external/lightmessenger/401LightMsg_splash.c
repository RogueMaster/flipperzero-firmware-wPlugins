/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2024 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "401LightMsg_splash.h"

//static const char* TAG = "401_LightMsgSplash";

/**
 * Handles input events for the splash scene. Sends custom events based on
 * the user's keypresses.
 *
 * @param input_event The input event structure.
 * @param ctx The application context.
 * @return true if the event was consumed, false otherwise.
 */
static bool app_splash_input_callback(InputEvent* input_event, void* ctx) {
    UNUSED(input_event);
    AppContext* app = (AppContext*)ctx;
    if(input_event->type == InputTypeShort) {
        if(input_event->key == InputKeyBack) {
            view_dispatcher_send_custom_event(app->view_dispatcher, AppSplashEventQuit);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, AppSplashEventRoll);
        }
    }
    return true;
}

/**
 * Renders the splash scene. Draws the splash icon and text on the canvas.
 *
 * @param canvas The canvas to draw on.
 * @param ctx The application context.
 */
// Invoked by the draw callback to render the AppSplash.
static void app_splash_render_callback(Canvas* canvas, void* _model) {
    AppStateCtx* model = (AppStateCtx*)_model;
    uint8_t i = 0;
    canvas_clear(canvas);
    switch(model->app_state) {
    case AppStateSplash:
        canvas_draw_icon(canvas, 0, 0, &I_401_lghtmsg_splash);
        break;
    case AppStateFlashlight:
        canvas_draw_icon(canvas, 0, 0, &I_401_lghtmsg_flashlight);
        switch(model->screen) {
        default:
        case 0:
            for(i = 0; i < LIGHTMSG_LED_ROWS; i++) {
                SK6805_set_led_color(i, 0xFF, 0xFF, 0xFF);
            }
            break;
        case 1:
            for(i = 0; i < LIGHTMSG_LED_ROWS; i++) {
                SK6805_set_led_color(i, 0xAA, 0xAA, 0xAA);
            }
            break;
        case 2:
            for(i = 0; i < LIGHTMSG_LED_ROWS; i++) {
                SK6805_set_led_color(i, 0x44, 0x44, 0x44);
            }
            break;
        }
        SK6805_update();
        break;
    case AppStateAbout:
    default:
        switch(model->screen) {
        default:
        case 0:
            canvas_draw_icon(canvas, ((64 - 48) / 2), (64 - 48), &I_lab401);
            canvas_set_font(canvas, FontPrimary);

            canvas_draw_str_aligned(canvas, 64 + 32, 10, AlignCenter, AlignTop, "Light");
            canvas_draw_str_aligned(canvas, 64 + 32, 20, AlignCenter, AlignTop, "Messenger");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "https://lab401.com");
            canvas_draw_str_aligned(canvas, 64 + 32, 35, AlignCenter, AlignTop, "Brought to you");
            canvas_draw_str_aligned(canvas, 64 + 32, 45, AlignCenter, AlignTop, "by Lab401");
            //canvas_draw_str_aligned(canvas, 64 + 32, 45, AlignCenter, AlignTop, "with love <3");
            break;
        case 1:
            canvas_draw_icon(canvas, ((64 - 52) / 2), (64 - 52), &I_cyberpunk_company);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64 + 32, 10, AlignCenter, AlignTop, "CYBERPUNK");
            canvas_draw_str_aligned(canvas, 64 + 32, 20, AlignCenter, AlignTop, "COMPANY");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, 64, 0, AlignCenter, AlignTop, "https://cyberpunk.company");
            canvas_draw_str_aligned(canvas, 64 + 32, 35, AlignCenter, AlignTop, "Engineering");
            canvas_draw_str_aligned(canvas, 64 + 32, 45, AlignCenter, AlignTop, "From Tixlegeek");

            break;
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 120, 55, AlignCenter, AlignTop, "next>");

        break;
    }
}

/**
 * Allocates and initializes a new AppSplash instance.
 *
 * @param ctx The application context.
 * @return Pointer to the newly allocated AppSplash instance.
 */
AppSplash* app_splash_alloc(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppSplash* appSplash = malloc(sizeof(AppSplash));
    appSplash->view = view_alloc();
    view_allocate_model(appSplash->view, ViewModelTypeLockFree, sizeof(AppStateCtx));
    view_set_context(appSplash->view, app);
    view_set_draw_callback(appSplash->view, app_splash_render_callback);
    view_set_input_callback(appSplash->view, app_splash_input_callback);
    return appSplash;
}
/**
 * Retrieves the view associated with the AppSplash instance.
 *
 * @param appSplash Pointer to the AppSplash instance.
 * @return Pointer to the associated view.
 */
View* app_splash_get_view(AppSplash* appSplash) {
    furi_assert(appSplash);
    return appSplash->view;
}
/**
 * Handles the entry into the splash scene. Switches the view to the splash
 * scene.
 *
 * @param context The application context.
 */
void app_scene_splash_on_enter(void* context) {
    AppContext* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSplash);
}
/**
 * Handles events for the splash scene. Processes custom events to navigate to
 * other scenes based on the user's actions.
 *
 * @param context The application context.
 * @param event The scene manager event.
 * @return true if the event was consumed, false otherwise.
 */

bool app_scene_splash_on_event(void* context, SceneManagerEvent event) {
    AppContext* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == AppSplashEventRoll) {
            AppStateCtx* model =
                (AppStateCtx*)view_get_model(app_splash_get_view(app->sceneSplash));
            if(model->app_state == AppStateAbout) {
                model->screen++;
                if(model->screen >= SPLASH_MAX_ABOUT_SCREENS) model->screen = 0;
                view_commit_model(app_splash_get_view(app->sceneSplash), true);
                consumed = true;
            } else if(model->app_state == AppStateFlashlight) {
                model->screen++;
                if(model->screen >= SPLASH_MAX_FLASHLIGHT_SCREENS) model->screen = 0;
                view_commit_model(app_splash_get_view(app->sceneSplash), true);
                consumed = true;
            } else {
                UNUSED(model);
                view_dispatcher_send_custom_event(app->view_dispatcher, AppSplashEventQuit);
            }
        } else if(event.event == AppSplashEventQuit) {
            scene_manager_next_scene(app->scene_manager, AppSceneMainMenu);
            consumed = true;
        }
    }
    return consumed;
}
/**
 * Handles the exit from the splash scene. No specific actions are
 * currently performed during exit.
 *
 * @param context The application context.
 */
void app_scene_splash_on_exit(void* context) {
    UNUSED(context);
    SK6805_off();
}

/**
 * Frees the resources associated with an AppSplash instance.
 *
 * @param appSplash Pointer to the AppSplash instance to be freed.
 */
void app_splash_free(AppSplash* appSplash) {
    furi_assert(appSplash);
    view_free(appSplash->view);
    free(appSplash);
}

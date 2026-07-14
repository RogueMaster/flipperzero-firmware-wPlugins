#include "../gemini_app_i.h"

#define REFRESH_RATE 250

static FuriString* contents = NULL;
static FuriString* line = NULL;

static void gemini_scene_receive_serial_timer_callback(void* context) {
    GeminiApp* app = context;
    furi_string_reset(line);
    bool read = uart_helper_read(app->uart_helper, line, 0);
    if(read) {
        gemini_text_box_set_text(app->gemini_text_box, "");
        size_t new_len = furi_string_size(contents) + furi_string_size(line);
        if(new_len >= 4096) {
            furi_string_right(contents, new_len / 2);
        }
        furi_string_cat(contents, line);
        gemini_text_box_set_text(app->gemini_text_box, furi_string_get_cstr(contents));
    }
}

static bool gemini_text_box_nav_callback(void* context) {
    GeminiApp* app = context;
    GeminiScene back_scene =
        (GeminiScene)scene_manager_get_scene_state(app->scene_manager, GeminiSceneReceiveSerial);
    if(back_scene == GeminiSceneMainMenu) {
        // False should be returned to allow the back event to be handled by the scene manager.
        return false;
    } else {
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, back_scene);
    }
    return true;
}

void gemini_scene_receive_serial_set_next(void* context, GeminiScene back_scene) {
    GeminiApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager, GeminiSceneReceiveSerial, (uint32_t)back_scene);
}

void gemini_scene_receive_serial_on_enter(void* context) {
    GeminiApp* app = context;
    contents = furi_string_alloc();
    line = furi_string_alloc();
    gemini_text_box_set_text(app->gemini_text_box, "");
    gemini_text_box_set_focus_end(app->gemini_text_box);
    gemini_text_box_set_nav_callback(app->gemini_text_box, gemini_text_box_nav_callback, app);
    app->timer =
        furi_timer_alloc(gemini_scene_receive_serial_timer_callback, FuriTimerTypePeriodic, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GeminiViewGeminiTextBox);
    furi_timer_start(app->timer, REFRESH_RATE);
}

bool gemini_scene_receive_serial_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false; // event not handled.
}

void gemini_scene_receive_serial_on_exit(void* context) {
    GeminiApp* app = context;
    furi_timer_stop(app->timer);
    furi_string_free(line);
    furi_string_free(contents);
}

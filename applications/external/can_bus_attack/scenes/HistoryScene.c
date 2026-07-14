#include "../app_user.h"
#include <string.h>

static bool is_frame_in_history(CanIdHistory* hist, uint8_t* buf, uint8_t len) {
    for(uint8_t i = 0; i < hist->count; i++) {
        if(hist->dlc[i] != len) continue;
        if(memcmp(hist->history[i], buf, len) == 0) return true;
    }
    return false;
}

static void add_to_history(CanIdHistory* hist, uint8_t* buf, uint8_t len) {
    if(hist->count < MAX_HISTORY && !is_frame_in_history(hist, buf, len)) {
        memcpy(hist->history[hist->count], buf, len);
        hist->dlc[hist->count] = len;
        hist->count++;
    }
}

void save_frame_history(App* app, uint32_t canid, uint8_t* buf, uint8_t len) {
    for(uint8_t i = 0; i < app->history_count; i++) {
        if(app->history_list[i].canid == canid) {
            add_to_history(&app->history_list[i], buf, len);
            return;
        }
    }

    if(app->history_count < 100) {
        app->history_list[app->history_count].canid = canid;
        app->history_list[app->history_count].count = 0;
        add_to_history(&app->history_list[app->history_count], buf, len);
        app->history_count++;
    }
}

// ---------- SCENE: Histórico ----------
void app_scene_history_on_enter(void* context) {
    App* app = context;
    uint32_t canid = scene_manager_get_scene_state(app->scene_manager, app_scene_history_view);
    CanIdHistory* hist = NULL;

    for(uint8_t i = 0; i < app->history_count; i++) {
        if(app->history_list[i].canid == canid) {
            hist = &app->history_list[i];
            break;
        }
    }

    furi_string_reset(app->text);
    if(hist && hist->count > 0) {
        furi_string_cat_printf(app->text, "Historical 0x%lx:\n", canid);
        for(uint8_t j = 0; j < hist->count; j++) {
            furi_string_cat_printf(app->text, "[%u]: ", j + 1);
            for(uint8_t b = 0; b < hist->dlc[j]; b++) {
                furi_string_cat_printf(app->text, "%02X ", hist->history[j][b]);
            }
            furi_string_cat_str(app->text, "\n");
        }
    } else {
        furi_string_cat_str(app->text, "No history for this ID.");
    }

    text_box_set_font(app->textBox, TextBoxFontText);
    text_box_reset(app->textBox);
    text_box_set_text(app->textBox, furi_string_get_cstr(app->text));
    text_box_set_focus(app->textBox, TextBoxFocusStart);
    view_dispatcher_switch_to_view(app->view_dispatcher, TextBoxView);
}

bool app_scene_history_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void app_scene_history_on_exit(void* context) {
    App* app = context;
    furi_string_reset(app->text);
    text_box_reset(app->textBox);
}

/*
 * Recent pushes list.
 * Only shows pushes compatible with the current tag profile.
 */

#include "../tagtinker_app.h"
#define EVT_ADD_NEW 200
#define EVT_RECENT  0

static void recent_list_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static char recent_labels[TAGTINKER_MAX_PRESETS][48];
static uint8_t filtered_indices[TAGTINKER_MAX_PRESETS];
static uint8_t filtered_count = 0;

void tagtinker_scene_preset_list_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;

    tagtinker_recents_load(app);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Recent Pushes");

    submenu_add_item(app->submenu, "[+] New Text", EVT_ADD_NEW, recent_list_cb, app);

    filtered_count = 0;
    for(uint8_t i = 0; i < app->recent_count; i++) {
        /* Filter by current tag's width/height if available */
        if(app->selected_target >= 0) {
            TagTinkerTarget* target = &app->targets[app->selected_target];
            if(target->profile.width > 0 && target->profile.height > 0) {
                if(app->recents[i].width != target->profile.width ||
                   app->recents[i].height != target->profile.height) {
                    continue; /* Incompatible size, skip */
                }
            }
        }

        filtered_indices[filtered_count] = i;
        snprintf(
            recent_labels[filtered_count],
            sizeof(recent_labels[filtered_count]),
            "\"%s\"",
            app->recents[i].text);

        submenu_add_item(
            app->submenu,
            recent_labels[filtered_count],
            EVT_RECENT + filtered_count,
            recent_list_cb,
            app);

        filtered_count++;
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewSubmenu);
}

bool tagtinker_scene_preset_list_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == EVT_ADD_NEW) {
        memset(app->text_input_buf, 0, sizeof(app->text_input_buf));
        scene_manager_set_scene_state(app->scene_manager, TagTinkerSceneTextInput, 0);
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneTextInput);
        return true;
    }

    uint32_t f_idx = event.event - EVT_RECENT;
    if(f_idx < filtered_count) {
        uint8_t r_idx = filtered_indices[f_idx];

        app->esl_width = app->recents[r_idx].width;
        app->esl_height = app->recents[r_idx].height;
        app->img_page = app->recents[r_idx].page;
        app->invert_text = app->recents[r_idx].invert;
        app->color_clear = app->recents[r_idx].color_clear;
        app->text_padding_pct = app->recents[r_idx].padding;
        app->signal_mode = TagTinkerSignalPP4;
        strncpy(app->text_input_buf, app->recents[r_idx].text, sizeof(app->text_input_buf) - 1);

        TagTinkerTarget* target = &app->targets[app->selected_target];

        /* Auto-save/update recents order */
        tagtinker_recents_add(app, app->text_input_buf);

        FURI_LOG_I(TAGTINKER_TAG, "TX Recent: reps=%u", app->data_frame_repeats);

        tagtinker_prepare_text_tx(app, target->plid);
        app->tx_spam = false;
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneTransmit);
        return true;
    }

    return false;
}

void tagtinker_scene_preset_list_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    submenu_reset(app->submenu);
}

/*
 * Text preset list.
 */

#include "../tagtinker_app.h"
#define EVT_ADD_NEW  200
#define EVT_PRESET   0

static void presets_load(TagTinkerApp* app) {
    app->preset_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, APP_DATA_PATH("presets.txt"),
           FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[512];
        uint16_t read = storage_file_read(file, buf, sizeof(buf) - 1);
        buf[read] = '\0';
        storage_file_close(file);

        char* line = buf;
        while(line && *line && app->preset_count < TAGTINKER_MAX_PRESETS) {
            char* nl = strchr(line, '\n');
            if(nl) *nl = '\0';

            unsigned w, h, pg, inv, clr;
            if(sscanf(line, "%u|%u|%u|%u|%u|", &w, &h, &pg, &inv, &clr) == 5) {
                char* p = line;
                int pipes = 0;
                while(*p && pipes < 5) { if(*p == '|') pipes++; p++; }

                uint8_t idx = app->preset_count++;
                app->presets[idx].width = (uint16_t)w;
                app->presets[idx].height = (uint16_t)h;
                app->presets[idx].page = (uint8_t)pg;
                app->presets[idx].invert = (inv != 0);
                app->presets[idx].color_clear = (clr != 0);
                memset(app->presets[idx].text, 0, TAGTINKER_PRESET_TEXT_LEN);
                if(pipes == 5) {
                    strncpy(app->presets[idx].text, p, TAGTINKER_PRESET_TEXT_LEN - 1);
                }
            }
            line = nl ? nl + 1 : NULL;
        }
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void preset_list_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static char preset_labels[TAGTINKER_MAX_PRESETS][48];

void tagtinker_scene_preset_list_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;

    presets_load(app);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Text Presets");

    submenu_add_item(app->submenu, "[+] New Preset",
        EVT_ADD_NEW, preset_list_cb, app);

    for(uint8_t i = 0; i < app->preset_count; i++) {
        snprintf(preset_labels[i], sizeof(preset_labels[i]),
            "%ux%u \"%s\"",
            app->presets[i].width,
            app->presets[i].height,
            app->presets[i].text);
        submenu_add_item(app->submenu, preset_labels[i],
            EVT_PRESET + i, preset_list_cb, app);
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

    uint32_t idx = event.event - EVT_PRESET;
    if(idx < app->preset_count) {
        app->esl_width = app->presets[idx].width;
        app->esl_height = app->presets[idx].height;
        app->img_page = app->presets[idx].page;
        app->invert_text = app->presets[idx].invert;
        app->color_clear = app->presets[idx].color_clear;
        strncpy(app->text_input_buf, app->presets[idx].text,
            sizeof(app->text_input_buf) - 1);

        FURI_LOG_I(TAGTINKER_TAG, "Preset %lu: %ux%u \"%s\"",
            idx, app->esl_width, app->esl_height, app->text_input_buf);

        TagTinkerTarget* target = &app->targets[app->selected_target];
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

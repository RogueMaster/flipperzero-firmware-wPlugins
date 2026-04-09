/*
 * Size picker scene.
 */

#include "../tagtinker_app.h"
#include "../views/tagtinker_font.h"
#include "../protocol/tagtinker_proto.h"
#include <storage/storage.h>

static const uint16_t width_values[] = {
    48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128, 136, 144, 152, 160, 168, 172,
    176, 184, 192, 200, 208, 224, 240, 256, 264, 272, 288, 296, 304, 320, 400, 648, 800,
};

static const uint16_t height_values[] = {
    50, 55, 60, 65, 70, 72, 75, 80, 85, 90, 96, 104, 112, 120, 128, 140, 152, 176, 192, 300, 480,
};

static const uint16_t coord_values[] = {
    0,   8,   16,  24,  32,  40,  48,  56,  64,  72,  80,  88,  96,  104, 112, 120, 128,
    136, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 240, 248, 256, 264,
    272, 280, 288, 296, 304, 312, 320, 328, 336, 344, 352, 360, 368, 376, 384, 392, 400,
    408, 416, 424, 432, 440, 448, 456, 464, 472, 480, 488, 496, 504, 512, 520, 528, 536,
    544, 552, 560, 568, 576, 584, 592, 600, 608, 616, 624, 632, 640, 648, 656, 664, 672,
    680, 688, 696, 704, 712, 720, 728, 736, 744, 752, 760, 768, 776, 784, 792, 800,
};

static const char* compression_labels[] = {"Auto", "Raw", "RLE"};

#define W_COUNT COUNT_OF(width_values)
#define H_COUNT COUNT_OF(height_values)
#define COORD_COUNT COUNT_OF(coord_values)

enum {
    SettingWidth,
    SettingHeight,
    SettingPage,
    SettingPolarity,
    SettingMode,
    SettingCompression,
    SettingFrameRepeat,
    SettingOffsetX,
    SettingOffsetY,
    SettingSave,
    SettingTransmit,
};

static void clamp_current_offsets(TagTinkerApp* app);

static void width_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= W_COUNT) idx = W_COUNT - 1;
    app->esl_width = width_values[idx];

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", app->esl_width);
    variable_item_set_current_value_text(item, buf);
    clamp_current_offsets(app);
}

static void height_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= H_COUNT) idx = H_COUNT - 1;
    app->esl_height = height_values[idx];

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", app->esl_height);
    variable_item_set_current_value_text(item, buf);
    clamp_current_offsets(app);
}

static uint8_t nearest_value_index(const uint16_t* values, uint8_t count, uint16_t target) {
    uint8_t best_idx = 0;
    uint16_t best_diff = UINT16_MAX;

    for(uint8_t i = 0; i < count; i++) {
        uint16_t value = values[i];
        uint16_t diff = (value > target) ? (value - target) : (target - value);
        if(diff < best_diff) {
            best_diff = diff;
            best_idx = i;
        }
    }

    return best_idx;
}

static void clamp_current_offsets(TagTinkerApp* app) {
    if(!app) return;
    if(app->selected_target < 0 || app->selected_target >= app->target_count) return;

    const TagTinkerTarget* target = &app->targets[app->selected_target];
    if(!target->profile.known || !target->profile.width || !target->profile.height) return;

    uint16_t max_x = (app->esl_width < target->profile.width) ?
                         (uint16_t)(target->profile.width - app->esl_width) :
                         0U;
    uint16_t max_y = (app->esl_height < target->profile.height) ?
                         (uint16_t)(target->profile.height - app->esl_height) :
                         0U;

    if(app->draw_x > max_x) app->draw_x = max_x;
    if(app->draw_y > max_y) app->draw_y = max_y;
}

static void page_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    app->img_page = variable_item_get_current_value_index(item);

    char buf[4];
    snprintf(buf, sizeof(buf), "%u", app->img_page);
    variable_item_set_current_value_text(item, buf);
}

static void polarity_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    app->invert_text = (variable_item_get_current_value_index(item) == 1);

    variable_item_set_current_value_text(
        item, app->invert_text ? "W on B" : "B on W");
}

static void mode_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    TagTinkerTarget* target = (app->selected_target >= 0) ? &app->targets[app->selected_target] : NULL;
    bool accent = tagtinker_target_supports_accent(target);
    app->color_clear = (variable_item_get_current_value_index(item) == 1);

    variable_item_set_current_value_text(
        item,
        accent ? (app->color_clear ? "Accent" : "Black")
               : (app->color_clear ? "Dual Plane" : "Mono Fast"));
}

static void compression_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index > TagTinkerCompressionRle) index = TagTinkerCompressionRle;
    app->compression_mode = (TagTinkerCompressionMode)index;
    variable_item_set_current_value_text(item, compression_labels[index]);
}

static void frame_repeat_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    app->data_frame_repeats = variable_item_get_current_value_index(item) + 1U;

    char buf[4];
    snprintf(buf, sizeof(buf), "%u", app->data_frame_repeats);
    variable_item_set_current_value_text(item, buf);
}

static void offset_x_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COORD_COUNT) index = COORD_COUNT - 1U;
    app->draw_x = coord_values[index];
    clamp_current_offsets(app);

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", app->draw_x);
    variable_item_set_current_value_text(item, buf);
}

static void offset_y_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COORD_COUNT) index = COORD_COUNT - 1U;
    app->draw_y = coord_values[index];
    clamp_current_offsets(app);

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", app->draw_y);
    variable_item_set_current_value_text(item, buf);
}

static void save_presets_to_sd(TagTinkerApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, APP_DATA_PATH("presets.txt"),
           FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        for(uint8_t i = 0; i < app->preset_count; i++) {
            char line[80];
            int len = snprintf(line, sizeof(line), "%u|%u|%u|%d|%d|%s\n",
                app->presets[i].width, app->presets[i].height,
                app->presets[i].page,
                app->presets[i].invert ? 1 : 0,
                app->presets[i].color_clear ? 1 : 0,
                app->presets[i].text);
            storage_file_write(file, line, (uint16_t)len);
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void setting_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;

    if(index == SettingSave) {
        /* Save current settings as a preset */
        if(app->preset_count < TAGTINKER_MAX_PRESETS) {
            uint8_t idx = app->preset_count++;
            app->presets[idx].width = app->esl_width;
            app->presets[idx].height = app->esl_height;
            app->presets[idx].page = app->img_page;
            app->presets[idx].invert = app->invert_text;
            app->presets[idx].color_clear = app->color_clear;
            strncpy(app->presets[idx].text, app->text_input_buf,
                TAGTINKER_PRESET_TEXT_LEN - 1);
            app->presets[idx].text[TAGTINKER_PRESET_TEXT_LEN - 1] = '\0';

            save_presets_to_sd(app);
            notification_message(app->notifications, &sequence_success);
            FURI_LOG_I(TAGTINKER_TAG, "Preset saved: %ux%u", app->esl_width, app->esl_height);
        }
        return;
    }

    if(index != SettingTransmit) return;

    FURI_LOG_I(TAGTINKER_TAG, "TX: %ux%u pg=%u inv=%d clr=%d",
        app->esl_width, app->esl_height, app->img_page,
        app->invert_text, app->color_clear);

    TagTinkerTarget* target = &app->targets[app->selected_target];
    tagtinker_prepare_text_tx(app, target->plid);
    app->tx_spam = false;
    scene_manager_next_scene(app->scene_manager, TagTinkerSceneTransmit);
}

void tagtinker_scene_size_picker_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;

    variable_item_list_reset(app->var_item_list);

    uint8_t w_idx = nearest_value_index(width_values, W_COUNT, app->esl_width);
    uint8_t h_idx = nearest_value_index(height_values, H_COUNT, app->esl_height);
    app->esl_width = width_values[w_idx];
    app->esl_height = height_values[h_idx];
    clamp_current_offsets(app);

    /* Width */
    VariableItem* item_w = variable_item_list_add(
        app->var_item_list, "Width", W_COUNT, width_changed, app);
    variable_item_set_current_value_index(item_w, w_idx);
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", app->esl_width);
        variable_item_set_current_value_text(item_w, buf);
    }

    /* Height */
    VariableItem* item_h = variable_item_list_add(
        app->var_item_list, "Height", H_COUNT, height_changed, app);
    variable_item_set_current_value_index(item_h, h_idx);
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", app->esl_height);
        variable_item_set_current_value_text(item_h, buf);
    }

    /* Page */
    VariableItem* item_pg = variable_item_list_add(
        app->var_item_list, "Page", 8, page_changed, app);
    variable_item_set_current_value_index(item_pg, app->img_page);
    {
        char buf[4];
        snprintf(buf, sizeof(buf), "%u", app->img_page);
        variable_item_set_current_value_text(item_pg, buf);
    }

    /* Polarity */
    VariableItem* item_col = variable_item_list_add(
        app->var_item_list, "Polarity", 2, polarity_changed, app);
    variable_item_set_current_value_index(item_col, app->invert_text ? 1 : 0);
    variable_item_set_current_value_text(
        item_col, app->invert_text ? "W on B" : "B on W");

    /* Text ink / plane mode */
    TagTinkerTarget* target = (app->selected_target >= 0) ? &app->targets[app->selected_target] : NULL;
    bool accent = tagtinker_target_supports_accent(target);
    VariableItem* item_mode = variable_item_list_add(
        app->var_item_list, accent ? "Ink" : "Mode", 2, mode_changed, app);
    variable_item_set_current_value_index(item_mode, app->color_clear ? 1 : 0);
    variable_item_set_current_value_text(
        item_mode,
        accent ? (app->color_clear ? "Accent" : "Black")
               : (app->color_clear ? "Dual Plane" : "Mono Fast"));

    VariableItem* item_comp = variable_item_list_add(
        app->var_item_list, "Compression", 3, compression_changed, app);
    variable_item_set_current_value_index(item_comp, app->compression_mode);
    variable_item_set_current_value_text(item_comp, compression_labels[app->compression_mode]);

    VariableItem* item_rep = variable_item_list_add(
        app->var_item_list, "Frame Repeat", 5, frame_repeat_changed, app);
    variable_item_set_current_value_index(item_rep, app->data_frame_repeats - 1U);
    {
        char buf[4];
        snprintf(buf, sizeof(buf), "%u", app->data_frame_repeats);
        variable_item_set_current_value_text(item_rep, buf);
    }

    VariableItem* item_x = variable_item_list_add(
        app->var_item_list, "Offset X", COORD_COUNT, offset_x_changed, app);
    variable_item_set_current_value_index(
        item_x, nearest_value_index(coord_values, COORD_COUNT, app->draw_x));
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", app->draw_x);
        variable_item_set_current_value_text(item_x, buf);
    }

    VariableItem* item_y = variable_item_list_add(
        app->var_item_list, "Offset Y", COORD_COUNT, offset_y_changed, app);
    variable_item_set_current_value_index(
        item_y, nearest_value_index(coord_values, COORD_COUNT, app->draw_y));
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", app->draw_y);
        variable_item_set_current_value_text(item_y, buf);
    }

    /* Save preset button */
    variable_item_list_add(app->var_item_list, "[*] Save Preset", 0, NULL, app);

    /* Transmit button */
    variable_item_list_add(app->var_item_list, ">> Send to Tag <<", 0, NULL, app);

    variable_item_list_set_enter_callback(app->var_item_list, setting_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewVarItemList);
}

bool tagtinker_scene_size_picker_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void tagtinker_scene_size_picker_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    variable_item_list_reset(app->var_item_list);
}

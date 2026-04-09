/*
 * Settings scene
 */

#include "../tagtinker_app.h"

enum {
    SettingsItemStartupWarning,
    SettingsItemSignalMode,
    SettingsItemCompression,
    SettingsItemFrameRepeat,
    SettingsItemOffsetX,
    SettingsItemOffsetY,
};

static const char* settings_toggle_labels[] = {"Off", "On"};
static const char* settings_signal_labels[] = {"PP4", "PP16"};
static const char* settings_compression_labels[] = {"Auto", "Raw", "RLE"};
static const uint16_t settings_coord_values[] = {
    0,   8,   16,  24,  32,  40,  48,  56,  64,  72,  80,  88,  96,  104, 112, 120, 128,
    136, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 240, 248, 256, 264,
    272, 280, 288, 296, 304, 312, 320, 328, 336, 344, 352, 360, 368, 376, 384, 392, 400,
    408, 416, 424, 432, 440, 448, 456, 464, 472, 480, 488, 496, 504, 512, 520, 528, 536,
    544, 552, 560, 568, 576, 584, 592, 600, 608, 616, 624, 632, 640, 648, 656, 664, 672,
    680, 688, 696, 704, 712, 720, 728, 736, 744, 752, 760, 768, 776, 784, 792, 800,
};

#define SETTINGS_COORD_COUNT COUNT_OF(settings_coord_values)

static uint8_t nearest_coord_index(uint16_t value) {
    uint8_t best_idx = 0;
    uint16_t best_diff = UINT16_MAX;

    for(uint8_t i = 0; i < SETTINGS_COORD_COUNT; i++) {
        uint16_t current = settings_coord_values[i];
        uint16_t diff = (current > value) ? (current - value) : (value - current);
        if(diff < best_diff) {
            best_diff = diff;
            best_idx = i;
        }
    }

    return best_idx;
}

static void startup_warning_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->show_startup_warning = (index == 1);
    variable_item_set_current_value_text(item, settings_toggle_labels[index]);

    if(!tagtinker_settings_save(app)) {
        FURI_LOG_W(TAGTINKER_TAG, "Failed to save settings");
    }
}

static void signal_mode_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    if(index > TagTinkerSignalPP16) index = TagTinkerSignalPP16;
    app->signal_mode = (TagTinkerSignalMode)index;
    variable_item_set_current_value_text(item, settings_signal_labels[index]);

    if(!tagtinker_settings_save(app)) {
        FURI_LOG_W(TAGTINKER_TAG, "Failed to save settings");
    }
}

static void compression_mode_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    if(index > TagTinkerCompressionRle) index = TagTinkerCompressionRle;
    app->compression_mode = (TagTinkerCompressionMode)index;
    variable_item_set_current_value_text(item, settings_compression_labels[index]);

    if(!tagtinker_settings_save(app)) {
        FURI_LOG_W(TAGTINKER_TAG, "Failed to save settings");
    }
}

static void frame_repeat_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->data_frame_repeats = index + 1U;

    char buf[4];
    snprintf(buf, sizeof(buf), "%u", app->data_frame_repeats);
    variable_item_set_current_value_text(item, buf);

    if(!tagtinker_settings_save(app)) {
        FURI_LOG_W(TAGTINKER_TAG, "Failed to save settings");
    }
}

static void offset_x_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= SETTINGS_COORD_COUNT) index = SETTINGS_COORD_COUNT - 1U;
    app->draw_x = settings_coord_values[index];

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", app->draw_x);
    variable_item_set_current_value_text(item, buf);

    if(!tagtinker_settings_save(app)) {
        FURI_LOG_W(TAGTINKER_TAG, "Failed to save settings");
    }
}

static void offset_y_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= SETTINGS_COORD_COUNT) index = SETTINGS_COORD_COUNT - 1U;
    app->draw_y = settings_coord_values[index];

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", app->draw_y);
    variable_item_set_current_value_text(item, buf);

    if(!tagtinker_settings_save(app)) {
        FURI_LOG_W(TAGTINKER_TAG, "Failed to save settings");
    }
}

void tagtinker_scene_settings_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;
    VariableItemList* list = app->var_item_list;

    variable_item_list_reset(list);

    VariableItem* item = variable_item_list_add(
        list, "Startup Warning", 2, startup_warning_changed, app);
    variable_item_set_current_value_index(item, app->show_startup_warning ? 1 : 0);
    variable_item_set_current_value_text(
        item, settings_toggle_labels[app->show_startup_warning ? 1 : 0]);

    item = variable_item_list_add(list, "Signal Mode", 2, signal_mode_changed, app);
    variable_item_set_current_value_index(item, app->signal_mode);
    variable_item_set_current_value_text(item, settings_signal_labels[app->signal_mode]);

    item = variable_item_list_add(list, "Compression", 3, compression_mode_changed, app);
    variable_item_set_current_value_index(item, app->compression_mode);
    variable_item_set_current_value_text(item, settings_compression_labels[app->compression_mode]);

    item = variable_item_list_add(list, "Frame Repeat", 5, frame_repeat_changed, app);
    variable_item_set_current_value_index(item, app->data_frame_repeats - 1U);
    {
        char buf[4];
        snprintf(buf, sizeof(buf), "%u", app->data_frame_repeats);
        variable_item_set_current_value_text(item, buf);
    }

    item = variable_item_list_add(list, "Offset X", SETTINGS_COORD_COUNT, offset_x_changed, app);
    variable_item_set_current_value_index(item, nearest_coord_index(app->draw_x));
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", app->draw_x);
        variable_item_set_current_value_text(item, buf);
    }

    item = variable_item_list_add(list, "Offset Y", SETTINGS_COORD_COUNT, offset_y_changed, app);
    variable_item_set_current_value_index(item, nearest_coord_index(app->draw_y));
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", app->draw_y);
        variable_item_set_current_value_text(item, buf);
    }

    variable_item_list_set_selected_item(list, SettingsItemStartupWarning);
    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewVarItemList);
}

bool tagtinker_scene_settings_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void tagtinker_scene_settings_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    variable_item_list_reset(app->var_item_list);
}

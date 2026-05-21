#include <gui/modules/dialog_ex.h>
#include <gui/modules/variable_item_list.h>
#include <gui/view.h>
#include <storage/storage.h>
#include <flipper_format.h>

#include "settings.h"

#include <furi.h>

#define SETTINGS_FILE_PATH APP_DATA_PATH("settings.txt")
#define SETTINGS_FILE_VERSION 1

void stratahero_load_settings(StrataHeroSettings* settings) {
    settings->sound_enabled = true;
    settings->vibro_enabled = true;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    if (flipper_format_file_open_existing(file, SETTINGS_FILE_PATH)) {
        flipper_format_read_bool(file, "sound_enabled", &settings->sound_enabled, 1);
        flipper_format_read_bool(file, "vibro_enabled", &settings->vibro_enabled, 1);
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

void stratahero_save_settings(StrataHeroSettings* settings) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    storage_common_mkdir(storage, APP_DATA_PATH(""));

    if (flipper_format_file_open_always(file, SETTINGS_FILE_PATH)) {
        flipper_format_write_header_cstr(file, "StrataHero Settings File", SETTINGS_FILE_VERSION);

        flipper_format_write_bool(file, "sound_enabled", &settings->sound_enabled, 1);
        flipper_format_write_bool(file, "vibro_enabled", &settings->vibro_enabled, 1);
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

struct StrataHeroSettingsWidget {
    StrataHeroSettings settings;
    StrataHeroSettings original_settings;

    VariableItemList* menu;
    VariableItem* sound_enabled_item;
    VariableItem* vibro_enabled_item;

    DialogEx* save_confirmation_dialog;
};

static void bool_change_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    bool* value = variable_item_get_context(item);
    *value = index == 1;

    variable_item_set_current_value_text(item, *value ? "ON" : "OFF");
}

static void save_confirmation_dialog_callback(DialogExResult result, void* context) {
    furi_assert(context);

    StrataHeroSettingsWidget* widget = context;
    switch (result) {
        case DialogExResultLeft:
            stratahero_save_settings(&widget->settings);
            // stratahero_switch_view(app, StrataHero_View_MainMenu);
            break;
        case DialogExResultCenter:
            // stratahero_switch_view(app, StrataHero_View_Settings);
            break;
        case DialogExResultRight:
            // stratahero_switch_view(app, StrataHero_View_MainMenu);
            break;

        default:
            break;
    }
}

static void settings_enter_callback(void* context) {
    StrataHeroSettingsWidget* widget = context;
    widget->settings = widget->original_settings;

    variable_item_set_current_value_index(widget->sound_enabled_item, widget->settings.sound_enabled ? 1 : 0);
    variable_item_set_current_value_text(widget->sound_enabled_item, widget->settings.sound_enabled ? "ON" : "OFF");

    variable_item_set_current_value_index(widget->vibro_enabled_item, widget->settings.vibro_enabled ? 1 : 0);
    variable_item_set_current_value_text(widget->vibro_enabled_item, widget->settings.vibro_enabled ? "ON" : "OFF");
}

StrataHeroSettingsWidget* stratahero_settings_widget_alloc() {
    StrataHeroSettingsWidget* widget = malloc(sizeof(StrataHeroSettingsWidget));
    widget->menu = variable_item_list_alloc();
    widget->sound_enabled_item = variable_item_list_add(widget->menu, "Sound", 2, bool_change_callback, &widget->settings.sound_enabled);
    widget->vibro_enabled_item = variable_item_list_add(widget->menu, "Vibro", 2, bool_change_callback, &widget->settings.vibro_enabled);

    // Settings save confirmation
    widget->save_confirmation_dialog = dialog_ex_alloc();
    dialog_ex_set_header(widget->save_confirmation_dialog, "Save settings?", 64, 10, AlignCenter, AlignTop);
    dialog_ex_set_left_button_text(widget->save_confirmation_dialog, "Save");
    dialog_ex_set_center_button_text(widget->save_confirmation_dialog, "Cancel");
    dialog_ex_set_right_button_text(widget->save_confirmation_dialog, "Discard");

    dialog_ex_set_context(widget->save_confirmation_dialog, widget);
    dialog_ex_set_result_callback(widget->save_confirmation_dialog, save_confirmation_dialog_callback);

    View* view = variable_item_list_get_view(widget->menu);
    view_set_enter_callback(view, settings_enter_callback);

    return widget;
}

void stratahero_settings_widget_free(StrataHeroSettingsWidget* widget) {
    variable_item_list_free(widget->menu);
    dialog_ex_free(widget->save_confirmation_dialog);
    free(widget);
}

View* stratahero_settings_widget_get_view(StrataHeroSettingsWidget* widget) {
    return variable_item_list_get_view(widget->menu);
}

StrataHeroSettings* stratahero_settings_widget_get_settings(StrataHeroSettingsWidget* widget) {
    return &widget->settings;
}

void stratahero_settings_widget_set_settings(StrataHeroSettingsWidget* widget, StrataHeroSettings* settings) {
    widget->original_settings = *settings;
    widget->settings = *settings;
}

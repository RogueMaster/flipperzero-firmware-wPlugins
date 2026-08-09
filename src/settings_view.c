#include "settings_view.h"

#include <furi.h>
#include <gui/modules/variable_item_list.h>
#include <stdio.h>
#include <string.h>

struct SettingsView {
    VariableItemList* list;
    VariableItem* offset_item;
    App* app;
};

static void settings_view_offset_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    furi_check(app);

    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.utc_offset_minutes = settings_index_to_offset(index);
    app->settings.loaded = true;

    char label[12];
    settings_format_offset(app->settings.utc_offset_minutes, label, sizeof(label));
    variable_item_set_current_value_text(item, label);

    clock_view_set_utc_offset(app->clock_view, app->settings.utc_offset_minutes);
    (void)settings_save(app->storage, &app->settings);
}

SettingsView* settings_view_alloc(App* app) {
    furi_check(app);

    SettingsView* settings_view = malloc(sizeof(SettingsView));
    furi_check(settings_view);
    memset(settings_view, 0, sizeof(SettingsView));
    settings_view->app = app;

    settings_view->list = variable_item_list_alloc();
    settings_view->offset_item = variable_item_list_add(
        settings_view->list,
        "UTC Offset",
        SETTINGS_OFFSET_COUNT,
        settings_view_offset_changed,
        app);

    uint8_t index = settings_offset_to_index(app->settings.utc_offset_minutes);
    variable_item_set_current_value_index(settings_view->offset_item, index);

    char label[12];
    settings_format_offset(app->settings.utc_offset_minutes, label, sizeof(label));
    variable_item_set_current_value_text(settings_view->offset_item, label);

    return settings_view;
}

void settings_view_free(SettingsView* settings_view) {
    furi_check(settings_view);
    variable_item_list_free(settings_view->list);
    free(settings_view);
}

VariableItemList* settings_view_get_list(SettingsView* settings_view) {
    furi_check(settings_view);
    return settings_view->list;
}

View* settings_view_get_view(SettingsView* settings_view) {
    furi_check(settings_view);
    return variable_item_list_get_view(settings_view->list);
}

void settings_view_sync_from_app(SettingsView* settings_view) {
    furi_check(settings_view);
    App* app = settings_view->app;

    uint8_t index = settings_offset_to_index(app->settings.utc_offset_minutes);
    variable_item_set_current_value_index(settings_view->offset_item, index);

    char label[12];
    settings_format_offset(app->settings.utc_offset_minutes, label, sizeof(label));
    variable_item_set_current_value_text(settings_view->offset_item, label);
}

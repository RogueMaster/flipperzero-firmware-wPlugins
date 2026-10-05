/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * How the reader behaves.
 *
 * Every item here is written back to the settings file when the screen is
 * left, so a choice made once holds for every later run.
 */
#include "../emrtd_i.h"

#include "../access/emrtd_access.h"

typedef enum {
    EmrtdSceneOptionsIndexMethod,
    EmrtdSceneOptionsIndexDataGroups,
    EmrtdSceneOptionsIndexExport,
    EmrtdSceneOptionsIndexTrace,
    EmrtdSceneOptionsIndexRemember,
} EmrtdSceneOptionsIndex;

static const char* const emrtd_scene_options_switch[] = {"Off", "On"};

/*
 * One of the rows below depends on another row, so what it says is worked out
 * in one place and used both when the screen is built and when the row itself
 * is toggled. Without the second use the hint would survive only until the
 * user pressed the very row it is attached to.
 */

/* The trace is written into the export directory, so it cannot happen at all
 * with exporting off; saying "On" there would be a lie. */
static void emrtd_scene_options_show_trace(const Emrtd* app, VariableItem* item) {
    if(app->config.write_trace && !app->config.export_to_sd) {
        variable_item_set_current_value_text(item, "Needs export");
    } else {
        variable_item_set_current_value_text(
            item, emrtd_scene_options_switch[app->config.write_trace ? 1 : 0]);
    }
}

static void emrtd_scene_options_method_changed(VariableItem* item) {
    Emrtd* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->config.method = (EmrtdAccessMethod)index;
    variable_item_set_current_value_text(item, emrtd_access_method_name(app->config.method));
}

static void emrtd_scene_options_export_changed(VariableItem* item) {
    Emrtd* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->config.export_to_sd = index != 0;
    variable_item_set_current_value_text(item, emrtd_scene_options_switch[index]);
}

static void emrtd_scene_options_trace_changed(VariableItem* item) {
    Emrtd* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->config.write_trace = index != 0;
    emrtd_scene_options_show_trace(app, item);
}

static void emrtd_scene_options_remember_changed(VariableItem* item) {
    Emrtd* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    app->remember_credentials = index != 0;
    variable_item_set_current_value_text(item, emrtd_scene_options_switch[index]);
}

static void emrtd_scene_options_enter_callback(void* context, uint32_t index) {
    furi_assert(context);
    Emrtd* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

/** How many of the optional groups are switched on, for the summary line. */
static void emrtd_scene_options_group_summary(const Emrtd* app, char* out, size_t out_size) {
    uint8_t selected = 0;
    uint8_t selectable = 0;

    for(size_t id = 0; id < EmrtdFileCount; id++) {
        const EmrtdFileInfo* info = emrtd_file_info((EmrtdFileId)id);
        if(info == NULL || info->dg_number < 0 || info->eac_protected) {
            continue;
        }
        selectable++;
        if(app->config.files & EMRTD_FILE_BIT(id)) {
            selected++;
        }
    }

    snprintf(out, out_size, "%u/%u", selected, selectable);
}

void emrtd_scene_options_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    VariableItemList* list = app->variable_item_list;
    VariableItem* item;

    /* The list keeps the label pointers it is given, so every label here is a
     * literal that outlives the screen. */
    item =
        variable_item_list_add(list, "Access method", 3, emrtd_scene_options_method_changed, app);
    variable_item_set_current_value_index(item, (uint8_t)app->config.method);
    variable_item_set_current_value_text(item, emrtd_access_method_name(app->config.method));

    char summary[16];
    emrtd_scene_options_group_summary(app, summary, sizeof(summary));
    item = variable_item_list_add(list, "Data groups", 1, NULL, app);
    variable_item_set_current_value_text(item, summary);

    item =
        variable_item_list_add(list, "Export to SD", 2, emrtd_scene_options_export_changed, app);
    variable_item_set_current_value_index(item, app->config.export_to_sd ? 1 : 0);
    variable_item_set_current_value_text(
        item, emrtd_scene_options_switch[app->config.export_to_sd ? 1 : 0]);

    item = variable_item_list_add(list, "APDU trace", 2, emrtd_scene_options_trace_changed, app);
    variable_item_set_current_value_index(item, app->config.write_trace ? 1 : 0);
    emrtd_scene_options_show_trace(app, item);

    item = variable_item_list_add(
        list, "Remember on SD", 2, emrtd_scene_options_remember_changed, app);
    variable_item_set_current_value_index(item, app->remember_credentials ? 1 : 0);
    variable_item_set_current_value_text(
        item, emrtd_scene_options_switch[app->remember_credentials ? 1 : 0]);

    variable_item_list_set_enter_callback(list, emrtd_scene_options_enter_callback, app);
    variable_item_list_set_selected_item(
        list, (uint8_t)scene_manager_get_scene_state(app->scene_manager, EmrtdSceneOptions));

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewVariableItemList);
}

bool emrtd_scene_options_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    Emrtd* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EmrtdSceneOptionsIndexDataGroups) {
            scene_manager_next_scene(app->scene_manager, EmrtdSceneDataGroups);
            consumed = true;
        }
    }

    return consumed;
}

void emrtd_scene_options_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    /* Come back to the item that was left, not to the top of the list. */
    scene_manager_set_scene_state(
        app->scene_manager,
        EmrtdSceneOptions,
        variable_item_list_get_selected_item_index(app->variable_item_list));

    variable_item_list_reset(app->variable_item_list);

    /*
     * Saving on the way out rather than on every keystroke keeps the card
     * writes down, and the screen cannot be left any other way.
     */
    emrtd_settings_save(app);
}

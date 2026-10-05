#include "../k2_rfid_app.h"
#include "k2_scene.h"

enum ConfigItems {
    ConfigItemPrinter,
    ConfigItemMaterial,
    ConfigItemColor,
    ConfigItemWeight,
    ConfigItemSerial,
    ConfigItemActions,
};

static void k2_scene_config_printer_change(VariableItem* item) {
    K2RfidApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->printer_idx = index;
    variable_item_set_current_value_text(item, k2_db_get_printer_name(index));
    k2_rfid_app_sync_config(app);
}

static void k2_scene_config_material_change(VariableItem* item) {
    K2RfidApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->material_idx = index;
    const K2Material* mat = k2_db_get_material(index);
    variable_item_set_current_value_text(item, mat ? mat->name : "Unknown");
    k2_rfid_app_sync_config(app);
}

static void k2_scene_config_color_change(VariableItem* item) {
    K2RfidApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->color_idx = index;
    const K2ColorPreset* col = k2_db_get_color(index);
    variable_item_set_current_value_text(item, col ? col->name : "Unknown");
    k2_rfid_app_sync_config(app);
}

static void k2_scene_config_weight_change(VariableItem* item) {
    K2RfidApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->weight_idx = index;
    const K2WeightOption* w = k2_db_get_weight(index);
    variable_item_set_current_value_text(item, w ? w->label : "Unknown");
    k2_rfid_app_sync_config(app);
}

static void k2_scene_config_enter_callback(void* context, uint32_t index) {
    K2RfidApp* app = context;
    if (index == ConfigItemSerial) {
        /* Randomize serial on enter */
        k2_rfid_app_randomize_serial(app);
        scene_manager_previous_scene(app->scene_manager);
        scene_manager_next_scene(app->scene_manager, K2SceneConfig);
    } else if (index == ConfigItemActions) {
        /* Return to main menu */
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, K2SceneMainMenu);
    }
}

void k2_scene_config_on_enter(void* context) {
    K2RfidApp* app = context;
    VariableItemList* vil = app->var_item_list;

    variable_item_list_reset(vil);

    /* 1. Printer Model */
    VariableItem* item_pr = variable_item_list_add(
        vil,
        "Printer",
        k2_db_get_printer_count(),
        k2_scene_config_printer_change,
        app);
    variable_item_set_current_value_index(item_pr, app->printer_idx);
    variable_item_set_current_value_text(item_pr, k2_db_get_printer_name(app->printer_idx));

    /* 2. Material */
    VariableItem* item_mat = variable_item_list_add(
        vil,
        "Filament",
        k2_db_get_material_count(),
        k2_scene_config_material_change,
        app);
    variable_item_set_current_value_index(item_mat, app->material_idx);
    const K2Material* mat = k2_db_get_material(app->material_idx);
    variable_item_set_current_value_text(item_mat, mat ? mat->name : "Unknown");

    /* 3. Color */
    VariableItem* item_col = variable_item_list_add(
        vil,
        "Color",
        k2_db_get_color_count(),
        k2_scene_config_color_change,
        app);
    variable_item_set_current_value_index(item_col, app->color_idx);
    const K2ColorPreset* col = k2_db_get_color(app->color_idx);
    variable_item_set_current_value_text(item_col, col ? col->name : "Unknown");

    /* 4. Weight */
    VariableItem* item_w = variable_item_list_add(
        vil,
        "Weight",
        k2_db_get_weight_count(),
        k2_scene_config_weight_change,
        app);
    variable_item_set_current_value_index(item_w, app->weight_idx);
    const K2WeightOption* w = k2_db_get_weight(app->weight_idx);
    variable_item_set_current_value_text(item_w, w ? w->label : "Unknown");

    /* 5. Serial number button */
    VariableItem* item_sn = variable_item_list_add(
        vil,
        "Serial (OK:Rand)",
        1,
        NULL,
        app);
    variable_item_set_current_value_text(item_sn, app->serial_str);

    /* 6. Done button */
    variable_item_list_add(vil, "Done", 1, NULL, app);

    variable_item_list_set_enter_callback(vil, k2_scene_config_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewVariableItemList);
}

bool k2_scene_config_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void k2_scene_config_on_exit(void* context) {
    K2RfidApp* app = context;
    k2_rfid_app_sync_config(app);
    variable_item_list_reset(app->var_item_list);
}

#include "../lidaremulator_app_i.h"

#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#define LIDAREMULATOR_SETTINGS_PATH INT_PATH(".lidaremulator.settings")
#define LIDAREMULATOR_SETTINGS_VERSION (2)
#define LIDAREMULATOR_SETTINGS_MAGIC (0x4C)

typedef struct {
    LidarEmulatorIrOutput ir_output;
    bool ir_ext_5v_enabled;
} LidarEmulatorSettings;

static const char* lidaremulator_ir_output_text[] = {
    "Internal",
    "Ext. 2(A7)",
};

static const char* lidaremulator_5v_text[] = {
    "OFF",
    "ON",
};

static void lidaremulator_scene_settings_ir_output_change_callback(VariableItem* item) {
    LidarEmulatorApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, lidaremulator_ir_output_text[index]);
    app->ir_output = (LidarEmulatorIrOutput)index;
}

static void lidaremulator_scene_settings_5v_change_callback(VariableItem* item) {
    LidarEmulatorApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, lidaremulator_5v_text[index]);
    app->ir_ext_5v_enabled = (index != 0);
}

static void lidaremulator_save_settings(LidarEmulatorApp* app) {
    LidarEmulatorSettings settings = {
        .ir_output = app->ir_output,
        .ir_ext_5v_enabled = app->ir_ext_5v_enabled,
    };
    saved_struct_save(
        LIDAREMULATOR_SETTINGS_PATH,
        &settings,
        sizeof(settings),
        LIDAREMULATOR_SETTINGS_MAGIC,
        LIDAREMULATOR_SETTINGS_VERSION);
}

void lidaremulator_load_settings(LidarEmulatorApp* app) {
    LidarEmulatorSettings settings = {0};
    if(saved_struct_load(
           LIDAREMULATOR_SETTINGS_PATH,
           &settings,
           sizeof(settings),
           LIDAREMULATOR_SETTINGS_MAGIC,
           LIDAREMULATOR_SETTINGS_VERSION)) {
        app->ir_output = settings.ir_output;
        app->ir_ext_5v_enabled = settings.ir_ext_5v_enabled;
    } else {
        /* Try v1 format for migration */
        LidarEmulatorSettings v1 = {0};
        if(saved_struct_load(
               LIDAREMULATOR_SETTINGS_PATH,
               &v1,
               sizeof(LidarEmulatorIrOutput),
               LIDAREMULATOR_SETTINGS_MAGIC,
               1)) {
            app->ir_output = v1.ir_output;
            app->ir_ext_5v_enabled = true;
        }
    }
}


void lidaremulator_scene_settings_on_enter(void* context) {
    LidarEmulatorApp* app = context;
    VariableItemList* var_item_list = app->variable_item_list;
    VariableItem* item;

    variable_item_list_reset(var_item_list);

    item = variable_item_list_add(
        var_item_list,
        "IR Output",
        LidarEmulatorIrOutputNum,
        lidaremulator_scene_settings_ir_output_change_callback,
        app);

    variable_item_set_current_value_index(item, app->ir_output);
    variable_item_set_current_value_text(item, lidaremulator_ir_output_text[app->ir_output]);

    item = variable_item_list_add(
        var_item_list,
        "5V on GPIO",
        2,
        lidaremulator_scene_settings_5v_change_callback,
        app);
    variable_item_set_current_value_index(item, app->ir_ext_5v_enabled ? 1 : 0);
    variable_item_set_current_value_text(item, lidaremulator_5v_text[app->ir_ext_5v_enabled ? 1 : 0]);

    view_dispatcher_switch_to_view(app->view_dispatcher, LidarEmulatorViewVariableList);
}

bool lidaremulator_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void lidaremulator_scene_settings_on_exit(void* context) {
    LidarEmulatorApp* app = context;
    variable_item_list_reset(app->variable_item_list);
    lidaremulator_save_settings(app);
}

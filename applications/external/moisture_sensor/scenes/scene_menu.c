#include "../moisture_sensor.h"

#define MENU_INDEX_DRY   0
#define MENU_INDEX_WET   1
#define MENU_INDEX_RESET 2
#define MENU_INDEX_SAVE  3

static void show_popup(
    MoistureSensorApp* app,
    const char* message,
    bool return_to_main,
    const NotificationSequence* sequence) {
    app->popup_message = message;
    app->popup_return_to_main = return_to_main;
    notification_message(app->notifications, sequence);
    scene_manager_next_scene(app->scene_manager, MoistureSensorScenePopup);
}

static void update_item_value_text(VariableItem* item, uint16_t value) {
    static char buf[8];
    snprintf(buf, sizeof(buf), "%d", value);
    variable_item_set_current_value_text(item, buf);
}

static void menu_dry_changed(VariableItem* item) {
    MoistureSensorApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->edit_dry_value = SENSOR_MIN_THRESHOLD + (index * ADC_STEP);
    if(app->edit_dry_value > ADC_MAX_VALUE) app->edit_dry_value = ADC_MAX_VALUE;
    update_item_value_text(item, app->edit_dry_value);
}

static void menu_wet_changed(VariableItem* item) {
    MoistureSensorApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->edit_wet_value = SENSOR_MIN_THRESHOLD + (index * ADC_STEP);
    if(app->edit_wet_value > ADC_MAX_VALUE) app->edit_wet_value = ADC_MAX_VALUE;
    update_item_value_text(item, app->edit_wet_value);
}

static void menu_enter_callback(void* context, uint32_t index) {
    MoistureSensorApp* app = context;

    if(index == MENU_INDEX_RESET) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MoistureSensorEventReset);
    } else if(index == MENU_INDEX_SAVE) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MoistureSensorEventSave);
    }
}

void moisture_sensor_scene_menu_on_enter(void* context) {
    MoistureSensorApp* app = context;

    VariableItemList* list = app->variable_item_list;
    variable_item_list_reset(list);

    uint8_t num_steps = (ADC_MAX_VALUE - SENSOR_MIN_THRESHOLD) / ADC_STEP + 1;

    app->item_dry = variable_item_list_add(list, "Dry (ADC)", num_steps, menu_dry_changed, app);
    uint8_t dry_index = (app->edit_dry_value - SENSOR_MIN_THRESHOLD) / ADC_STEP;
    variable_item_set_current_value_index(app->item_dry, dry_index);
    update_item_value_text(app->item_dry, app->edit_dry_value);

    app->item_wet = variable_item_list_add(list, "Wet (ADC)", num_steps, menu_wet_changed, app);
    uint8_t wet_index = (app->edit_wet_value - SENSOR_MIN_THRESHOLD) / ADC_STEP;
    variable_item_set_current_value_index(app->item_wet, wet_index);
    update_item_value_text(app->item_wet, app->edit_wet_value);

    variable_item_list_add(list, "Reset Defaults", 0, NULL, app);
    variable_item_list_add(list, "Save", 0, NULL, app);

    variable_item_list_set_enter_callback(list, menu_enter_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MoistureSensorViewMenu);
}

bool moisture_sensor_scene_menu_on_event(void* context, SceneManagerEvent event) {
    MoistureSensorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MoistureSensorEventReset) {
            app->edit_dry_value = ADC_DRY_DEFAULT;
            app->edit_wet_value = ADC_WET_DEFAULT;

            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->cal_dry_value = ADC_DRY_DEFAULT;
            app->cal_wet_value = ADC_WET_DEFAULT;
            furi_mutex_release(app->mutex);

            if(calibration_save(app)) {
                show_popup(app, "Defaults restored!", true, &sequence_success);
            } else {
                show_popup(app, "Save failed!", false, &sequence_error);
            }
            consumed = true;

        } else if(event.event == MoistureSensorEventSave) {
            if(app->edit_dry_value <= app->edit_wet_value) {
                show_popup(app, "Dry must be > Wet!", false, &sequence_error);
            } else {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->cal_dry_value = app->edit_dry_value;
                app->cal_wet_value = app->edit_wet_value;
                furi_mutex_release(app->mutex);

                if(calibration_save(app)) {
                    show_popup(app, "Saved!", true, &sequence_success);
                } else {
                    show_popup(app, "Save failed!", false, &sequence_error);
                }
            }
            consumed = true;
        }
    }
    return consumed;
}

void moisture_sensor_scene_menu_on_exit(void* context) {
    MoistureSensorApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}

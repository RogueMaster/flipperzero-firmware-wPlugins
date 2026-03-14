#pragma once

/* View IDs for ViewDispatcher */
typedef enum {
    ViewMain,
    ViewMainMenu,
    ViewSettings,
    ViewSensorsList,
    ViewSensorEdit,
    ViewSensorActions,
    ViewSensorInfo,
    ViewWidget,
    ViewPopup,
    ViewSettingsMenu,
    ViewCO2Settings,
    ViewClimateSettings,
    ViewsCount
} AppViews;

/* Main sensor display view */
void view_main_alloc(void);
void view_main_switch(void);
void view_main_free(void);

/* Main menu view */
void view_main_menu_alloc(void);
void view_main_menu_switch(void);
void view_main_menu_free(void);

/* Settings view */
void view_settings_alloc(void);
void view_settings_switch(void);
void view_settings_free(void);

/* Sensors list (add new sensor) */
void view_sensors_list_alloc(void);
void view_sensors_list_switch(void);
void view_sensors_list_free(void);

/* Sensor editor (offset, name, GPIO, save) */
void view_sensor_edit_alloc(void);
void view_sensor_edit_switch(Sensor* sensor);
void view_sensor_edit_free(void);

/* Sensor actions (info, edit, delete, add) */
void view_sensor_actions_alloc(void);
void view_sensor_actions_switch(Sensor* sensor);
void view_sensor_actions_free(void);

/* Sensor info (live readings for selected sensor) */
void view_sensor_info_alloc(void);
void view_sensor_info_switch(Sensor* sensor);
void view_sensor_info_free(void);

/* Widget (delete confirmation, help, about) */
void view_widgets_alloc(void);
void view_widgets_free(void);
void view_widget_delete_switch(Sensor* sensor);
void view_widget_help_switch(void);
void view_widget_about_switch(void);
void view_widget_help_from_settings_switch(void);
void view_widget_about_from_settings_switch(void);

/* Settings menu (top-level submenu) */
void view_settings_menu_alloc(void);
void view_settings_menu_switch(void);
void view_settings_menu_free(void);

/* CO2 sensor settings */
void view_co2_settings_alloc(void);
void view_co2_settings_switch(void);
void view_co2_settings_free(void);

/* Climate sensor settings */
void view_climate_settings_alloc(void);
void view_climate_settings_switch(void);
void view_climate_settings_free(void);

/* Popup */
void view_popup(const Icon* icon, char* header, char* message, uint32_t prev_view_id);

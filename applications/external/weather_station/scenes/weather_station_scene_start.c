#include "../weather_station_app_i.h"
#include <dialogs/dialogs.h>
#include <weather_station_icons.h>

typedef enum {
    SubmenuIndexWeatherStationReceiver,
    SubmenuIndexWeatherStationReadRaw,
    SubmenuIndexWeatherStationAbout,
} SubmenuIndex;

static bool weather_station_scene_start_select_raw(WeatherStationApp* app) {
    if(furi_string_empty(app->file_path)) furi_string_set(app->file_path, EXT_PATH("subghz"));

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".sub", &I_station_icon);
    options.base_path = EXT_PATH("subghz");
    options.hide_dot_files = true;

    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    bool selected = dialog_file_browser_show(dialogs, app->file_path, app->file_path, &options);
    furi_record_close(RECORD_DIALOGS);
    return selected;
}

void weather_station_scene_start_submenu_callback(void* context, uint32_t index) {
    WeatherStationApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void weather_station_scene_start_on_enter(void* context) {
    UNUSED(context);
    WeatherStationApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_add_item(
        submenu,
        "Read Weather Station",
        SubmenuIndexWeatherStationReceiver,
        weather_station_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Read RAW File",
        SubmenuIndexWeatherStationReadRaw,
        weather_station_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "About",
        SubmenuIndexWeatherStationAbout,
        weather_station_scene_start_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, WeatherStationSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, WeatherStationViewSubmenu);
}

bool weather_station_scene_start_on_event(void* context, SceneManagerEvent event) {
    WeatherStationApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexWeatherStationAbout) {
            scene_manager_next_scene(app->scene_manager, WeatherStationSceneAbout);
            consumed = true;
        } else if(event.event == SubmenuIndexWeatherStationReceiver) {
            scene_manager_next_scene(app->scene_manager, WeatherStationSceneReceiver);
            consumed = true;
        } else if(event.event == SubmenuIndexWeatherStationReadRaw) {
            if(weather_station_scene_start_select_raw(app)) {
                scene_manager_next_scene(app->scene_manager, WeatherStationSceneReadRaw);
            }
            consumed = true;
        }
        scene_manager_set_scene_state(app->scene_manager, WeatherStationSceneStart, event.event);
    }

    return consumed;
}

void weather_station_scene_start_on_exit(void* context) {
    WeatherStationApp* app = context;
    submenu_reset(app->submenu);
}

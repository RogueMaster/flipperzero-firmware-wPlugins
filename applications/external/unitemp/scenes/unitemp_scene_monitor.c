/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk (https://github.com/quen0n)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "../unitemp.h"
#include "../helpers/unitemp_utils.h"

static UnitempView view_mode = UnitempViewNoSensors;

void unitemp_scene_monitor_on_enter(void* context) {
    furi_assert(context);
    UnitempApp* app = context;

    if(app->settings->infinity_backlight) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }

    if(view_mode == UnitempViewNoSensors || unitemp_sensors_get_count() < 2) {
        if(unitemp_sensors_get_count() == 0) {
            view_mode = UnitempViewNoSensors;
        } else if(unitemp_sensors_get_count() == 1) {
            view_mode = UnitempViewSingleSensor;
        } else {
            view_mode = UnitempViewTempOverview;
        }
    }
    /* Start the reader thread. It will talk to the thermometer in the background. */
    furi_thread_start(app->reader_thread);
    view_dispatcher_switch_to_view(app->view_dispatcher, view_mode);
}

bool unitemp_scene_monitor_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    UnitempApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        if(view_mode == UnitempViewSingleSensor) {
            single_sensor_refresh_data(app->single_sensor);
        } else if(view_mode == UnitempViewTempOverview) {
            temp_overview_refresh_data(app->temp_overview);
        }
        consumed = true;
    }
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == CustomEventSwitchToSingleSensorView) {
            view_mode = UnitempViewSingleSensor;
            view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewSingleSensor);
            consumed = true;
        } else if(event.event == CustomEventSwitchToTempOverviewView) {
            view_mode = UnitempViewTempOverview;
            view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewTempOverview);
            consumed = true;
        } else if(event.event == CustomEventSwitchToSensorInfoView) {
            view_mode = UnitempViewSensorInfo;
            view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewSensorInfo);
            consumed = true;
        }
    }

    return consumed;
}

void unitemp_scene_monitor_on_exit(void* context) {
    furi_assert(context);
    UnitempApp* app = context;
    if(app->settings->infinity_backlight) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    }
    unitemp_reset_environment_state(app->notifications);
    /* Signal the reader thread to cease operation and exit */
    furi_thread_flags_set(furi_thread_get_id(app->reader_thread), UnitempThreadFlagExit);

    /* Wait for the reader thread to finish */
    furi_thread_join(app->reader_thread);
}

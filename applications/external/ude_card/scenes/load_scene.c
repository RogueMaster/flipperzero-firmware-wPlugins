/*
This file is part of UDECard App.
A Flipper Zero application to analyse student ID cards from the University of Duisburg-Essen (Intercard)

Copyright (C) 2025-2026 Alexander Hahn

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

#include "load_scene.h"

#include "udecard_app_i.h"

bool udecard_load_scene_file_path_dialog(App* app);

void udecard_load_scene_on_enter(void* context) {
    App* app = context;

    if(udecard_load_scene_file_path_dialog(app)) {
        UDECardLoadingResult loading_result = udecard_load_from_path(app->udecard, app->file_path);
        if(loading_result != UDECardLoadingResultSuccess) {
            view_dispatcher_send_custom_event(app->view_dispatcher, UDECardLoadSceneFailedEvent);
            return;
        }
        UDECardParsingResult parsing_result = udecard_parse(app->udecard, app->udecard->carddata);
        if(parsing_result & UDECardParsingResultErrorMagic) { // other parsing errors are not fatal
            view_dispatcher_send_custom_event(app->view_dispatcher, UDECardLoadSceneFailedEvent);
            return;
        }
        view_dispatcher_send_custom_event(app->view_dispatcher, UDECardLoadSceneSuccessEvent);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, UDECardLoadSceneCancelledEvent);
    }
}

bool udecard_load_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case UDECardLoadSceneSuccessEvent:
            scene_manager_next_scene(app->scene_manager, UDECardResultsScene);
            consumed = true;
            break;
        case UDECardLoadSceneFailedEvent:
            if(app->udecard->loading_result != UDECardLoadingResultSuccess) {
                udecard_app_error_dialog(
                    app, udecard_loading_error_string(app->udecard->loading_result));
            } else { // parsing error
                udecard_app_error_dialog(app, "Not a UDECard.");
            }
            udecard_load_scene_on_enter(context); // reopen file dialog
            consumed = true;
            break;
        case UDECardLoadSceneCancelledEvent:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        }
        break;
    default:
        break;
    }

    return consumed;
}

void udecard_load_scene_on_exit(void* context) {
    App* app = context;

    UNUSED(app);
    // nothing to do here
}

bool udecard_load_scene_file_path_dialog(App* app) {
    DialogsFileBrowserOptions browser_options;

    dialog_file_browser_set_basic_options(&browser_options, ".nfc", &I_Nfc_10x10);
    browser_options.base_path = EXT_PATH("nfc");
    browser_options.hide_dot_files = true;

    bool result = dialog_file_browser_show(
        app->dialogs_app, app->file_path, app->file_path, &browser_options);

    return result;
}

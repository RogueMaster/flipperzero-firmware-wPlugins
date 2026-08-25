#include "scene_wifi_connect.h"

#include "../api_caller.h"
#include "../utils/wifi_history.h"
#include "../wifi/wifi_manager.h"

typedef enum {
    WifiConnectEventPasswordEntered,
} WifiConnectEvent;

typedef enum {
    WifiConnectModeInput, // Ask the password via TextInput
    WifiConnectModeSaved, // Use the saved password from the history
} WifiConnectMode;

static void api_caller_scene_wifi_connect_input_callback(void* context) {
    furi_assert(context);
    AppContext* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WifiConnectEventPasswordEntered);
}

/** Connect with app->ssid/app->password and show the result in the TextBox. */
static void api_caller_scene_wifi_connect_try_connect(AppContext* app) {
    FuriString* reply = furi_string_alloc();

    bool ok = wifi_manager_save_and_connect(app, app->ssid, app->password, reply);

    if(ok) {
        wifi_history_add(app, app->ssid, app->password);
    }

    text_box_reset(app->text_box);
    if(ok) {
        text_box_set_text(app->text_box, locale_get(app, LocKeyWifiConnectedOk));
    } else {
        FuriString* message = furi_string_alloc_printf(
            locale_get(app, LocKeyWifiConnectFailed), furi_string_get_cstr(reply));
        text_box_set_text(app->text_box, furi_string_get_cstr(message));
        furi_string_free(message);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ApiCallerViewTextBox);
    furi_string_free(reply);
}

void api_caller_scene_wifi_connect_on_enter(void* context) {
    furi_assert(context);
    AppContext* app = context;

    uint32_t mode = scene_manager_get_scene_state(app->scene_manager, ApiCallerSceneWifiConnect);
    scene_manager_set_scene_state(app->scene_manager, ApiCallerSceneWifiConnect, 0);

    if(mode == WifiConnectModeSaved) {
        // The password is already in app->password: connect right away
        api_caller_scene_wifi_connect_try_connect(app);
        return;
    }

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, locale_get(app, LocKeyWifiPasswordHeader));
    memset(app->password, 0, sizeof(app->password));

    text_input_set_result_callback(
        app->text_input,
        api_caller_scene_wifi_connect_input_callback,
        app,
        app->password,
        sizeof(app->password),
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, ApiCallerViewTextInput);
}

bool api_caller_scene_wifi_connect_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    AppContext* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == WifiConnectEventPasswordEntered) {
        api_caller_scene_wifi_connect_try_connect(app);
        consumed = true;
    }

    return consumed;
}

void api_caller_scene_wifi_connect_on_exit(void* context) {
    furi_assert(context);
    AppContext* app = context;
    text_input_reset(app->text_input);
}

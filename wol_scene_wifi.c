#include "wol_flipper.h"

#include <string.h>

typedef enum {
    WifiIndexSsid,
    WifiIndexPassword,
    WifiIndexTest,
} WifiIndex;

static char wifi_label_ssid[WOL_SSID_LEN + 8];
static char wifi_label_pass[24];

static void wol_scene_wifi_callback(void* context, uint32_t index) {
    WolApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void wol_scene_wifi_on_enter(void* context) {
    WolApp* app = context;

    snprintf(
        wifi_label_ssid,
        sizeof(wifi_label_ssid),
        "SSID: %s",
        app->config.ssid[0] ? app->config.ssid : "<unset>");

    size_t pass_len = strlen(app->config.pass);
    if(pass_len == 0) {
        wol_strcpy(wifi_label_pass, sizeof(wifi_label_pass), "Password: <none>");
    } else {
        if(pass_len > 8) pass_len = 8;
        char stars[9];
        memset(stars, '*', pass_len);
        stars[pass_len] = '\0';
        snprintf(wifi_label_pass, sizeof(wifi_label_pass), "Password: %s", stars);
    }

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Wi-Fi setup");
    submenu_add_item(app->submenu, wifi_label_ssid, WifiIndexSsid, wol_scene_wifi_callback, app);
    submenu_add_item(
        app->submenu, wifi_label_pass, WifiIndexPassword, wol_scene_wifi_callback, app);
    submenu_add_item(app->submenu, "Test connection", WifiIndexTest, wol_scene_wifi_callback, app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, WolSceneWifi));
    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewSubmenu);
}

bool wol_scene_wifi_on_event(void* context, SceneManagerEvent event) {
    WolApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event > WifiIndexTest) return false;

    scene_manager_set_scene_state(app->scene_manager, WolSceneWifi, event.event);

    switch(event.event) {
    case WifiIndexSsid:
        app->text_field = WolTextFieldSsid;
        wol_strcpy(app->text_buf, sizeof(app->text_buf), app->config.ssid);
        scene_manager_next_scene(app->scene_manager, WolSceneTextInput);
        return true;

    case WifiIndexPassword:
        app->text_field = WolTextFieldPassword;
        wol_strcpy(app->text_buf, sizeof(app->text_buf), app->config.pass);
        scene_manager_next_scene(app->scene_manager, WolSceneTextInput);
        return true;

    case WifiIndexTest:
        app->wifi_test_mode = true;
        scene_manager_next_scene(app->scene_manager, WolSceneSend);
        return true;

    default:
        return false;
    }
}

void wol_scene_wifi_on_exit(void* context) {
    WolApp* app = context;
    submenu_reset(app->submenu);
}

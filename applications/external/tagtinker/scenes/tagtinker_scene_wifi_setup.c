/*
 * WiFi Setup
 * ==========
 *
 * Two-step text input: SSID first, then password. State machine lives in
 * the scene_state field of the scene manager so we can re-enter cleanly
 * after the text_input view returns.
 *
 *   state=0 -> prompt SSID
 *   state=1 -> prompt password
 *   state=2 -> sent, return to plugins scene
 */
#include "../tagtinker_app.h"
#include "../wifi/tagtinker_wifi.h"

#include <string.h>

#define EVT_TEXT_DONE 0xC1u

static void text_done_cb(void* ctx) {
    TagTinkerApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, EVT_TEXT_DONE);
}

static void prompt_ssid(TagTinkerApp* app) {
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "WiFi SSID");
    /* Reuse cached creds so re-entering doesn't blank the field. */
    strncpy(app->wifi_creds_ssid, app->wifi_ssid, sizeof(app->wifi_creds_ssid) - 1);
    app->wifi_creds_ssid[sizeof(app->wifi_creds_ssid) - 1] = 0;
    text_input_set_result_callback(
        app->text_input,
        text_done_cb,
        app,
        app->wifi_creds_ssid,
        sizeof(app->wifi_creds_ssid),
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewTextInput);
}

static void prompt_password(TagTinkerApp* app) {
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Password");
    /* Don't pre-fill the password field for visual privacy. */
    app->wifi_creds_pwd[0] = 0;
    text_input_set_result_callback(
        app->text_input,
        text_done_cb,
        app,
        app->wifi_creds_pwd,
        sizeof(app->wifi_creds_pwd),
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewTextInput);
}

void tagtinker_scene_wifi_setup_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;
    scene_manager_set_scene_state(app->scene_manager, TagTinkerSceneWifiSetup, 0);
    prompt_ssid(app);
}

bool tagtinker_scene_wifi_setup_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event != EVT_TEXT_DONE) return false;

    uint32_t st = scene_manager_get_scene_state(app->scene_manager, TagTinkerSceneWifiSetup);
    if(st == 0) {
        scene_manager_set_scene_state(app->scene_manager, TagTinkerSceneWifiSetup, 1);
        prompt_password(app);
        return true;
    }
    /* Both fields collected - send to ESP and pop back. */
    if(app->wifi) {
        tagtinker_wifi_set_creds(
            (TagTinkerWifi*)app->wifi, app->wifi_creds_ssid, app->wifi_creds_pwd);
    }
    /* Wipe the password from app memory once it's on the wire. */
    memset(app->wifi_creds_pwd, 0, sizeof(app->wifi_creds_pwd));
    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void tagtinker_scene_wifi_setup_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    text_input_reset(app->text_input);
}

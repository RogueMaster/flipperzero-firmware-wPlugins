#include "wi_fir_scene.h"

/* Handler arrays — order must match WiFirScene enum */

static const AppSceneOnEnterCallback wi_fir_scene_on_enter_handlers[] = {
    wi_fir_scene_main_menu_on_enter,
    wi_fir_scene_edit_ssid_on_enter,
    wi_fir_scene_edit_password_on_enter,
    wi_fir_scene_edit_security_on_enter,
    wi_fir_scene_confirm_on_enter,
    wi_fir_scene_transmit_on_enter,
    wi_fir_scene_scan_nfc_on_enter,
    wi_fir_scene_saved_on_enter,
    wi_fir_scene_about_on_enter,
    wi_fir_scene_settings_on_enter,
};

static const AppSceneOnEventCallback wi_fir_scene_on_event_handlers[] = {
    wi_fir_scene_main_menu_on_event,
    wi_fir_scene_edit_ssid_on_event,
    wi_fir_scene_edit_password_on_event,
    wi_fir_scene_edit_security_on_event,
    wi_fir_scene_confirm_on_event,
    wi_fir_scene_transmit_on_event,
    wi_fir_scene_scan_nfc_on_event,
    wi_fir_scene_saved_on_event,
    wi_fir_scene_about_on_event,
    wi_fir_scene_settings_on_event,
};

static const AppSceneOnExitCallback wi_fir_scene_on_exit_handlers[] = {
    wi_fir_scene_main_menu_on_exit,
    wi_fir_scene_edit_ssid_on_exit,
    wi_fir_scene_edit_password_on_exit,
    wi_fir_scene_edit_security_on_exit,
    wi_fir_scene_confirm_on_exit,
    wi_fir_scene_transmit_on_exit,
    wi_fir_scene_scan_nfc_on_exit,
    wi_fir_scene_saved_on_exit,
    wi_fir_scene_about_on_exit,
    wi_fir_scene_settings_on_exit,
};

const SceneManagerHandlers wi_fir_scene_handlers = {
    .on_enter_handlers = wi_fir_scene_on_enter_handlers,
    .on_event_handlers = wi_fir_scene_on_event_handlers,
    .on_exit_handlers = wi_fir_scene_on_exit_handlers,
    .scene_num = WiFirSceneCount,
};

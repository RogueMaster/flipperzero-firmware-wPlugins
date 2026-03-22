#pragma once

#include <gui/scene_manager.h>

/* Scene IDs */
typedef enum {
    WiFirSceneMainMenu,
    WiFirSceneEditSsid,
    WiFirSceneEditPassword,
    WiFirSceneEditSecurity,
    WiFirSceneConfirm,
    WiFirSceneTransmit,
    WiFirSceneScanNfc,
    WiFirSceneSaved,
    WiFirSceneAbout,
    WiFirSceneSettings,
    WiFirSceneCount,
} WiFirScene;

/* Custom event IDs */
typedef enum {
    WiFirCustomEventMainMenuCredentials,
    WiFirCustomEventMainMenuScanNfc,
    WiFirCustomEventMainMenuSaved,
    WiFirCustomEventMainMenuAbout,
    WiFirCustomEventTextInputDone,
    WiFirCustomEventSecurityDone,
    WiFirCustomEventConfirmSend,
    WiFirCustomEventConfirmBack,
    WiFirCustomEventTransmitDone,
    WiFirCustomEventTransmitFail,
    WiFirCustomEventNfcWifiFound,
    WiFirCustomEventNfcNotWifi,
    WiFirCustomEventNfcError,
    WiFirCustomEventSavedSelected,
    WiFirCustomEventConfirmDelete,
    WiFirCustomEventDeleteConfirmed,
    WiFirCustomEventDeleteCancelled,
    WiFirCustomEventMainMenuSettings,
    WiFirCustomEventAckSuccess,
    WiFirCustomEventAckFail,
    WiFirCustomEventAckTimeout,
    WiFirCustomEventAckDismiss,
} WiFirCustomEvent;

/* Scene manager handlers (defined in wi_fir_scene.c) */
extern const SceneManagerHandlers wi_fir_scene_handlers;

/* Forward declarations for all scene handlers */
void wi_fir_scene_main_menu_on_enter(void* context);
bool wi_fir_scene_main_menu_on_event(void* context, SceneManagerEvent event);
void wi_fir_scene_main_menu_on_exit(void* context);

void wi_fir_scene_edit_ssid_on_enter(void* context);
bool wi_fir_scene_edit_ssid_on_event(void* context, SceneManagerEvent event);
void wi_fir_scene_edit_ssid_on_exit(void* context);

void wi_fir_scene_edit_password_on_enter(void* context);
bool wi_fir_scene_edit_password_on_event(void* context, SceneManagerEvent event);
void wi_fir_scene_edit_password_on_exit(void* context);

void wi_fir_scene_edit_security_on_enter(void* context);
bool wi_fir_scene_edit_security_on_event(void* context, SceneManagerEvent event);
void wi_fir_scene_edit_security_on_exit(void* context);

void wi_fir_scene_confirm_on_enter(void* context);
bool wi_fir_scene_confirm_on_event(void* context, SceneManagerEvent event);
void wi_fir_scene_confirm_on_exit(void* context);

void wi_fir_scene_transmit_on_enter(void* context);
bool wi_fir_scene_transmit_on_event(void* context, SceneManagerEvent event);
void wi_fir_scene_transmit_on_exit(void* context);

void wi_fir_scene_scan_nfc_on_enter(void* context);
bool wi_fir_scene_scan_nfc_on_event(void* context, SceneManagerEvent event);
void wi_fir_scene_scan_nfc_on_exit(void* context);

void wi_fir_scene_saved_on_enter(void* context);
bool wi_fir_scene_saved_on_event(void* context, SceneManagerEvent event);
void wi_fir_scene_saved_on_exit(void* context);

void wi_fir_scene_about_on_enter(void* context);
bool wi_fir_scene_about_on_event(void* context, SceneManagerEvent event);
void wi_fir_scene_about_on_exit(void* context);

void wi_fir_scene_settings_on_enter(void* context);
bool wi_fir_scene_settings_on_event(void* context, SceneManagerEvent event);
void wi_fir_scene_settings_on_exit(void* context);

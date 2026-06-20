#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <infrared.h>
#include <infrared_worker.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "led_remote_types.h"
#include "helpers/led_remote_ir.h"
#include "scenes/led_remote_scene.h"
#include "views/led_remote_view_remote.h"

#define LED_REMOTE_APP_NAME "LED Remote"
#define LED_REMOTE_DATA_DIR "/ext/apps_data/led_remote"

// Remote type definition (physical model)
typedef struct {
    uint8_t button_count;
    uint8_t grid_cols;
    const char* type_name;
    const char* file_prefix;
    const char* const* button_names;
    const char* const* button_labels_short;
} LedRemoteTypeDef;

extern const LedRemoteTypeDef LED_REMOTE_TYPE_DEFS[LedRemoteTypeCount];

struct LedRemoteApp {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    Submenu* submenu;
    Popup* popup;
    TextInput* text_input;
    LedRemoteViewRemote* view_remote;

    InfraredWorker* ir_worker;
    LedRemoteSignal signals[LED_REMOTE_MAX_BUTTONS];
    bool learned[LED_REMOTE_MAX_BUTTONS];

    LedButton learn_target;

    Storage* storage;
    NotificationApp* notifications;

    char profile_name[32];
    char save_path[128];
    char capture_label[48];

    char profiles[LED_REMOTE_MAX_PROFILES][32];
    uint8_t profile_count;
    char profile_selected[32];
    bool profile_edit_is_new;
    char text_input_buf[32];

    LedRemoteLayout layout;
    LedRemoteType remote_type;

    uint8_t button_count;
    uint8_t grid_cols;
    const char* const* button_names;
    const char* const* button_labels_short;
};

typedef struct LedRemoteApp LedRemoteApp;

void led_remote_build_save_path(LedRemoteApp* app);

void led_remote_switch_profile(LedRemoteApp* app, const char* name);
void led_remote_switch_type(LedRemoteApp* app, LedRemoteType new_type);
void led_remote_apply_type(LedRemoteApp* app);

void led_remote_scan_profiles(LedRemoteApp* app);
void led_remote_rename_profile(LedRemoteApp* app, const char* old_name, const char* new_name);
void led_remote_delete_profile(LedRemoteApp* app, const char* name);

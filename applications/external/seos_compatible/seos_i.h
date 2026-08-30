#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>

#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>

#include <input/input.h>

#include <lib/nfc/nfc.h>
#include <nfc/nfc_listener.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_device.h>

#include "seos.h"
#include "keys.h"
#include "seos_credential.h"
#include "seos_common.h"
#include "seos_reader.h"
#include "seos_emulator.h"
#include "seos_ble_plugin.h"
#include "seos_protocol.h"
#include "scenes/seos_scene.h"
#include "cmac.h"
#include "seos_custom_event.h"

#define SEOS_TEXT_STORE_SIZE 128

struct Seos {
    bool is_debug_enabled;
    ViewDispatcher* view_dispatcher;
    Gui* gui;
    NotificationApp* notifications;
    SceneManager* scene_manager;

    char text_store[SEOS_TEXT_STORE_SIZE + 1];
    FuriString* text_box_store;

    // Common Views
    Submenu* submenu;
    Popup* popup;
    Loading* loading;
    TextInput* text_input;
    TextBox* text_box;
    Widget* widget;

    Nfc* nfc;
    NfcListener* listener;
    NfcPoller* poller;
    NfcDevice* nfc_device;

    SeosCredential* credential;

    // NFC
    SeosEmulator* seos_emulator;
    SeosReader* seos_reader;

    // BLE, loaded only while a scene needs it. has_external_ble is the saved
    // setting: whether the user has a dongle attached and wants it used.
    bool has_external_ble;
    const SeosBlePlugin* ble_plugin;
    void* ble_context;
    struct PluginManager* ble_manager;
    struct CompositeApiResolver* ble_resolver;
    FlowMode flow_mode;

    uint8_t keys_version;
    FuriString* active_key_file;
};

typedef enum {
    SeosViewMenu,
    SeosViewPopup,
    SeosViewLoading,
    SeosViewTextInput,
    SeosViewTextBox,
    SeosViewWidget,
} SeosView;

void seos_text_store_set(Seos* seos, const char* text, ...);

void seos_text_store_clear(Seos* seos);

void seos_blink_start(Seos* seos);

void seos_blink_stop(Seos* seos);

void seos_show_loading_popup(void* context, bool show);

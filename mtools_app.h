#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <nfc/nfc.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <notification/notification.h>
#include "nfc/magic_tag.h"

typedef enum {
    MToolsSceneHome,
    MToolsSceneMagicType,
    MToolsSceneUidChange,
    MToolsSceneAbout,
    MToolsSceneCount,
} MToolsScene;

typedef struct MToolsUidChanger MToolsUidChanger;

typedef struct MToolsApp {
    Gui* gui;
    Nfc* nfc;
    NfcScanner* scanner;
    NotificationApp* notifications;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    View* main_view;
    MToolsUidChanger* uid_changer;
    uint8_t selected_tool;
    uint8_t active_scene;
    uint8_t about_page;
    uint8_t about_list_page;
    FuriTimer* magic_anim_timer;
    uint8_t magic_anim_phase;
    bool magic_detecting;
    bool magic_result_ready;
    bool magic_read_handled;
    bool magic_finish_queued;
    uint32_t magic_anim_start_tick;
    uint8_t magic_pending_status;
    NfcListener* about_listener;
    MfUltralightData* about_ndef_data;
    uint8_t scan_status;
    NfcPoller* magic_poller;
    bool magic_scanning;
    bool magic_scan_found;
    uint8_t magic_protocol;
    bool magic_type2;
    uint8_t magic_iso_status;
    uint16_t magic_iso_blocks;
    uint8_t magic_iso_block_size;
    uint8_t magic_iso_ic_ref;
    uint8_t magic_uid[10];
    uint8_t magic_uid_len;
    uint8_t magic_sak;
    uint8_t magic_atqa[2];
} MToolsApp;

int32_t mtools_app(void* p);

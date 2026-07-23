#pragma once

#include <furi.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "ha_uart.h"
#include "ha_proto.h"
#include "ha_custom_event.h"
#include "scenes/ha_scene.h"

#define HA_SSID_MAX             (33) // 32 + NUL
#define HA_MAX_PLAYERS          (12)
#define HA_NICK_LEN             (20)
#define HA_MAX_ASSETS           (8)
#define HA_ASSET_PATH           (40)
#define HA_ASSET_MIME           (32)
#define HA_LINE_MAX             (512)
#define HA_LINK_TIMEOUT_MS      (5000)
#define HA_HANDSHAKE_TIMEOUT_MS (4000) // no ack progress -> board isn't our firmware
#define HA_CONSOLE_MAX          (3072)
#define HA_FILE_MAX             (60000) // max single web asset streamed to the ESP

#define HA_DATA_DIR    EXT_PATH("apps_data/hotspot_arcade")
#define HA_LOGS_DIR    HA_DATA_DIR "/logs"
#define HA_CONFIG_PATH HA_DATA_DIR "/config.txt"

// Content (ESP firmware, web bundle, trivia packs) ships inside the fap via
// fap_file_assets; the loader extracts it to apps_assets on launch, so a fresh install
// of just the .fap is playable with no SD setup. apps_assets is re-synced from the fap
// every launch, so anything a user drops there is lost: user content lives in apps_data
// instead, which the loader never touches. Both are read, apps_data winning on a clash.
#define HA_ASSETS_DIR   EXT_PATH("apps_assets/hotspot_arcade")
#define HA_FIRMWARE_DIR HA_ASSETS_DIR "/firmware"
// One flash manifest per supported board; the board picker chooses which to flash.
#define HA_OFFICIAL_FW  HA_FIRMWARE_DIR "/official_devboard/flash_official.txt"
#define HA_WROOM_FW     HA_FIRMWARE_DIR "/wroom/flash_wroom.txt"
// The C5 boots from 0x2000 rather than 0x1000; the offsets live in its manifest.
#define HA_C5_FW        HA_FIRMWARE_DIR "/c5/flash_c5.txt"

#define HA_BUNDLED_WEB_DIR HA_ASSETS_DIR "/web"
#define HA_USER_WEB_DIR    HA_DATA_DIR "/web"

#define HA_BUNDLED_PACKS_DIR  HA_ASSETS_DIR "/packs"
#define HA_USER_PACKS_DIR     HA_DATA_DIR "/packs"
// Compatibility: packs used to live in a trivia-only directory. Still read so a
// user's existing SD content does not vanish. Remove one release after the packs/
// layout ships.
#define HA_BUNDLED_TRIVIA_DIR HA_ASSETS_DIR "/trivia"
#define HA_USER_TRIVIA_DIR    HA_DATA_DIR "/trivia"

typedef enum {
    HaViewSubmenu,
    HaViewTextInput,
    HaViewWidget,
    HaViewVarItemList,
} HaView;

// A web bundle file, parsed from manifest.json, streamed to the ESP at start.
typedef struct {
    char path[HA_ASSET_PATH]; // URL path the ESP serves at ("/")
    char file[HA_ASSET_PATH]; // filename on SD, relative to app->web_dir
    char mime[HA_ASSET_MIME];
    bool gzip;
} HaAsset;

// A connected player, mirrored from the ESP (JOIN/LEAVE/SCORE).
typedef struct {
    bool used;
    uint8_t pid;
    char nick[HA_NICK_LEN];
    int32_t score;
} HaPlayer;

// Handshake sequence at session start (driven by ESP STATUS acks).
typedef enum {
    HaHsIdle,
    HaHsClear, // sent CLEAR_FILES, waiting "cleared"
    HaHsFiles, // streaming files, waiting "fok" per file
    HaHsSetAp, // sent SET_AP, waiting "ap_set"
    HaHsStart, // sent START, waiting "up"
    HaHsUp, // portal live
    HaHsErr,
} HaHandshake;

typedef struct HotspotArcadeApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    DialogsApp* dialogs;
    NotificationApp* notifications;

    Submenu* submenu;
    TextInput* text_input;
    Widget* widget;
    VariableItemList* var_item_list;

    HaUart* uart;

    // Config (persisted)
    FuriString* ssid;
    char ssid_buf[HA_SSID_MAX];
    bool sound_on;
    bool vibro_on;

    // Web bundle (from manifest.json)
    HaAsset assets[HA_MAX_ASSETS];
    uint8_t asset_count;
    // Dir the loaded manifest came from (user bundle if present, else the bundled one).
    // Asset files are read from here, so a user bundle is never mixed with bundled files.
    const char* web_dir;

    // Live roster
    HaPlayer players[HA_MAX_PLAYERS];

    // Active game (HA_GAME_*)
    uint8_t active_game;

    // Event feed / console
    FuriString* console; // scrollable raw event log
    FuriString* last_event; // most recent host-facing event line

    // RX frame parser (ESP -> Flipper)
    uint8_t rx_state;
    uint8_t rx_type;
    uint16_t rx_len;
    uint16_t rx_idx;
    uint8_t rx_crc;
    uint8_t rx_buf[HA_MAX_PAYLOAD + 1];

    // Status / lifecycle
    FuriString* status;
    bool portal_running;
    bool session_active;
    bool menu_shows_active;
    HaHandshake hs;
    uint8_t file_idx; // during HaHsFiles
    uint32_t last_handshake_tick; // rate-limits auto-reconnect so a brownout-reboot
        // loop can't tight-loop the handshake

    // Board liveness
    uint32_t last_rx_tick;
    uint32_t last_ping_tick; // last valid PING frame = our firmware is present
    uint16_t board_fw_version; // firmware version reported in the beacon (0 = unknown)
    bool link_lost;
    bool awaiting_board;

    // --- ESP flasher (added for the on-device firmware installer) ---
    // The flash worker runs off the GUI thread and posts progress/done events;
    // `flashing` blocks Back while a write is in progress (can't be aborted safely).
    FuriThread* flash_thread;
    FuriString* flash_manifest; // selected flash.txt path
    bool flash_auto_boot; // pulse DTR/RTS instead of asking for BOOT/RESET
    volatile bool flashing; // true once connected (blocks Back mid-write)
    volatile bool flash_cancel; // set on exit to stop the download-mode poll
    volatile uint8_t flash_img, flash_cnt, flash_pct;
    char flash_stage[40];
    char flash_msg[80];
    bool flash_ok;
    uint8_t flash_phase; // 0=waiting for download mode, 1=flashing, 2=done
    // Set on the done screen after a successful flash: the tick poll watches for the
    // freshly-reset board's PING beacon and continues on its own, so the user doesn't
    // have to tap RESET and then also confirm. Continue stays as the manual fallback.
    volatile bool flash_await_boot;
    // --- end ESP flasher ---

    volatile bool closing;
} HotspotArcadeApp;

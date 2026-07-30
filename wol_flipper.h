#pragma once

#include <furi.h>
#include <stdio.h>
#include <string.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include <dialogs/dialogs.h>

#include "wol_config.h"
#include "wol_esp.h"
#include "wol_flasher.h"
#include "wol_scene.h"

#define WOL_TEXT_BUF_LEN WOL_PASS_LEN

#define WOL_APP_DIR    EXT_PATH("apps_data/wol_flipper")
#define WOL_BACKUP_DIR WOL_APP_DIR "/backup"

/* Firmware images ship inside the .fap and land in /assets. A copy dropped
 * into /data overrides them, which is how a locally built firmware gets tested
 * without rebuilding the fap. */
#define WOL_FW_DIR         WOL_APP_DIR "/fw"
#define WOL_FW_DATA_PATH   WOL_FW_DIR "/%s"
#define WOL_FW_ASSETS_PATH APP_ASSETS_PATH("%s")

/* Offsets the ESP32-S2 bootloader expects; see esp32/partitions.csv. */
#define WOL_FW_BOOTLOADER_ADDR 0x1000
#define WOL_FW_PARTITIONS_ADDR 0x8000
#define WOL_FW_APP_ADDR        0x10000

typedef enum {
    WolViewSubmenu,
    WolViewTextInput,
    WolViewByteInput,
    WolViewPopup,
    WolViewWidget,
} WolView;

/** What the send scene was entered to do. */
typedef enum {
    WolWakeOpSend,
    WolWakeOpWifiTest,
    /** Identify the running firmware. Touches no Wi-Fi, so it isolates a dead
     *  board from bad credentials. */
    WolWakeOpPing,
    /** List visible APs, which turns "not found" into something checkable. */
    WolWakeOpScan,
} WolWakeOp;

/** Which field the shared text input scene is currently editing. */
typedef enum {
    WolTextFieldName,
    WolTextFieldIp,
    WolTextFieldSsid,
    WolTextFieldPassword,
} WolTextField;

/** Progress reported by the send worker. */
typedef enum {
    WolSendStepPower,
    WolSendStepSync,
    WolSendStepWifi,
    WolSendStepPacket,
    WolSendStepDone,
    WolSendStepErrBoard,
    WolSendStepErrFirmware,
    WolSendStepErrPower,
    WolSendStepErrReboot,
    WolSendStepErrWifi,
    WolSendStepErrWifiNotFound,
    WolSendStepErrWifiAuth,
    WolSendStepErrScan,
    WolSendStepErrSend,
    WolSendStepCount,
} WolSendStep;

#define WOL_EVENT_SEND_BASE  0x100
#define WOL_EVENT_SEND(step) (WOL_EVENT_SEND_BASE + (uint32_t)(step))

/** What the flasher scene was entered to do. */
typedef enum {
    WolFlasherOpInfo,
    WolFlasherOpFlashFirmware,
    WolFlasherOpBackup,
    WolFlasherOpRestore,
} WolFlasherOp;

/* Progress carries stage and percent in one word so nothing is shared across
 * threads: 0x1000 + (stage << 8) + percent, and 0x2000 + result when done. */
#define WOL_EVENT_FLASH_BASE 0x1000
#define WOL_EVENT_FLASH(stage, percent) \
    (WOL_EVENT_FLASH_BASE + ((uint32_t)(stage) << 8) + (uint32_t)(percent))
#define WOL_EVENT_FLASH_DONE_BASE    0x2000
#define WOL_EVENT_FLASH_DONE(result) (WOL_EVENT_FLASH_DONE_BASE + (uint32_t)(result))

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    TextInput* text_input;
    ByteInput* byte_input;
    Popup* popup;
    Widget* widget;

    WolConfig config;

    /** Scratch copy of the target being edited/created. */
    WolTarget edit;
    /** Index into config.targets of the target being edited or woken. */
    uint8_t target_index;
    bool edit_is_new;

    /** Target list scene behaviour: pick and wake vs pick and edit. */
    bool list_mode_wake;

    WolTextField text_field;
    char text_buf[WOL_TEXT_BUF_LEN];

    WolEspAp scan_list[WOL_SSID_MAX_SCAN];
    uint8_t scan_count;

    FuriThread* worker;
    volatile bool worker_cancel;
    WolWakeOp wake_op;
    /** 5V on pin 1 is held for the whole session, see wol_app_alloc(). */
    bool otg_by_us;

    DialogsApp* dialogs;
    WolFlasherOp flasher_op;
    /** Backup being restored, or the dump file being created. */
    FuriString* flasher_path;
    /** Written by a worker before it posts its done event, read after. */
    char worker_info[96];
    /** Owned by the GUI thread; the popup keeps a pointer to it. */
    char status_text[512];
    /** Raw board output from the last failed command, for the error screen. */
    char raw_reply[256];
} WolApp;

/** Bring 5V back if the boost tripped. Call from a worker before talking. */
void wol_app_ensure_power(WolApp* app);

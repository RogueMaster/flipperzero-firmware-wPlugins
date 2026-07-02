#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_ccid.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/view.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/buffered_file_stream.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---------------------------------------------------------------------------
 * Constants
 * --------------------------------------------------------------------------- */

#define CCID_EMU_MAX_ATR_LEN     33
#define CCID_EMU_MAX_RULES       24 /* was 64; 24 × ~100 B = 2.4 KB    */
#define CCID_EMU_MAX_APDU_LEN    32 /* was 64; covers most smart-card   */
#define CCID_EMU_MAX_NAME_LEN    64
#define CCID_EMU_MAX_DESC_LEN    128
#define CCID_EMU_MAX_HEX_STR     (CCID_EMU_MAX_APDU_LEN * 3)
#define CCID_EMU_LOG_MAX_ENTRIES 10 /* was 20; ring buffer for monitor  */
#define CCID_EMU_CARDS_DIR       EXT_PATH("ccid_emulator/cards")
#define CCID_EMU_LOGS_DIR        EXT_PATH("ccid_emulator/logs")
#define CCID_EMU_SAMPLE_FILE     EXT_PATH("ccid_emulator/cards/test_card.ccid")

/* ---------------------------------------------------------------------------
 * View IDs
 * --------------------------------------------------------------------------- */

typedef enum {
    CcidEmulatorViewCardBrowser,
    CcidEmulatorViewCardInfo,
    CcidEmulatorViewApduMonitor,
    CcidEmulatorViewSettings,
    CcidEmulatorViewCount,
} CcidEmulatorViewId;

/* ---------------------------------------------------------------------------
 * Custom events
 * --------------------------------------------------------------------------- */

typedef enum {
    CcidEmulatorEventCardSelected,
    CcidEmulatorEventActivateCard,
    CcidEmulatorEventStopEmulation,
    CcidEmulatorEventApduExchange,
    CcidEmulatorEventExportLog, /* right-press in APDU monitor -> save to SD */
} CcidEmulatorEvent;

/* ---------------------------------------------------------------------------
 * CCID Rule  -- one command pattern / response pair
 * --------------------------------------------------------------------------- */

typedef struct {
    uint8_t command[CCID_EMU_MAX_APDU_LEN]; /* expected command bytes          */
    uint8_t mask[CCID_EMU_MAX_APDU_LEN]; /* 0xFF = exact, 0x00 = wildcard   */
    uint16_t command_len;

    uint8_t response[CCID_EMU_MAX_APDU_LEN];
    uint16_t response_len;
} CcidRule;

/* ---------------------------------------------------------------------------
 * CcidCard  -- everything parsed from a .ccid file
 * --------------------------------------------------------------------------- */

typedef struct {
    char name[CCID_EMU_MAX_NAME_LEN];
    char description[CCID_EMU_MAX_DESC_LEN];

    uint8_t atr[CCID_EMU_MAX_ATR_LEN];
    uint8_t atr_len;

    CcidRule rules[CCID_EMU_MAX_RULES];
    uint16_t rule_count;

    uint8_t default_response[CCID_EMU_MAX_APDU_LEN];
    uint16_t default_response_len;
} CcidCard;

/* ---------------------------------------------------------------------------
 * APDU log entry
 * --------------------------------------------------------------------------- */

typedef struct {
    uint32_t timestamp; /* furi_get_tick()                */
    char command_hex[CCID_EMU_MAX_HEX_STR]; /* "00 A4 04 00 ..."             */
    char response_hex[CCID_EMU_MAX_HEX_STR]; /* "6F 19 ... 90 00"             */
    bool matched; /* true if a rule was matched     */
} CcidApduLogEntry;

/* ---------------------------------------------------------------------------
 * USB VID/PID preset
 * --------------------------------------------------------------------------- */

typedef struct {
    const char* label;
    uint16_t vid;
    uint16_t pid;
} CcidUsbPreset;

/* ---------------------------------------------------------------------------
 * Main application state
 * --------------------------------------------------------------------------- */

typedef struct {
    /* GUI plumbing */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* card_browser;
    Widget* card_info;
    View* apdu_monitor;
    VariableItemList* settings;

    /* Storage */
    Storage* storage;

    /* Card data */
    CcidCard* card; /* currently loaded card (heap)          */
    bool emulating; /* true while USB CCID is active         */
    FuriHalUsbInterface* prev_usb_if; /* previous USB interface to restore     */

    /* Discovered .ccid file paths (heap allocated strings) */
    char** card_paths;
    uint16_t card_path_count;

    /* APDU log ring buffer */
    CcidApduLogEntry log_entries[CCID_EMU_LOG_MAX_ENTRIES];
    uint32_t log_count; /* total entries written (may wrap)      */
    FuriMutex* log_mutex;
    volatile bool log_dirty; /* set by USB callback, polled by timer  */

    /* Refresh timer for APDU monitor — polls log_dirty every 200 ms */
    FuriTimer* apdu_refresh_timer;

    /* Settings */
    uint8_t usb_preset_index;

    /* CCID callbacks struct (static lifetime while emulating) */
    CcidCallbacks ccid_callbacks;
} CcidEmulatorApp;

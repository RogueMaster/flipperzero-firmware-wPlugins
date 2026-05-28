#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bt.h>
#include <extra_beacon.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>

// ── Beacon definitions ────────────────────────────────────────────────────────
// BLE manufacturer specific advertisement format:
//   Byte 0:   Length (total bytes following)
//   Byte 1:   AD Type 0xFF (manufacturer specific)
//   Byte 2-3: Manufacturer ID 0x0183 (little-endian: 0x83, 0x01)
//   Byte 4-9: Location payload
//
// Disney manufacturer ID: 0x0183
// Location payloads from community research (reddit.com/r/GalaxysEdge)

typedef struct {
    const char* name;
    const char* description;
    uint8_t     payload[6]; // the 6 location bytes after manufacturer ID
} DroidBeacon;

static const DroidBeacon DROID_BEACONS[] = {
    {
        "Market",
        "Black Spire Outpost market",
        { 0x0A, 0x04, 0x01, 0x02, 0xA6, 0x01 },
    },
    {
        "Droid Depot",
        "Build your own droid",
        { 0x0A, 0x04, 0x02, 0x02, 0xA6, 0x01 },
    },
    {
        "Resistance",
        "Resistance base area",
        { 0x0A, 0x04, 0x03, 0x02, 0xA6, 0x01 },
    },
    {
        "Rides",
        "Rise of Resistance / Falcon",
        { 0x0A, 0x04, 0x04, 0x02, 0xA6, 0x01 },
    },
    {
        "Cantina",
        "Oga's Cantina",
        { 0x0A, 0x04, 0x05, 0x02, 0xA6, 0x01 },
    },
    {
        "Dok Ondar / Savi's",
        "Ancient relics & lightsabers",
        { 0x0A, 0x04, 0x06, 0x02, 0xA6, 0x01 },
    },
    {
        "First Order",
        "First Order territory",
        { 0x0A, 0x04, 0x07, 0x02, 0xA6, 0x01 },
    },
};

#define DROID_BEACON_COUNT (sizeof(DROID_BEACONS) / sizeof(DROID_BEACONS[0]))

// Disney manufacturer ID 0x0183 in little-endian
#define DISNEY_MFR_ID_LO 0x83
#define DISNEY_MFR_ID_HI 0x01

// ── Scene IDs ────────────────────────────────────────────────────────────────
// @FZGEN_BEGIN scene_enum
typedef enum {
    DroidbeaconSceneSplash,
    DroidbeaconSceneMenu,
    DroidbeaconSceneBeaming,
    DroidbeaconSceneResult,
    DroidbeaconSceneCount,
} DroidbeaconScene;
// @FZGEN_END scene_enum

// ── View IDs ─────────────────────────────────────────────────────────────────
// @FZGEN_BEGIN view_enum
typedef enum {
    DroidbeaconViewSplash,
    DroidbeaconViewMenu,
    DroidbeaconViewBeaming,
    DroidbeaconViewResult,
    DroidbeaconViewCount,
} DroidbeaconView;
// @FZGEN_END view_enum

// ── Custom events ─────────────────────────────────────────────────────────────
// @FZGEN_BEGIN event_enum
typedef enum {
    DroidbeaconEventAppExit,
    DroidbeaconEventSplashNext,
    DroidbeaconEventMenuSelect,
    DroidbeaconEventBeamStart,
    DroidbeaconEventBeamStop,
    DroidbeaconEventBeamDone,
} DroidbeaconCustomEvent;
// @FZGEN_END event_enum

// ── App state ────────────────────────────────────────────────────────────────
typedef struct {
    Gui*             gui;
    ViewDispatcher*  view_dispatcher;
    SceneManager*    scene_manager;
    NotificationApp* notifications;

    // Views
    Widget*  splash_widget;
    Submenu* menu;
    Widget*  beaming_widget;
    Popup*   result_popup;

// @FZGEN_BEGIN struct_fields
    uint8_t selected_beacon;
    bool    is_beaming;
// @FZGEN_END struct_fields
} DroidbeaconApp;

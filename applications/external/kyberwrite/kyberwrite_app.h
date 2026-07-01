#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include <lib/lfrfid/lfrfid_worker.h>
#include <lib/lfrfid/protocols/lfrfid_protocols.h>
#include <lib/lfrfid/tools/em4305.h>
#include <toolbox/protocols/protocol_dict.h>

// ── Crystal color/voice data ─────────────────────────────────────────────────
typedef struct {
    const char* name;
    uint32_t    addr6;
} KyberCrystal;

static const KyberCrystal KYBER_CRYSTALS[] = {
    { "White  (Palpatine/Ahsoka)",     0x18003000 },
    { "Red    (Vader/Yoda)",           0x5E003000 },
    { "Orange",                        0x3D003000 },
    { "Yellow (Palpatine/Guard)",      0x7B003000 },
    { "Green  (Palpatine/Qui-Gon)",    0x0C803000 },
    { "Cyan",                          0x4A803000 },
    { "Blue   (Palpatine/Obi-Wan)",    0x29803000 },
    { "Purple (Palpatine/Mace 1)",     0x6F803000 },
    { "White  (Palpatine/Chirrut)",    0x14403000 },
    { "Red    (Palpatine/Yoda)",       0x52403000 },
    { "Red    (Dooku/Yoda)",           0x31403000 },
    { "Yellow (Palpatine/Maz)",        0x77403000 },
    { "Green  (Palpatine/Yoda)",       0x00C03000 },
    { "Red    (Maul/Yoda)",            0x46C03000 },
    { "Blue   (Palpatine/Old Luke)",   0x25C03000 },
    { "Purple (Palpatine/Mace 2)",     0x63C03000 },
    { "Red    (Vader 8-Ball/Yoda)",    0x3E183000 },
    { "Green  (Palpatine/Yoda 8-B)",   0x5D183000 },
    { "Black  (Snoke/Yoda)",           0x1B183000 },
};

#define KYBER_CRYSTAL_COUNT (sizeof(KYBER_CRYSTALS) / sizeof(KYBER_CRYSTALS[0]))

static const uint32_t KYBER_BASE_DATA[16] = {
    0x00040072, 0x6147FBB3, 0x00000000, 0x000064FC,
    0x0001805F, 0x000001FF, 0x0C803000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00008002, 0x00000000,
};

static const uint8_t KYBER_WRITE_ORDER[] = { 0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 1 };
#define KYBER_WRITE_STEPS (sizeof(KYBER_WRITE_ORDER) / sizeof(KYBER_WRITE_ORDER[0]))

// ── Scene IDs ────────────────────────────────────────────────────────────────
typedef enum {
    KyberSceneSplash,
    KyberSceneHint,
    KyberSceneMenu,
    KyberSceneColorSelect,
    KyberSceneWriting,
    KyberSceneResult,
    KyberSceneRead,
    KyberSceneCount,
} KyberScene;

// ── View IDs ─────────────────────────────────────────────────────────────────
typedef enum {
    KyberViewSplash,
    KyberViewHint,
    KyberViewMenu,
    KyberViewColorSelect,
    KyberViewWriting,
    KyberViewResult,
    KyberViewRead,
    KyberViewCount,
} KyberView;

// ── Write mode ───────────────────────────────────────────────────────────────
typedef enum {
    KyberModeFullInit,
    KyberModeQuickChange,
} KyberMode;

// ── App state ────────────────────────────────────────────────────────────────
typedef struct {
    Gui*             gui;
    ViewDispatcher*  view_dispatcher;
    SceneManager*    scene_manager;
    NotificationApp* notifications;

    // Views
    Submenu* menu;
    Submenu* color_menu;
    Widget*  writing_widget;
    Popup*   result_popup;
    Widget*  read_widget;
    Widget*  splash_widget;
    Widget*  hint_widget;

    // RFID
    LFRFIDWorker*  rfid_worker;
    ProtocolDict*  protocol_dict;

    // State
    KyberMode mode;
    uint8_t   crystal_index;
    bool      write_success;
    bool      show_hints;
    char*     hint_title;
    uint8_t   hint_text_lines;
    const char**    hint_text;
    KyberScene hint_next_scene;
} KyberApp;

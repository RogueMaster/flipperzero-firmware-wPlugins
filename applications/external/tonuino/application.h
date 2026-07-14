#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>
#include <gui/modules/number_input.h>
#include <notification/notification_messages.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <nfc/nfc.h>
#include <lib/nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <lib/nfc/protocols/mf_classic/mf_classic.h>

#define TONUINO_CARD_SIZE 16
#define TONUINO_BOX_ID_0  0x13
#define TONUINO_BOX_ID_1  0x37
#define TONUINO_BOX_ID_2  0xB3
#define TONUINO_BOX_ID_3  0x47
#define TONUINO_VERSION   0x02

#define APP_VERSION      "2.0.0"
#define APP_BUILD_NUMBER 82

typedef enum {
    ModeHoerspiel = 1,
    ModeAlbum = 2,
    ModeParty = 3,
    ModeEinzel = 4,
    ModeHoerbuch = 5,
    ModeAdmin = 6,
    ModeHoerspielVB = 7,
    ModeAlbumVB = 8,
    ModePartyVB = 9,
    ModeHoerbuch1 = 10,
    ModeRepeatLast = 11,
} TonuinoMode;

typedef struct {
    uint8_t box_id[4];
    uint8_t version;
    uint8_t folder;
    uint8_t mode;
    uint8_t special1;
    uint8_t special2;
    uint8_t reserved[7];
} TonuinoCard;

typedef enum {
    TonuinoViewSubmenu,
    TonuinoViewNumberInput,
    TonuinoViewWidget,
    TonuinoViewTextBox,
    TonuinoViewCount,
} TonuinoViewId;

typedef struct {
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    NumberInput* number_input;
    Widget* widget; // CRITICAL: All scenes using Widget MUST call widget_reset() in on_enter
    TextBox* text_box;
    NotificationApp* notifications;

    TonuinoCard card_data;
    char text_buffer[32];
    char read_text_buffer[512];
} TonuinoApp;

TonuinoApp* tonuino_app_alloc();
void tonuino_app_free(TonuinoApp* app);
void tonuino_build_card_data(TonuinoApp* app);

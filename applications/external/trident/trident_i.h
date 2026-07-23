#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "trident_icons.h" // generated from icons/ by fbt

#include "helpers/marauder.h"
#include "helpers/marauder_uart.h"
#include "helpers/nrf24_radio.h"
#include "helpers/subghz_radio.h"
#include "helpers/trident_storage.h"
#include "views/console_view.h"
#include "views/spectrum_view.h"
#include "views/meter_view.h"
#include "scenes/trident_scene.h"

#define TRIDENT_VERSION   "1.1"
#define TRIDENT_CMD_MAX   64
#define TRIDENT_TITLE_MAX 24
#define TRIDENT_LINK_TIMEOUT_MS \
    1500u // no serial byte for this long -> ESP32 link shown as idle, not live

typedef enum {
    TridentViewSubmenu, // home + every sub-menu
    TridentViewConsole, // live ESP32 serial console + NRF24 sniffer log
    TridentViewSpectrum, // NRF24 / CC1101 band analyzer
    TridentViewMeter, // NRF24 / CC1101 finder gauge
    TridentViewVarList, // settings
    TridentViewTextInput, // send raw command / enter a value
    TridentViewWidget, // about + attack confirmation
} TridentViewId;

typedef enum {
    // submenu item ids are sent verbatim as custom events, so keep app events high
    TridentCustomEventConfirmStart = 1000,
    TridentCustomEventConsoleSend,
    TridentCustomEventSelectApplied,
    TridentCustomEventInputDone,
} TridentCustomEvent;

typedef enum {
    TridentUartUsart = 0, // GPIO 13 TX / 14 RX  (standard ESP32 wiring)
    TridentUartLpuart = 1, // GPIO 15 TX / 16 RX  (alternate wiring)
} TridentUartChannel;

typedef enum {
    TridentCc1101Internal = 0, // Flipper's own CC1101
    TridentCc1101External = 1, // the board's CC1101 (needs cc1101_ext driver)
} TridentCc1101Device;

typedef struct TridentSettings {
    uint8_t uart_channel; // TridentUartChannel
    uint8_t cc1101_device; // TridentCc1101Device
    uint8_t subghz_band; // index into trident_subghz_bands[]
    bool autoscroll; // console follows newest output
    bool confirm_attacks; // gate ESP32 deauth/beacon/probe behind a prompt
    bool sound;
    bool vibro;
    bool led;
} TridentSettings;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    // shared GUI modules
    Submenu* submenu;
    VariableItemList* var_item_list;
    TextInput* text_input;
    Widget* widget;

    // custom views
    ConsoleView* console_view; // ESP32 serial console + NRF24 sniffer log
    SpectrumView* spectrum_view; // NRF24 + CC1101 band analyzer
    MeterView* meter_view; // NRF24 + CC1101 finder gauge

    // the three radios
    MarauderUart* uart; // ESP32 link
    Nrf24Radio* nrf24; // 2.4 GHz analyzer
    SubghzRadio* subghz; // Sub-GHz analyzer

    TridentSettings settings;

    // command staged for the console scene to run on entry
    char pending_cmd[TRIDENT_CMD_MAX];
    char pending_title[TRIDENT_TITLE_MAX];
    bool pending_is_attack;

    // text-input scratch (raw command sender / target index)
    char input_buf[TRIDENT_CMD_MAX];
    char select_kind; // 'a' = AP, 's' = station (for the Select scene)

    // generic input-prompt scene: build "<prefix><typed value>", then optionally
    // drop into the console running <after_cmd>
    char input_prefix[24];
    char input_header[32];
    char input_after_cmd[24];
    char input_after_title[TRIDENT_TITLE_MAX];

    // when true, leaving the console scene must NOT stop the running op
    bool console_keep_running;

    volatile uint32_t last_rx_tick; // last byte received from the ESP32
} TridentApp;

/* ---- settings.c helpers ---- */
const char* trident_uart_channel_label(uint8_t index);

/* ---- ESP32 link (trident.c) ---- */
// Stage a command and route to the console (through the confirm gate for attacks).
void trident_launch(TridentApp* app, const char* title, const char* cmd, bool is_attack);
// Open the input-prompt scene: on commit, send "<prefix><value>"; if after_cmd is
// non-empty, then open the console running after_cmd (titled after_title).
void trident_prompt(
    TridentApp* app,
    const char* header,
    const char* prefix,
    const char* after_title,
    const char* after_cmd);
void trident_link_ensure(TridentApp* app); // acquire UART + worker if not already up
void trident_link_disarm(TridentApp* app); // stop the current op and release the UART
void trident_link_send(TridentApp* app, const char* cmd); // raw send (adds nothing)
void trident_notify_start(TridentApp* app); // short feedback when an attack starts
bool trident_link_is_live(TridentApp* app); // received data recently?
void trident_click(TridentApp* app); // short Geiger-style click (finder feedback)

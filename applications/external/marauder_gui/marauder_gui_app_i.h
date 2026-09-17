#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/widget.h>
#include <gui/modules/number_input.h>
#include "marauder_text_input.h"
#include <storage/storage.h>

#include "marauder_uart.h"
#include "file/sequential_file.h"
#include "marauder_gui_pcap_index.h"
#include "scenes/marauder_gui_scene.h"

#define MARAUDER_AP_LIST_MAX          32
#define MARAUDER_LINE_MAX             96
/* Max distinct SSIDs the pcap-filter SSID picker can track at once - capped at the same size as
   MARAUDER_AP_LIST_MAX since that list (unique SSIDs seen in the file) is built the same way. */
#define MARAUDER_PCAP_SSID_FILTER_MAX MARAUDER_AP_LIST_MAX

/* Where PCAP captures land on the Flipper's SD card. Both dirs are created (if missing) right
   before the first capture file is opened - see marauder_gui_scene_pcap_sniff.c. */
#define MARAUDER_GUI_DATA_DIR EXT_PATH("apps_data/marauder_gui")
#define MARAUDER_GUI_PCAP_DIR MARAUDER_GUI_DATA_DIR "/pcaps"

typedef struct MarauderGuiApp MarauderGuiApp;

/* Set by whichever scene currently wants to consume incoming UART lines (NULL otherwise) */
typedef void (*MarauderUartLineHandler)(MarauderGuiApp* app, const char* line);

/* Set by whichever scene wants a callback on every ~100ms app tick (NULL otherwise) */
typedef void (*MarauderTickHandler)(MarauderGuiApp* app);

/* Set by a frozen scan/detector scene to re-issue whatever command(s) its on_enter originally
   sent (NULL otherwise) - called when Ok is pressed while frozen, so the user can resume a
   stopped scan without backing all the way out and re-entering the scene. Must NOT reset
   already-collected results (ap_list/terminal_log etc.) - only (re)send the start command(s). */
typedef void (*MarauderResumeHandler)(MarauderGuiApp* app);

/* Custom event id fired by the shared log-scan "Devam Et" button (see marauder_gui_log_scan.c) -
   a fixed, arbitrary value far outside any scene's own small sequential custom-event enum, so
   there's no risk of collision on the Widget-based scenes that use it (none of them define any
   other custom event of their own). */
#define MARAUDER_RESUME_CUSTOM_EVENT 0x5245534du /* 'RESM' */

/* Custom event id fired by the shared attack-view's Ok handling (see
   marauder_gui_attack_view.c) - every scene using MarauderGuiViewAttackStatus checks for this
   same value in its on_event instead of defining its own "stop" event. */
#define MARAUDER_ATTACK_STOP_CUSTOM_EVENT 0x53544f50u /* 'STOP' */

/* Custom event id fired by the shared WifiList view's Right-key handling (see
   marauder_gui_wifi_list_input_callback in wifi_scanning.c) - only wifi_select_aps.c enables
   this (gated by wifi_list_show_selected_count), to move from "pick which APs" to "pick which
   attack to run against all of them". Value sits far outside MARAUDER_AP_LIST_MAX so it can
   never collide with a real row-index custom event. */
#define MARAUDER_WIFI_LIST_PROCEED_CUSTOM_EVENT 0x50524f43u /* 'PROC' */

/* Which animation marauder_gui_attack_view.c draws in place of a raw status line - purely
   decorative "something is happening" feedback, not real RF data (Marauder's serial output has
   no per-packet signal figures for a running attack/spam). Spectrum suits a single steady
   stream (a WiFi attack targeting one AP); Radar (a rotating sweep line over a couple of device
   "blips" that flash as the sweep passes them) suits BLE spam's device-discovery/broadcast feel. */
typedef enum {
    MarauderAttackViewStyleSpectrum,
    MarauderAttackViewStyleRadar,
} MarauderAttackViewStyle;

typedef enum {
    MarauderGuiViewWidget,
    MarauderGuiViewWifiList,
    MarauderGuiViewNumberInput,
    MarauderGuiViewTextInput,
    MarauderGuiViewMenu,
    MarauderGuiViewAttackStatus,
    MarauderGuiViewPcapTable,
} MarauderGuiView;

/* Persisted in APP_DATA_PATH("language.bin") (see marauder_gui_save_language()/ the read in
   marauder_gui_app_alloc()) - every user-facing string in the app is picked at draw time based
   on this, via marauder_gui_text() below. */
typedef enum {
    MarauderLanguageTurkish,
    MarauderLanguageEnglish,
} MarauderLanguage;

/* A menu item with a description reachable via the Right arrow key - see marauder_gui_menu.c.
   Every item carries both languages so menus don't need a second static array per language -
   marauder_gui_menu.c picks the right pair at draw time from app->language. */
typedef struct {
    const char* label_tr;
    const char* label_en;
    const char* description_tr;
    const char* description_en;
} MarauderMenuItem;

struct MarauderGuiApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Widget* widget;
    View* wifi_list_view;
    NumberInput* number_input;
    MarauderTextInput* text_input;
    View* menu_view;
    View* attack_status_view;
    View* pcap_table_view;

    MarauderLanguage language;

    MarauderUart* uart;
    MarauderUartLineHandler uart_line_handler;
    MarauderTickHandler tick_handler;
    MarauderResumeHandler resume_handler;

    /* PCAP capture to the Flipper SD card (see marauder_gui_scene_pcap_sniff.c). The raw pcap
       byte stream Marauder sends over serial is drained every app tick (marauder_gui.c) and, when
       is_writing_pcap is set, written straight to capture_file. */
    Storage* storage;
    File* capture_file;
    bool is_writing_pcap;
    uint32_t pcap_bytes;
    /* Full command for the active capture, flags included but WITHOUT "-serial" (the capture
       scene appends that) - e.g. "sniffraw", "sniffpmkid -d -l", "sniffpmkid -d -c 6". May point
       at pcap_cmd_buf for commands built at runtime. */
    const char* pcap_sniff_cmd;
    /* Bare command name used as the .pcap filename prefix ("sniffpmkid"), kept separate from
       pcap_sniff_cmd so flags/spaces never end up in a filename. */
    const char* pcap_sniff_prefix;
    const char* pcap_sniff_label; /* header shown on the capture screen */
    char pcap_save_name[64]; /* basename of the file being written, for display */
    char pcap_cmd_buf[48]; /* holds runtime-built commands (the "on channel N" PMKID modes) */
    /* Command up to (and including) "-c" for the PMKID "on channel" modes; the channel picker
       rebuilds pcap_cmd_buf from this each time, so re-entering it can't append twice. */
    const char* pcap_pmkid_base;
    int pcap_pmkid_channel; /* last channel picked for the PMKID "on channel" modes */
    /* True for the "targeted" PMKID modes, which run on Marauder's selected-AP list ("-l"): the
       flow scans + selects an AP first, then pcap_sniff.c issues "select -a <idx>" before the
       sniff and toggles it back off on exit. False for every broadcast capture, which starts
       straight away. Selected AP is app->selected_ap_index / app->ap_list, as in the attack flow.
       Note "sniffdeauth" ignores AP selection and channel entirely (it is a passive sniffer -
       see CommandLine.cpp's SNIFF_DEAUTH_CMD handler), so it is never AP-scoped. */
    bool pcap_ap_scoped;

    char rx_line_buf[MARAUDER_LINE_MAX];
    size_t rx_line_len;
    char last_uart_line[MARAUDER_LINE_MAX];

    /* Shared "live scan result list" state - used by both wifi_scanning (AP list, via
       "scanall"/"list -a") and bt_tracker_scan (AirTag/tracker list, via "sniffbt"/"list -t").
       Only one of those scenes is ever active at a time, so sharing is safe. */
    char ap_list[MARAUDER_AP_LIST_MAX][MARAUDER_LINE_MAX];
    size_t ap_count;
    uint32_t wifi_scan_refresh_tick;
    bool wifi_scan_frozen;
    bool wifi_scan_keep_alive;
    size_t wifi_list_selected;
    size_t wifi_list_scroll_offset;
    size_t wifi_list_marquee_tick;
    size_t wifi_list_marquee_hold;
    uint32_t wifi_list_marquee_delay;
    const char* wifi_list_scanning_label;
    const char* wifi_list_empty_label;
    /* Overrides the generic "Stopped (Back:Menu)" frozen-state header text when non-NULL - only
       wifi_select_aps.c sets this (to mention the Right-key "pick attack" action), NULL again in
       its on_exit so no other WifiList consumer ever sees it. */
    const char* wifi_list_frozen_label;
    const char* list_scan_refresh_command;
    const char* list_scan_start_command;
    /* Cached by marauder_gui_log_scan_redraw() on every call, purely so the freeze/resume
       handlers can redraw immediately with the right header text without every consuming scene
       having to store its own title separately. */
    const char* log_scan_title;

    int selected_ap_index;
    bool attack_active;

    size_t bt_spam_type_index;

    int selected_tracker_index;
    int bt_tracker_action; /* 0 = findmy (play sound), 1 = spoofat (spoof) */

    size_t wifi_spam_type_index;
    bool wifi_spam_use_count;
    int wifi_spam_count;
    char wifi_spam_status_label[32];

    int selected_probe_index;
    int karma_state;
    uint32_t karma_wait_ticks;

    char terminal_cmd[64];
    char terminal_log[600];
    size_t terminal_log_len;

    char pending_pwn_name[32];

    int wifi_ap_attack_type; /* 0 = deauth, 1 = probe flood, 2 = AP clone spam, 3 = targeted deauth, 9 = join WiFi (password step next, not an attack), 10 = view AP info, 11 = clone STA MAC (station step next, not an attack), 12 = clone AP MAC, 13 = packet count, 14 = AP fox hunt, 15 = station fox hunt (station step next, not an attack) */

    bool wifi_station_capturing;
    int station_global_index[MARAUDER_AP_LIST_MAX];
    int selected_station_index;
    char selected_station_label[MARAUDER_LINE_MAX];

    /* true = "randapmac", false = "randstamac" - see wifi_random_mac.c */
    bool wifi_mac_random_is_ap;

    /* Opt-in for the shared WifiList view's draw callback: when true, draws a small selected-AP
       count badge in the top-right corner (counting "[x] " markers in ap_list). Only
       wifi_select_aps.c ever sets this - true in its on_enter, false again in its on_exit so it
       never leaks into any of the other scenes that reuse this same view. */
    bool wifi_list_show_selected_count;

    char wifi_join_password[64];
    bool wifi_join_use_saved;
    bool wifi_join_failed;
    uint32_t wifi_join_wait_ticks;
    bool wifi_join_done;

    /* Marauder has no CLI command that reports live connection/SSID status, so this is our own
       best-known state, set once a join attempt finishes without an explicit failure message
       (see wifi_join_status.c) - an inference, not a guarantee the link is still up later. */
    bool wifi_is_connected;
    char wifi_connected_label[MARAUDER_LINE_MAX];
    char wifi_pending_label[MARAUDER_LINE_MAX];
    /* True while parsing a "settings" dump waiting for the "Value: " line right after
       "Name: ClientSSID" - used only by the saved-join flow to learn the real SSID text
       (see wifi_join_status.c). */
    bool wifi_settings_awaiting_ssid;

    int selected_ip_index;

    /* Generic menu view state - see marauder_gui_menu.c. Whichever scene is showing a menu
       points menu_items at its own static table via marauder_gui_menu_set_items(); only one
       menu is ever on screen at a time so sharing these fields is safe. */
    const MarauderMenuItem* menu_items;
    size_t menu_item_count;
    const char* menu_header;
    size_t menu_selected;
    size_t menu_scroll_offset;
    bool menu_showing_description;
    size_t menu_description_scroll;
    size_t menu_marquee_tick;
    size_t menu_marquee_hold;
    uint32_t menu_marquee_delay;

    /* Shared attack/spam status view (see marauder_gui_attack_view.c) - whichever scene is
       showing it sets title/target/style once in its on_enter (constant for that screen's whole
       lifetime, so the draw callback doesn't need to recompute them) and increments
       attack_spectrum_phase once per tick to animate it; only one such scene is ever on screen
       at a time, so sharing these fields is safe. */
    const char* attack_view_title;
    const char* attack_view_target;
    char attack_view_target_buf[32];
    MarauderAttackViewStyle attack_view_style;
    uint32_t attack_spectrum_phase;

    /* True when wifi_attack.c was entered from wifi_select_aps_attack_menu.c instead of the
       normal single-AP wifi_attack_menu->wifi_scanning path - see marauder_gui_scene_wifi_attack.c
       for what this changes (no select -a/channel -s prep, target label is a selected-count
       instead of one AP/station, no deselect-on-exit). Always reset to false in wifi_attack.c's
       on_exit so it never leaks into a later single-AP attack. */
    bool wifi_attack_multi_ap;

    /* Saved-.pcap browsing/filtering (see marauder_gui_pcap_index.c and the pcap_file_list/
       pcap_view/pcap_filter scenes). pcap_view_filename is set by the file-picker scene (which
       reuses ap_list/wifi_list like every other "pick from a list" scene); pcap_index is built
       once from it on entering the viewer and reused across filter changes (filtering just
       re-walks the already-parsed index, no re-reading the file). */
    char pcap_view_filename[64];
    /* Which file pcap_index currently holds - compared against pcap_view_filename in on_enter so
       returning from the filter/SSID sub-screens (same file) skips re-parsing and keeps the
       filter the user just set, while actually picking a different file (via pcap_file_list)
       triggers a fresh parse + resets the filter to "show everything". */
    char pcap_index_loaded_filename[64];
    MarauderPcapIndexEntry* pcap_index;
    size_t pcap_index_count;
    size_t pcap_index_capacity;
    bool pcap_index_truncated;
    size_t pcap_view_selected;
    size_t pcap_view_scroll_offset;
    size_t pcap_view_marquee_tick;
    size_t pcap_view_marquee_hold;
    uint32_t pcap_view_marquee_delay;
    /* (1 << MarauderPcapFrameType) bits; defaults to "all types" on first entry. */
    uint16_t pcap_filter_type_mask;
    /* SSID allow-list picked from the SSIDs actually seen in this file (pcap_filter_ssid.c) -
       not free-typed, so it can only ever contain strings that genuinely appear in the index. */
    char pcap_filter_ssids[MARAUDER_PCAP_SSID_FILTER_MAX][16];
    size_t pcap_filter_ssid_count;
    /* Scratch buffer for pcap_rename.c - holds the file's base name (".pcap" stripped) while the
       text input edits it; re-joined with ".pcap" only once the user confirms. Sized so
       base + ".pcap" + '\0' can never exceed pcap_view_filename's 64 bytes. */
    char pcap_rename_buffer[59];

    /* Live per-AP/type packet-count dashboard shown by pcap_sniff.c while a live capture is
       running (see marauder_gui_pcap_live_frame_callback in marauder_gui.c, and
       MarauderPcapLiveParser in marauder_gui_pcap_index.h/.c). Fed straight from the same byte
       stream that's already being written to the .pcap file - never re-reads the file. Reuses
       ap_list/MarauderGuiViewWifiList for the actual on-screen table like every other "list of
       rows" scene; pcap_live_aps is the underlying per-AP counters that ap_list's row strings get
       reformatted from whenever pcap_live_dirty says something changed. */
    MarauderPcapLiveParser pcap_live_parser;
    MarauderPcapLiveApStat pcap_live_aps[MARAUDER_AP_LIST_MAX];
    size_t pcap_live_ap_count;
    bool pcap_live_dirty;
    uint32_t pcap_live_refresh_tick;
};

/* Picks the TR or EN string based on the app's current language - the one helper every scene
   uses for any text that isn't a static MarauderMenuItem (dynamic headers, status lines, button
   labels, ...). Declared static inline so it doesn't need its own translation unit. */
static inline const char*
    marauder_gui_text(const MarauderGuiApp* app, const char* tr, const char* en) {
    return (app->language == MarauderLanguageEnglish) ? en : tr;
}

static inline const char*
    marauder_gui_menu_item_label(const MarauderGuiApp* app, const MarauderMenuItem* item) {
    return marauder_gui_text(app, item->label_tr, item->label_en);
}

static inline const char*
    marauder_gui_menu_item_description(const MarauderGuiApp* app, const MarauderMenuItem* item) {
    return marauder_gui_text(app, item->description_tr, item->description_en);
}

/* Implemented in marauder_gui.c - reads/writes APP_DATA_PATH("language.bin") (one byte). Load is
   called once from marauder_gui_app_alloc(); Save is called by the Language picker scene. */
void marauder_gui_load_language(MarauderGuiApp* app);
void marauder_gui_save_language(MarauderGuiApp* app);

/* Implemented in scenes/marauder_gui_scene_wifi_scanning.c - a small custom View (list with a
   marquee-scrolling long-name highlight) used only by that scene. */
View* marauder_gui_wifi_list_view_alloc(MarauderGuiApp* app);
void marauder_gui_wifi_list_redraw(MarauderGuiApp* app);

/* Implemented in scenes/marauder_gui_scene_pcap_view.c - a small custom View (filterable,
   scrollable table over app->pcap_index) used only by that scene, following the same pattern as
   marauder_gui_wifi_list_view_alloc() above. */
View* marauder_gui_pcap_table_view_alloc(MarauderGuiApp* app);
void marauder_gui_pcap_table_redraw(MarauderGuiApp* app);

/* Implemented in marauder_gui_attack_view.c - a custom View shared by every attack/spam status
   scene (wifi_attack.c, bt_spam.c) that replaces Marauder's raw confirmation line with an
   animated graphic (Widget has no hook for freeform canvas drawing, hence a dedicated View
   here). Each consuming scene sets app->attack_view_title/target/style in its on_enter, then
   its own tick_handler increments attack_spectrum_phase and calls the redraw below; Ok is
   handled here and fires MARAUDER_ATTACK_STOP_CUSTOM_EVENT for the scene's on_event to catch. */
View* marauder_gui_attack_view_alloc(MarauderGuiApp* app);
void marauder_gui_attack_view_redraw(MarauderGuiApp* app);

/* Implemented in marauder_gui_list_scan.c - shared plumbing for "pure detector" scenes: send a
   start command, periodically re-poll a "list -X"-style refresh command into the shared list
   view, no follow-up action on selecting an entry (unlike wifi_scanning/bt_tracker_scan/
   wifi_probe_sniff, which each have their own bespoke version of this because they DO have a
   next step after picking an item). */
void marauder_gui_list_scan_start(
    MarauderGuiApp* app,
    const char* start_command,
    const char* refresh_command,
    const char* scanning_label,
    const char* empty_label);
bool marauder_gui_list_scan_handle_back(MarauderGuiApp* app, SceneManagerEvent event);
void marauder_gui_list_scan_exit(MarauderGuiApp* app);

/* Implemented in marauder_gui_log_scan.c - shared plumbing for detectors whose events are raw
   streamed lines rather than a "list -X"-pollable indexed list (deauth sniff, pwnagotchi,
   meta/flock/skimmer BLE detectors, mactrack's periodic table). Reuses the terminal_log buffer
   (never active at the same time as the Terminal feature) and the Widget view with a scrolling
   text element instead of the WifiList view, since there's no per-item "select" action here. */
void marauder_gui_log_scan_clear(MarauderGuiApp* app);
void marauder_gui_log_scan_append(MarauderGuiApp* app, const char* line);
void marauder_gui_log_scan_redraw(MarauderGuiApp* app, const char* title);
bool marauder_gui_log_scan_handle_back(MarauderGuiApp* app, SceneManagerEvent event);
void marauder_gui_log_scan_exit(MarauderGuiApp* app);
bool marauder_gui_log_scan_handle_back_keep_running(MarauderGuiApp* app, SceneManagerEvent event);
void marauder_gui_log_scan_exit_keep_running(MarauderGuiApp* app);

/* Implemented in scenes/marauder_gui_scene_bt_spam.c - the "blespam -t <arg>" type table,
   shared with marauder_gui_scene_bt_menu.c which lists them for picking one. */
size_t marauder_gui_bt_spam_type_count(void);
const char* marauder_gui_bt_spam_type_label(size_t index);
const char* marauder_gui_bt_spam_type_label_en(size_t index);
const char* marauder_gui_bt_spam_type_arg(size_t index);
const char* marauder_gui_bt_spam_type_description(size_t index);
const char* marauder_gui_bt_spam_type_description_en(size_t index);

/* Implemented in scenes/marauder_gui_scene_wifi_spam.c - the "attack -t beacon -r"/etc. type
   table, shared with marauder_gui_scene_wifi_spam_menu.c which lists them for picking one. */
size_t marauder_gui_wifi_spam_type_count(void);
const char* marauder_gui_wifi_spam_type_label(size_t index);
const char* marauder_gui_wifi_spam_type_label_en(size_t index);
const char* marauder_gui_wifi_spam_type_command(size_t index);
const char* marauder_gui_wifi_spam_type_description(size_t index);
const char* marauder_gui_wifi_spam_type_description_en(size_t index);

/* Implemented in marauder_gui_menu.c - the generic "menu with Right-key descriptions" view that
   replaces plain Submenu everywhere in this app. A scene calls marauder_gui_menu_set_items()
   from its on_enter with its own static MarauderMenuItem[] table (Ok still fires a custom event
   with the selected index exactly like Submenu did, so on_event handlers are unchanged) and
   should set app->tick_handler = NULL in its on_exit. */
View* marauder_gui_menu_view_alloc(MarauderGuiApp* app);
void marauder_gui_menu_set_items(
    MarauderGuiApp* app,
    const MarauderMenuItem* items,
    size_t count,
    const char* header);
/* Redraws without touching selection/scroll state - for a scene whose item labels change in
   place after the initial set_items call (e.g. a settings list refreshing on/off text). */
void marauder_gui_menu_redraw(MarauderGuiApp* app);

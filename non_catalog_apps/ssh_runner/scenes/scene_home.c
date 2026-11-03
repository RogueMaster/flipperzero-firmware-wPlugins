/* Home, Settings, Developer, About. SPDX-License-Identifier: GPL-3.0-only */
#include "scene_util.h"
#include <furi_hal_rtc.h>

enum {
    HomeQuickConnect,
    HomeHosts,
    HomeRecent,
    HomeNetwork,
    HomeKeys,
    HomeSettings,
};

static const char* const home_items[] = {
    "Quick Connect",
    "Hosts",
    "Recent",
    "Network",
    "Keys",
    "Settings",
};

static void home_refresh_status(SshRunnerApp* app) {
    HomeStatus st;
    app_fill_home_status(app, &st);
    home_view_set_status(app->home, &st);
}

void ssh_runner_scene_home_on_enter(void* context) {
    SshRunnerApp* app = context;
    keys_unload(app); /* back at the top: the key list is scanned again when needed */
    home_view_set_items(app->home, home_items, COUNT_OF(home_items));
    home_view_set_callback(app->home, util_submenu_cb, app);
    home_view_set_selected(
        app->home, (uint8_t)scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneHome));
    home_refresh_status(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewHome);
}

bool ssh_runner_scene_home_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == EvtChanged) {
        if(app_take_changes(app) & (SESSION_CHG_NET | SESSION_CHG_LINK)) home_refresh_status(app);
        return true;
    }
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneHome, event.event);
    switch(event.event) {
    case HomeQuickConnect: {
        /* Prefill with the most recent unsaved target, selected so typing replaces it. */
        char last[96] = "";
        for(uint8_t i = 0; i < app->recent.count; i++)
            if(!app->recent.items[i].alias[0]) {
                target_format(&app->recent.items[i].target, last, sizeof(last));
                break;
            }
        util_start_input(app, InputQuickConnect, last);
        return true;
    }
    case HomeHosts:
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneHosts);
        return true;
    case HomeRecent:
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneRecent);
        return true;
    case HomeNetwork:
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneNetwork);
        return true;
    case HomeKeys:
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneKeys);
        return true;
    case HomeSettings:
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneSettings);
        return true;
    }
    return false;
}

void ssh_runner_scene_home_on_exit(void* context) {
    UNUSED(context);
}

/* ---------------- Settings ---------------- */

/* Same order as the menu: the scene state (0 on first entry) selects the item by id. */
enum {
    SettingsAbout,
    SettingsDeveloper,
    SettingsSecurity,
    SettingsExamples,
};

void ssh_runner_scene_settings_on_enter(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Settings");
    util_submenu_add(app->submenu, "About", SettingsAbout, util_submenu_cb, app);
    /* like stock and community apps: developer tools only with Settings > System > Debug */
    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug))
        util_submenu_add(app->submenu, "Developer", SettingsDeveloper, util_submenu_cb, app);
    util_submenu_add(app->submenu, "Security", SettingsSecurity, util_submenu_cb, app);
    util_submenu_add(app->submenu, "Restore Examples", SettingsExamples, util_submenu_cb, app);
    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneSettings));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool ssh_runner_scene_settings_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event >= EvtBase) return false;
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneSettings, event.event);
    if(event.event == SettingsDeveloper)
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneDeveloper);
    else if(event.event == SettingsAbout)
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneAbout);
    else if(event.event == SettingsSecurity)
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneSecurity);
    else if(event.event == SettingsExamples) {
        ExamplesResult r = store_examples(app, false);
        char* text = util_scratch; /* shown by the message scene */
        if(!r.available)
            snprintf(text, UTIL_SCRATCH_SIZE, "This build has no\nexamples.");
        else if(!r.commands && !r.scripts && !r.no_room)
            snprintf(text, UTIL_SCRATCH_SIZE, "All examples are there.\nNothing was changed.");
        else {
            /* only what was added: "Added 9 commands.", "Added 1 script.", or both */
            char cmds[24] = "", scripts[16] = "";
            if(r.commands)
                snprintf(
                    cmds, sizeof(cmds), "%u command%s", r.commands, r.commands == 1 ? "" : "s");
            if(r.scripts)
                snprintf(
                    scripts, sizeof(scripts), "%u script%s", r.scripts, r.scripts == 1 ? "" : "s");
            snprintf(
                text,
                UTIL_SCRATCH_SIZE,
                "Added %s%s%s.%s",
                cmds,
                cmds[0] && scripts[0] ? " and\n" : "",
                scripts,
                r.no_room ? "\nSome did not fit (16 max)." : "\nYour own were kept.");
        }
        app_show_message(app, "Restore Examples", text, SshRunnerSceneSettings);
    }
    return true;
}

void ssh_runner_scene_settings_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
}

/* ---------------- Developer ---------------- */

static const char* const mock_values[] = {"Off", "On"};

#if APP_HAS_MOCK
static void dev_mock_changed(VariableItem* item) {
    SshRunnerApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, mock_values[idx]);
    bool mock = idx == 1;
    if(mock != app->settings.mock) {
        app->settings.mock = mock;
        store_save_settings(app);
        app_backend_restart(app);
    }
}

/* the same words as the home header; chosen from a list ("No network" is wider than the
 * 37 px a list value may show) */
static const char* const mock_board_names[] = {"Linked", "No network", "No board"};
#else
#define dev_mock_changed NULL /* release: no mock backend, the row only shows "Off" */
#endif

static void dev_refresh(SshRunnerApp* app) {
    VariableItemList* vl = app->var_list;
    variable_item_list_reset(vl);
    /* the release build has no mock backend: the row stays (same rows in both builds) */
    VariableItem* it =
        variable_item_list_add(vl, "Mock Backend", APP_HAS_MOCK ? 2 : 1, dev_mock_changed, app);
    variable_item_set_current_value_index(it, app->settings.mock ? 1 : 0);
    variable_item_set_current_value_text(it, mock_values[app->settings.mock ? 1 : 0]);

    /* "ESP32 fw" + "0.1.0": the whole version fits the 37 px value */
    it = variable_item_list_add(vl, "ESP32 fw", 1, NULL, app);
    const BackendLink* l = &app->session->link;
    static char board[32];
    if(l->state == BackendLinkUp && !app->settings.mock)
        snprintf(
            board,
            sizeof(board),
            "%s",
            strncmp(l->version, "fes ", 4) ? l->version : l->version + 4);
    else
        snprintf(
            board,
            sizeof(board),
            "%s",
            l->state == BackendLinkUp       ? "mock" :
            l->state == BackendLinkMismatch ? "mismatch" :
                                              "No board");
    util_fit_value(board, sizeof(board)); /* OFW only: its list cannot scroll a value */
    variable_item_set_current_value_text(it, board);

    it = variable_item_list_add(vl, "Protocol", 1, NULL, app);
    static char proto[8];
    snprintf(proto, sizeof(proto), "v%d", PROTO_VERSION);
    variable_item_set_current_value_text(it, proto);

    it = variable_item_list_add(vl, "Link Test 1 MiB", 1, NULL, app);
    static char lt[16];
    const BackendLinkTest* t = &app->session->link_test;
    if(t->frames && !t->done)
        snprintf(lt, sizeof(lt), "%lu%%", (unsigned long)(t->sent * 100u / t->frames));
    else
        snprintf(
            lt, sizeof(lt), "%s", t->done ? (t->aborted || t->mismatches ? "FAIL" : "OK") : "Run");
    variable_item_set_current_value_text(it, lt);
    if(app->settings.mock) {
        variable_item_list_add(vl, "Mock Board", 1, NULL, app); /* OK: choose */
    }
    variable_item_list_set_enter_callback(vl, util_var_list_enter_cb, app);
}

#define DEV_ITEM_LINK_TEST  3
#define DEV_ITEM_MOCK_BOARD 4
#define LINK_TEST_FRAMES    2048u /* x 512 B = 1 MiB each way */

static void dev_show_link_test(SshRunnerApp* app) {
    const BackendLinkTest* t = &app->session->link_test;
    char text[160];
    uint32_t kbs = t->ms ? (t->bytes / 512u) * 1000u / t->ms :
                           0; /* both ways, KiB/s; 32-bit math */
    char each[16]; /* "1 MiB", as on the Developer row */
    if(t->bytes % 1048576u == 0)
        snprintf(each, sizeof(each), "%lu MiB", (unsigned long)(t->bytes / 1048576u));
    else
        snprintf(each, sizeof(each), "%lu KiB", (unsigned long)(t->bytes / 1024u));
    snprintf(
        text,
        sizeof(text),
        "%s\nFrames: %lu of %lu\nEach way: %s\nUndetected errors: %lu\nRetransmits: %lu\n"
        "Time: %lu.%lu s, %lu KiB/s\nBaud: %lu",
        t->aborted    ? "Link lost (no reply)." :
        t->mismatches ? "Data mismatch!" :
                        "All data verified.",
        (unsigned long)t->sent,
        (unsigned long)t->frames,
        each,
        (unsigned long)t->mismatches,
        (unsigned long)t->retransmits,
        (unsigned long)(t->ms / 1000u),
        (unsigned long)(t->ms % 1000u / 100u),
        (unsigned long)kbs,
        (unsigned long)t->baud);
    app_show_message(app, "Link Test", text, 0);
}

void ssh_runner_scene_developer_on_enter(void* context) {
    SshRunnerApp* app = context;
    dev_refresh(app);
    variable_item_list_set_selected_item(app->var_list, 0); /* always opens at the top */
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewVarList);
}

bool ssh_runner_scene_developer_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == EvtChanged) {
        uint32_t chg = app_take_changes(app);
        if(chg & (SESSION_CHG_LINK | SESSION_CHG_LINKTEST)) {
            uint8_t sel = variable_item_list_get_selected_item_index(app->var_list);
            dev_refresh(app);
            variable_item_list_set_selected_item(app->var_list, sel);
        }
        if((chg & SESSION_CHG_LINKTEST) && app->session->link_test.done) dev_show_link_test(app);
        return true;
    }
    if(event.type == SceneManagerEventTypeCustom && event.event == DEV_ITEM_MOCK_BOARD &&
       app->settings.mock) {
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneMockBoard);
        return true;
    }
    if(event.type == SceneManagerEventTypeCustom && event.event == DEV_ITEM_LINK_TEST) {
        const BackendLinkTest* t = &app->session->link_test;
        if(t->frames && !t->done) return true; /* already running */
        if(app->backend.ops->link_test(app->backend.self, LINK_TEST_FRAMES)) {
            memset(&app->session->link_test, 0, sizeof(app->session->link_test));
            app->session->link_test.frames = LINK_TEST_FRAMES;
            dev_refresh(app);
            variable_item_list_set_selected_item(app->var_list, DEV_ITEM_LINK_TEST);
        } else {
            app_show_error(app, "Link Test", "Needs a connected board", 0);
        }
        return true;
    }
    return false;
}

void ssh_runner_scene_developer_on_exit(void* context) {
    SshRunnerApp* app = context;
    variable_item_list_reset(app->var_list);
}

/* ---------------- Mock board (Developer, mock only) ---------------- */

#if APP_HAS_MOCK
void ssh_runner_scene_mock_board_on_enter(void* context) {
    SshRunnerApp* app = context;
    Submenu* sm = app->submenu;
    util_submenu_reset(sm);
    submenu_set_header(sm, "Mock Board");
    for(uint32_t i = 0; i < COUNT_OF(mock_board_names); i++)
        util_submenu_add(sm, mock_board_names[i], i, util_submenu_cb, app);
    submenu_set_selected_item(sm, app->mock_scenario);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool ssh_runner_scene_mock_board_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event >= COUNT_OF(mock_board_names))
        return false;
    app->mock_scenario = (uint8_t)event.event;
    if(app->backend.ops->mock_scenario)
        app->backend.ops->mock_scenario(app->backend.self, app->mock_scenario);
    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void ssh_runner_scene_mock_board_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
}
#else
/* release: never entered (Developer offers it only with the mock backend on) */
void ssh_runner_scene_mock_board_on_enter(void* context) {
    UNUSED(context);
}

bool ssh_runner_scene_mock_board_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ssh_runner_scene_mock_board_on_exit(void* context) {
    UNUSED(context);
}
#endif

/* ---------------- About ---------------- */

void ssh_runner_scene_about_on_enter(void* context) {
    SshRunnerApp* app = context;
    TextView* tv = app->text_view;
    text_view_reset(tv);
    const BackendLink* l = &app->session->link;
    /* words in FontSecondary; the board's version and the data folder on their own
     * FontKeyboard lines (no technical tokens in a proportional font). TextView copies every
     * block. */
    char text[240];
    snprintf(
        text,
        sizeof(text),
        "Run SSH commands and SD card scripts through an ESP32-S3 + W5500 add-on, "
        "over LAN or WiFi.\n"
        "\n"
        "Backend: %s",
        app->settings.mock ? "mock (demo)" : "ESP32 (UART)");
    /* the board line says what it is ("ESP32 fw 0.1.0" / "No board"): no label */
    /* bytes the receive interrupt had to drop (buffer full): shown only when there are any */
    char lost[24] = "";
    if(l->rx_dropped) snprintf(lost, sizeof(lost), " (%lu lost)", (unsigned long)l->rx_dropped);
    char more[224];
    snprintf(
        more,
        sizeof(more),
        "Version " SSH_RUNNER_VERSION "\n"
        "Protocol: v%d\n"
        "Link: %s\nSent %lu B, received %lu B%s\n"
        "Memory: %lu B free at start\n"
        "License: GPL-3.0\n"
        "Author: " SSH_RUNNER_AUTHOR "\n"
        "Website:",
        PROTO_VERSION,
        app->settings.mock ? "mock" : (l->port_ok ? "USART ok" : "USART busy"),
        (unsigned long)l->tx_bytes,
        (unsigned long)l->rx_bytes,
        lost,
        (unsigned long)app->heap_at_start);
    char board[32];
    if(l->state == BackendLinkUp)
        util_board_version(l->version, board, sizeof(board));
    else
        snprintf(board, sizeof(board), "No board");
    text_view_set_header(tv, "SSH Runner", NULL); /* words; the version is in the text */
    text_view_set_text(tv, 0, text, TextViewFontBody);
    /* the version is technical text; "No board" is words */
    text_view_set_text(
        tv, 1, board, l->state == BackendLinkUp ? TextViewFontMono : TextViewFontBody);
    text_view_set_text(tv, 2, more, TextViewFontBody);
    text_view_set_text(tv, 3, SSH_RUNNER_WEB, TextViewFontMono);
    text_view_set_text(tv, 4, "\nData folder on the SD card:", TextViewFontBody);
    text_view_set_text(tv, 5, "apps_data/ssh_runner", TextViewFontMono);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewText);
}

bool ssh_runner_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ssh_runner_scene_about_on_exit(void* context) {
    SshRunnerApp* app = context;
    text_view_reset(app->text_view);
}

#include "../sigroam.h"

#include "../src/sr_board_tools.h"
#include "../src/sr_dialect.h"
#include "../src/sr_marauder_join.h"
#include "../src/sr_marauder_upload.h"
#include "../src/sr_mau_logs.h"

#include <input/input.h>
#include <stdio.h>
#include <string.h>

/*
 * Scout Lite board tools. Non-lite builds never reach a transmit.
 * One worker command per tick: a failed send stays queued and retries.
 */

enum {
    PgPeer = 0,
    PgGate,
    PgMenu,
    PgIgnore,
    PgLogs,
    PgSplit,
    PgAsk,
    PgNote,
    PgAdd,
};

enum {
    AskRm = 1,
    AskClear,
    AskLog,
    AskAll,
};

enum {
    WaitNone = 0,
    WaitList,
    WaitLs,
    WaitAdded,
    WaitRemoved,
    WaitCleared,
    WaitFile,
    WaitFiles,
    WaitSplit,
};

enum {
    TagPlain = 0,
    TagIgnBar,
    TagLsBar,
};

enum {
    ItemAdd = 100,
    ItemHome = 101,
    ItemClear = 102,
    ItemHint = 103,
    ItemMore = 200,
    ItemAll = 201,
    ItemSplit = 300,
    ItemNow = 310,
    ItemApply = 311,
    ItemReboot = 312,
};

enum {
    EvLeft = 1,
    EvCenter = 2,
    EvRight = 3,
    EvAdd = 4,
    EvItem = 1000,
};

static uint8_t s_page;
static uint8_t s_ask;
static uint8_t s_wait;
static uint8_t s_tag;
static bool s_cmd_on;
static bool s_listing;
static bool s_split_known;
static bool s_log_sent;
static char s_cmd[96];
static char s_banner[32];
static char s_add[SR_BT_SSID_MAX + 1u];
static char s_rm_ssid[SR_BT_SSID_MAX + 1u];
static char s_rm_name[SR_MAU_LOG_NAME_MAX + 1u];
static unsigned s_rm_ord;
static uint32_t s_split_kb;
static int s_split_pick;
static uint32_t s_reply_seen;
static uint32_t s_split_seen;
static uint8_t s_back;
static unsigned s_row_n;
static char s_rows[20][28];
static SrBtDel s_del;

static const char* tools_keep(const char* text) {
    if(text == NULL) {
        text = "";
    }
    if(s_row_n >= 20u) {
        return text;
    }
    snprintf(s_rows[s_row_n], sizeof(s_rows[0]), "%s", text);
    return s_rows[s_row_n++];
}

static void tools_show_menu(SigRoamApp* app);
static void tools_show_ignore(SigRoamApp* app);
static void tools_show_logs(SigRoamApp* app);
static void tools_show_split(SigRoamApp* app);
static void tools_show_ask(SigRoamApp* app);

static bool tools_lite(const SigRoamApp* app) {
    return sr_dialect_lite_status(&app->model.firmware);
}

static SrBtGate tools_gate(const SigRoamApp* app) {
    return sr_bt_gate((unsigned)app->model.session, sr_mau_up_in_flight(), sr_join_listening());
}

static void tools_button(GuiButtonType result, InputType type, void* context) {
    SigRoamApp* app = context;
    uint32_t ev = EvLeft;

    if(app == NULL || type != InputTypeShort) {
        return;
    }
    if(result == GuiButtonTypeCenter) {
        ev = EvCenter;
    } else if(result == GuiButtonTypeRight) {
        ev = EvRight;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, ev);
}

static void tools_item(void* context, uint32_t index) {
    SigRoamApp* app = context;

    if(app != NULL) {
        view_dispatcher_send_custom_event(app->view_dispatcher, EvItem + index);
    }
}

static void tools_add_done(void* context) {
    SigRoamApp* app = context;

    if(app != NULL) {
        view_dispatcher_send_custom_event(app->view_dispatcher, EvAdd);
    }
}

static void tools_text(SigRoamApp* app, uint8_t page, const char* text, bool buttons) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget, 0, 0, SR_CANVAS_W, buttons ? (uint8_t)(SR_CANVAS_H - 14) : SR_CANVAS_H, text);
    s_page = page;
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewWidget);
}

static void tools_btn(SigRoamApp* app, GuiButtonType side, const char* label) {
    widget_add_button_element(app->widget, side, label, tools_button, app);
}

static void tools_stop_tx(void) {
    s_cmd_on = false;
    s_wait = (uint8_t)WaitNone;
    s_listing = false;
}

static bool tools_enqueue(const char* cmd, uint8_t tag) {
    size_t n = 0u;

    if(s_cmd_on || cmd == NULL) {
        return false;
    }
    while(cmd[n] != '\0' && n + 1u < sizeof(s_cmd)) {
        s_cmd[n] = cmd[n];
        n++;
    }
    if(cmd[n] != '\0') {
        return false;
    }
    s_cmd[n] = '\0';
    s_cmd_on = true;
    s_tag = tag;
    return true;
}

static void tools_show_gate(SigRoamApp* app, SrBtGate gate) {
    const char* text = "Stop scan first";

    tools_stop_tx();
    sr_bt_reset();
    sr_mau_logs_reset();
    if(gate == SrBtGateUpload) {
        text = "Upload running";
    } else if(gate == SrBtGateJoin) {
        text = "Joining Wi-Fi";
    }
    tools_text(app, (uint8_t)PgGate, text, false);
}

static void tools_show_peer(SigRoamApp* app) {
    SrBtPeer peer = sr_bt_peer(app->model.firmware.version[0] != '\0', tools_lite(app));
    const char* text = "Needs Scout Lite\nfirmware";

    if(peer == SrBtPeerEmpty) {
        text = "Open Dashboard or\nProbe first";
    }
    tools_text(app, (uint8_t)PgPeer, text, false);
}

static void tools_show_menu(SigRoamApp* app) {
    s_banner[0] = '\0';
    tools_stop_tx();
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Board tools");
    submenu_add_item(app->submenu, "Ignore list", 0u, tools_item, app);
    submenu_add_item(app->submenu, "Logs on card", 1u, tools_item, app);
    submenu_add_item(app->submenu, "Split size", 2u, tools_item, app);
    s_page = (uint8_t)PgMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewSubmenu);
}

static void tools_show_note(SigRoamApp* app, const char* text, uint8_t back) {
    s_wait = (uint8_t)WaitNone;
    s_back = back;
    tools_text(app, (uint8_t)PgNote, text, true);
    tools_btn(app, GuiButtonTypeCenter, "OK");
}

static bool tools_begin(SigRoamApp* app, const char* cmd) {
    SrBtGate gate = tools_gate(app);

    if(gate != SrBtGateOk) {
        tools_show_gate(app, gate);
        return false;
    }
    if(!tools_enqueue(cmd, (uint8_t)TagPlain)) {
        return false;
    }
    tools_text(app, s_page, "Reading...", false);
    return true;
}

static void tools_start_list(SigRoamApp* app) {
    sr_bt_arm_list();
    s_wait = (uint8_t)WaitList;
    s_listing = false;
    if(!tools_begin(app, sr_bt_ignore_cmd())) {
        s_wait = (uint8_t)WaitNone;
    }
}

static void tools_start_ls(SigRoamApp* app) {
    sr_mau_logs_arm();
    sr_bt_arm_reply();
    s_reply_seen = sr_bt_reply_seq();
    s_listing = true;
    s_wait = (uint8_t)WaitLs;
    if(!tools_begin(app, sr_mau_logs_cmd())) {
        s_listing = false;
        s_wait = (uint8_t)WaitNone;
    }
}

static void tools_start_reply(SigRoamApp* app, const char* cmd, uint8_t wait) {
    sr_bt_arm_reply();
    s_reply_seen = sr_bt_reply_seq();
    s_split_seen = sr_bt_split_seq();
    s_wait = wait;
    if(!tools_begin(app, cmd)) {
        s_wait = (uint8_t)WaitNone;
    }
}

static void tools_show_ignore(SigRoamApp* app) {
    unsigned i;
    unsigned ord = 0u;
    char ssid[SR_BT_SSID_MAX + 1u];
    char shown[21];
    bool listed = false;
    const char* home = app->model.cfg.ssid;

    s_row_n = 0u;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, s_banner[0] != '\0' ? s_banner : "Ignore list");
    for(i = 0u; i < sr_bt_ignore_count(); i++) {
        if(!sr_bt_ignore_item(i, &ord, ssid, sizeof(ssid))) {
            continue;
        }
        sr_bt_show_ssid(ssid, shown, sizeof(shown));
        submenu_add_item(app->submenu, tools_keep(shown), i, tools_item, app);
        if(home != NULL && strcmp(ssid, home) == 0) {
            listed = true;
        }
    }
    submenu_add_item(app->submenu, "+ Add SSID", (uint32_t)ItemAdd, tools_item, app);
    if(app->model.cfg_rev != 0u && sr_bt_offer_home(app->model.cfg.home, home, listed)) {
        submenu_add_item(app->submenu, "+ Add home Wi-Fi", (uint32_t)ItemHome, tools_item, app);
    }
    if(sr_bt_ignore_count() > 0u) {
        submenu_add_item(app->submenu, "Clear all", (uint32_t)ItemClear, tools_item, app);
    }
    submenu_add_item(app->submenu, "Skips SD log only", (uint32_t)ItemHint, tools_item, app);
    s_page = (uint8_t)PgIgnore;
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewSubmenu);
}

static void tools_show_logs(SigRoamApp* app) {
    unsigned i;
    char name[SR_MAU_LOG_NAME_MAX + 1u];
    char row[24];
    uint32_t size = 0u;
    uint32_t extra;
    bool sent = false;

    s_row_n = 0u;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, s_banner[0] != '\0' ? s_banner : "Logs on card");
    for(i = 0u; sr_mau_logs_slot(i, name, sizeof(name), &size, &sent); i++) {
        if(!sr_bt_log_row(name, size, sent, row, sizeof(row))) {
            continue;
        }
        submenu_add_item(app->submenu, tools_keep(row), i, tools_item, app);
    }
    extra = sr_mau_logs_extra();
    if(extra != SR_MAU_LOGS_UNKNOWN && sr_bt_log_more(extra, row, sizeof(row))) {
        submenu_add_item(app->submenu, tools_keep(row), (uint32_t)ItemMore, tools_item, app);
    }
    if(sr_mau_logs_matches() > 0u) {
        submenu_add_item(app->submenu, "Delete all logs", (uint32_t)ItemAll, tools_item, app);
    }
    s_page = (uint8_t)PgLogs;
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewSubmenu);
}

static void tools_show_split(SigRoamApp* app) {
    unsigned i;
    char row[28];

    s_row_n = 0u;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Split size");
    if(s_split_known && !sr_bt_split_is_preset(s_split_kb)) {
        snprintf(row, sizeof(row), "Now: %lu KB", (unsigned long)s_split_kb);
        submenu_add_item(app->submenu, tools_keep(row), (uint32_t)ItemNow, tools_item, app);
    }
    for(i = 0u; i < 6u; i++) {
        const char* label = sr_bt_split_label(i);
        bool mark = s_split_known && sr_bt_split_kb(i) == s_split_kb;

        snprintf(row, sizeof(row), "%s%s", mark ? "* " : "", label != NULL ? label : "");
        submenu_add_item(app->submenu, tools_keep(row), (uint32_t)ItemSplit + i, tools_item, app);
    }
    submenu_add_item(app->submenu, "Applies to next scan", (uint32_t)ItemApply, tools_item, app);
    submenu_add_item(app->submenu, "Board reboot: 1024 KB", (uint32_t)ItemReboot, tools_item, app);
    {
        int cur = sr_bt_split_cursor(s_split_known, s_split_kb, s_split_pick);

        submenu_set_selected_item(
            app->submenu,
            cur == SR_BT_SPLIT_CUR_NOW ? (uint32_t)ItemNow : (uint32_t)ItemSplit + (uint32_t)cur);
    }
    s_page = (uint8_t)PgSplit;
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewSubmenu);
}

static void tools_show_ask(SigRoamApp* app) {
    char text[96];
    char detail[48];

    text[0] = '\0';
    if(s_ask == (uint8_t)AskRm) {
        snprintf(text, sizeof(text), "Remove %s?", s_rm_ssid);
    } else if(s_ask == (uint8_t)AskClear) {
        snprintf(text, sizeof(text), "Clear all?");
    } else if(s_ask == (uint8_t)AskLog) {
        snprintf(text, sizeof(text), "Delete %s?", s_rm_name);
        if(!s_log_sent && strlen(text) + 18u < sizeof(text)) {
            memcpy(text + strlen(text), "\nNot uploaded yet", 18u);
        }
    } else if(s_del.phase == (uint8_t)SrBtDelAll2) {
        snprintf(text, sizeof(text), "Really delete all?\nCannot be undone");
    } else {
        snprintf(text, sizeof(text), "Delete all logs?");
        if(sr_bt_del_all_detail(
               sr_mau_logs_matches(), sr_mau_logs_unsent(), detail, sizeof(detail)) &&
           strlen(text) + 1u + strlen(detail) + 1u < sizeof(text)) {
            size_t n = strlen(text);

            text[n++] = '\n';
            memcpy(text + n, detail, strlen(detail) + 1u);
        }
    }
    tools_text(app, (uint8_t)PgAsk, text, true);
    tools_btn(app, GuiButtonTypeLeft, "Cancel");
    if(s_ask == (uint8_t)AskAll && s_del.phase == (uint8_t)SrBtDelAll2) {
        tools_btn(app, GuiButtonTypeRight, "Delete");
    } else if(s_ask == (uint8_t)AskAll) {
        tools_btn(app, GuiButtonTypeCenter, "Next");
    } else if(s_ask == (uint8_t)AskClear) {
        tools_btn(app, GuiButtonTypeCenter, "Clear");
    } else if(s_ask == (uint8_t)AskRm) {
        tools_btn(app, GuiButtonTypeCenter, "Remove");
    } else {
        tools_btn(app, GuiButtonTypeCenter, "Delete");
    }
}

static void tools_open_add(SigRoamApp* app) {
    if(sr_bt_list_full(sr_bt_ignore_count())) {
        tools_show_note(app, "List full (10)", (uint8_t)PgIgnore);
        return;
    }
    memset(s_add, 0, sizeof(s_add));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Add SSID");
    text_input_set_minimum_length(app->text_input, 1u);
    text_input_set_result_callback(
        app->text_input, tools_add_done, app, s_add, sizeof(s_add), true);
    s_page = (uint8_t)PgAdd;
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewTextInput);
}

static void tools_send_add(SigRoamApp* app, const char* ssid) {
    char cmd[80];
    size_t n = 0u;

    if(ssid == NULL) {
        tools_show_ignore(app);
        return;
    }
    while(ssid[n] != '\0') {
        n++;
    }
    if(n == 0u) {
        tools_show_ignore(app);
        return;
    }
    if(sr_bt_quote_add_cmd(ssid, n, cmd, sizeof(cmd)) != SrBtQuoteOk) {
        tools_show_note(app, "Can't add this SSID", (uint8_t)PgIgnore);
        return;
    }
    tools_start_reply(app, cmd, (uint8_t)WaitAdded);
}

static void tools_back_list(SigRoamApp* app) {
    if(s_page == (uint8_t)PgNote) {
        if(s_back == (uint8_t)PgLogs) {
            tools_show_logs(app);
        } else if(s_back == (uint8_t)PgSplit) {
            tools_show_split(app);
        } else if(s_back == (uint8_t)PgMenu) {
            tools_show_menu(app);
        } else {
            tools_show_ignore(app);
        }
        return;
    }
    if(s_ask == (uint8_t)AskLog || s_ask == (uint8_t)AskAll || s_page == (uint8_t)PgLogs) {
        tools_show_logs(app);
    } else if(s_page == (uint8_t)PgSplit) {
        tools_show_split(app);
    } else {
        tools_show_ignore(app);
    }
}

static void tools_on_send(SigRoamApp* app, SrBtDelAct act) {
    char cmd[80];

    if(act == SrBtDelActSendAll) {
        tools_start_reply(app, sr_bt_rm_all_cmd(), (uint8_t)WaitFiles);
        return;
    }
    if(s_ask == (uint8_t)AskClear) {
        tools_start_reply(app, sr_bt_clear_cmd(), (uint8_t)WaitCleared);
        return;
    }
    if(s_ask == (uint8_t)AskRm) {
        if(!sr_bt_ignore_rm_cmd(s_rm_ord, cmd, sizeof(cmd))) {
            tools_show_note(app, "Invalid index", (uint8_t)PgIgnore);
            return;
        }
        tools_start_reply(app, cmd, (uint8_t)WaitRemoved);
        return;
    }
    if(!sr_bt_rm_cmd(s_rm_name, cmd, sizeof(cmd))) {
        tools_show_note(app, "Remove failed", (uint8_t)PgLogs);
        return;
    }
    tools_start_reply(app, cmd, (uint8_t)WaitFile);
}

static void tools_on_button(SigRoamApp* app, unsigned key) {
    SrBtDelAct act;
    bool blocked = tools_gate(app) != SrBtGateOk;

    if(s_page == (uint8_t)PgNote) {
        if(key == (unsigned)EvCenter || key == (unsigned)EvLeft) {
            tools_back_list(app);
        }
        return;
    }
    if(s_page != (uint8_t)PgAsk) {
        return;
    }
    act = sr_bt_del_on_key(&s_del, key, furi_get_tick(), blocked);
    if(act == SrBtDelActCancel) {
        tools_back_list(app);
        return;
    }
    if(act == SrBtDelActNext) {
        tools_show_ask(app);
        return;
    }
    if(act == SrBtDelActSendOne || act == SrBtDelActSendAll) {
        tools_on_send(app, act);
        return;
    }
    if(blocked && (key == (unsigned)EvCenter || key == (unsigned)EvRight)) {
        tools_show_gate(app, tools_gate(app));
    }
}

static void tools_on_item(SigRoamApp* app, uint32_t index) {
    char name[SR_MAU_LOG_NAME_MAX + 1u];
    char ssid[SR_BT_SSID_MAX + 1u];
    uint32_t size = 0u;
    unsigned ord = 0u;
    bool sent = false;

    if(s_page == (uint8_t)PgMenu) {
        if(index == 0u) {
            s_banner[0] = '\0';
            tools_start_list(app);
        } else if(index == 1u) {
            s_banner[0] = '\0';
            tools_start_ls(app);
        } else if(index == 2u) {
            tools_start_reply(app, sr_bt_split_query_cmd(), (uint8_t)WaitSplit);
        }
        return;
    }
    if(s_page == (uint8_t)PgIgnore) {
        if(index == (uint32_t)ItemHint) {
            return;
        }
        if(index == (uint32_t)ItemAdd) {
            tools_open_add(app);
            return;
        }
        if(index == (uint32_t)ItemHome) {
            tools_send_add(app, app->model.cfg.ssid);
            return;
        }
        if(index == (uint32_t)ItemClear) {
            s_ask = (uint8_t)AskClear;
            sr_bt_del_open_one(&s_del);
            tools_show_ask(app);
            return;
        }
        if(sr_bt_ignore_item(index, &ord, ssid, sizeof(ssid))) {
            s_rm_ord = ord;
            memcpy(s_rm_ssid, ssid, strlen(ssid) + 1u);
            s_ask = (uint8_t)AskRm;
            sr_bt_del_open_one(&s_del);
            tools_show_ask(app);
        }
        return;
    }
    if(s_page == (uint8_t)PgLogs) {
        if(index == (uint32_t)ItemMore) {
            return;
        }
        if(index == (uint32_t)ItemAll) {
            s_ask = (uint8_t)AskAll;
            sr_bt_del_open_all(&s_del, furi_get_tick());
            tools_show_ask(app);
            return;
        }
        if(sr_mau_logs_slot(index, name, sizeof(name), &size, &sent)) {
            memcpy(s_rm_name, name, strlen(name) + 1u);
            s_log_sent = sent;
            s_ask = (uint8_t)AskLog;
            sr_bt_del_open_one(&s_del);
            tools_show_ask(app);
        }
        return;
    }
    if(s_page == (uint8_t)PgSplit) {
        char cmd[32];
        unsigned preset;

        if(index == (uint32_t)ItemNow || index == (uint32_t)ItemApply ||
           index == (uint32_t)ItemReboot) {
            return;
        }
        if(index < (uint32_t)ItemSplit || index >= (uint32_t)ItemSplit + 6u) {
            return;
        }
        preset = (unsigned)(index - (uint32_t)ItemSplit);
        s_split_pick = (int)preset;
        if(!sr_bt_split_cmd(sr_bt_split_kb(preset), cmd, sizeof(cmd))) {
            return;
        }
        tools_start_reply(app, cmd, (uint8_t)WaitSplit);
    }
}

static void tools_on_add(SigRoamApp* app) {
    tools_send_add(app, s_add);
}

static void tools_take_split(void) {
    uint32_t kb = 0u;

    if(sr_bt_split_seq() == s_split_seen) {
        return;
    }
    s_split_seen = sr_bt_split_seq();
    if(sr_bt_split_value(&kb)) {
        s_split_kb = kb;
        s_split_known = true;
    }
}

static void tools_finish_reply(SigRoamApp* app) {
    unsigned kind = sr_bt_reply_kind();

    if(kind == (unsigned)SrBtReplyBusy || sr_bt_ignore_busy()) {
        tools_show_gate(app, SrBtGateScan);
        return;
    }
    if(kind == (unsigned)SrBtReplySd) {
        tools_show_note(app, "SD not available", (uint8_t)PgLogs);
        return;
    }
    if(kind == (unsigned)SrBtReplyRemoveFail) {
        tools_show_note(app, "Remove failed", (uint8_t)PgLogs);
        return;
    }
    if(s_wait == (uint8_t)WaitFiles && kind == (unsigned)SrBtReplyFile) {
        return;
    }
    if(s_wait == (uint8_t)WaitAdded) {
        if(kind == (unsigned)SrBtReplyAddFail) {
            snprintf(s_banner, sizeof(s_banner), "Add failed");
        } else if(kind == (unsigned)SrBtReplyAdded) {
            s_banner[0] = '\0';
        } else {
            return;
        }
        tools_start_list(app);
        return;
    }
    if(s_wait == (uint8_t)WaitRemoved) {
        if(kind == (unsigned)SrBtReplyBadIndex) {
            snprintf(s_banner, sizeof(s_banner), "Invalid index");
        } else if(kind == (unsigned)SrBtReplyRemoved) {
            s_banner[0] = '\0';
        } else {
            return;
        }
        tools_start_list(app);
        return;
    }
    if(s_wait == (uint8_t)WaitCleared) {
        if(kind != (unsigned)SrBtReplyCleared) {
            return;
        }
        s_banner[0] = '\0';
        tools_start_list(app);
        return;
    }
    if(s_wait == (uint8_t)WaitFile) {
        if(kind != (unsigned)SrBtReplyFile) {
            return;
        }
        s_banner[0] = '\0';
        tools_start_ls(app);
        return;
    }
    if(s_wait == (uint8_t)WaitFiles) {
        if(kind != (unsigned)SrBtReplyFiles) {
            return;
        }
        snprintf(s_banner, sizeof(s_banner), "Deleted %lu files", (unsigned long)sr_bt_reply_n());
        tools_start_ls(app);
        return;
    }
    if(s_wait == (uint8_t)WaitSplit && kind == (unsigned)SrBtReplySplitBad && !s_split_known) {
        tools_show_note(app, "Invalid size", (uint8_t)PgSplit);
    }
}

static void tools_observe(SigRoamApp* app) {
    bool split_moved;

    if(s_wait == (uint8_t)WaitList && sr_bt_ignore_closed()) {
        s_wait = (uint8_t)WaitNone;
        if(sr_bt_ignore_busy()) {
            tools_show_gate(app, SrBtGateScan);
        } else {
            tools_show_ignore(app);
        }
        return;
    }
    if(s_wait == (uint8_t)WaitLs && sr_bt_reply_seq() != s_reply_seen) {
        unsigned kind = sr_bt_reply_kind();

        if(kind == (unsigned)SrBtReplyBusy || kind == (unsigned)SrBtReplySd) {
            s_reply_seen = sr_bt_reply_seq();
            s_listing = false;
            tools_finish_reply(app);
            return;
        }
    }
    if(s_wait == (uint8_t)WaitLs && sr_mau_logs_closed()) {
        s_wait = (uint8_t)WaitNone;
        s_listing = false;
        tools_show_logs(app);
        return;
    }
    split_moved = sr_bt_split_seq() != s_split_seen;
    tools_take_split();
    if(split_moved && (s_page == (uint8_t)PgSplit || s_wait == (uint8_t)WaitSplit)) {
        if(s_wait == (uint8_t)WaitSplit) {
            s_wait = (uint8_t)WaitNone;
        }
        tools_show_split(app);
    }
    if(s_wait != (uint8_t)WaitNone && sr_bt_reply_seq() != s_reply_seen) {
        s_reply_seen = sr_bt_reply_seq();
        tools_finish_reply(app);
    }
}

static void tools_pump(SigRoamApp* app) {
    SrBtGate gate;

    if(!tools_lite(app)) {
        return;
    }
    gate = tools_gate(app);
    if(gate != SrBtGateOk) {
        if(s_page != (uint8_t)PgGate) {
            tools_show_gate(app, gate);
        }
        return;
    }
    if(s_page == (uint8_t)PgGate) {
        tools_show_menu(app);
        return;
    }
    if(s_cmd_on) {
        if(app->worker != NULL && sr_worker_send_cmd(app->worker, s_cmd)) {
            s_cmd_on = false;
            if(s_tag == (uint8_t)TagIgnBar) {
                sr_bt_ignore_barrier_sent();
            } else if(s_tag == (uint8_t)TagLsBar) {
                sr_mau_logs_barrier_sent();
            }
        }
        return;
    }
    if(sr_bt_ignore_barrier_due()) {
        (void)tools_enqueue(sr_bt_barrier_cmd(), (uint8_t)TagIgnBar);
        return;
    }
    if(s_listing && sr_mau_logs_barrier_due()) {
        (void)tools_enqueue(sr_mau_logs_barrier_cmd(), (uint8_t)TagLsBar);
        return;
    }
    tools_observe(app);
}

void sigroam_scene_tools_on_enter(void* context) {
    SigRoamApp* app = context;

    s_page = (uint8_t)PgMenu;
    s_ask = 0u;
    s_wait = (uint8_t)WaitNone;
    s_tag = (uint8_t)TagPlain;
    s_cmd_on = false;
    s_listing = false;
    s_split_known = false;
    s_split_pick = -1;
    s_log_sent = false;
    s_banner[0] = '\0';
    s_add[0] = '\0';
    s_reply_seen = 0u;
    s_split_seen = 0u;
    sr_bt_del_reset(&s_del);
    sr_bt_reset();
    if(!tools_lite(app)) {
        tools_show_peer(app);
        return;
    }
    if(tools_gate(app) != SrBtGateOk) {
        tools_show_gate(app, tools_gate(app));
        return;
    }
    tools_show_menu(app);
}

bool sigroam_scene_tools_on_event(void* context, SceneManagerEvent event) {
    SigRoamApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        if(s_page == (uint8_t)PgAsk || s_page == (uint8_t)PgNote || s_page == (uint8_t)PgAdd) {
            sr_bt_del_reset(&s_del);
            tools_back_list(app);
            return true;
        }
        if(s_page == (uint8_t)PgIgnore || s_page == (uint8_t)PgLogs ||
           s_page == (uint8_t)PgSplit || s_wait != (uint8_t)WaitNone) {
            tools_stop_tx();
            sr_bt_reset();
            sr_mau_logs_reset();
            tools_show_menu(app);
            return true;
        }
        return false;
    }
    if(event.type == SceneManagerEventTypeCustom) {
        if(!tools_lite(app)) {
            return true;
        }
        if(event.event == (uint32_t)EvAdd) {
            tools_on_add(app);
            return true;
        }
        if(event.event == (uint32_t)EvLeft || event.event == (uint32_t)EvCenter ||
           event.event == (uint32_t)EvRight) {
            tools_on_button(app, (unsigned)event.event);
            return true;
        }
        if(event.event >= (uint32_t)EvItem) {
            tools_on_item(app, event.event - (uint32_t)EvItem);
            return true;
        }
        return true;
    }
    if(event.type == SceneManagerEventTypeTick) {
        tools_pump(app);
        return true;
    }
    return false;
}

void sigroam_scene_tools_on_exit(void* context) {
    SigRoamApp* app = context;

    tools_stop_tx();
    sr_bt_reset();
    sr_mau_logs_reset();
    if(app->widget != NULL) {
        widget_reset(app->widget);
    }
    if(app->submenu != NULL) {
        submenu_reset(app->submenu);
    }
    if(app->text_input != NULL) {
        text_input_reset(app->text_input);
    }
}

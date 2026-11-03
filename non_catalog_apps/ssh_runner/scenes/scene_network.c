/* Network status, per-interface IP settings, WiFi scan/join, ping.
 * SPDX-License-Identifier: GPL-3.0-only */
#include "scene_util.h"
#include <ssh_runner_icons.h>

enum {
    NetLan,
    NetWifi,
    NetJoin,
    NetPing,
};

static char net_lan_label[40];
static char net_wifi_label[48];

/* The row: the name, and flush right what the interface is doing; once up, LAN shows its
 * address and WiFi its network (the address is one level in, like a phone's WiFi list). */
static void iface_label(SshRunnerApp* app, uint8_t iface, char* buf, size_t size) {
    Session* s = app->session;
    const char* name = iface == ProtoIfaceLan ? "LAN" : "WiFi";
    const ProtoIfaceStatus* st = iface == ProtoIfaceLan ? &s->net.lan : &s->net.wifi;
    char value[PROTO_STR_SSID + 1];
    if(s->link.state != BackendLinkUp) {
        snprintf(value, sizeof(value), "No board");
    } else if(!(st->flags & PROTO_NET_PRESENT)) {
        snprintf(value, sizeof(value), "Not present");
    } else if(!(st->flags & PROTO_NET_LINK)) {
        snprintf(value, sizeof(value), iface == ProtoIfaceLan ? "No cable" : "Off");
    } else if(iface == ProtoIfaceWifi && s->net.wifi_ssid[0]) {
        text_ascii(s->net.wifi_ssid, value, sizeof(value), TEXT_TOFU);
    } else if(!(st->flags & PROTO_NET_IP)) {
        snprintf(value, sizeof(value), "Waiting for IP");
    } else {
        ip4_format(st->ip, value, sizeof(value));
    }
    util_submenu_flush(buf, size, name, value);
}

void ssh_runner_scene_network_on_enter(void* context) {
    SshRunnerApp* app = context;
    app->backend.ops->net_get_status(app->backend.self);
    Submenu* sm = app->submenu;
    util_submenu_reset(sm);
    submenu_set_header(sm, "Network");
    iface_label(app, ProtoIfaceLan, net_lan_label, sizeof(net_lan_label));
    iface_label(app, ProtoIfaceWifi, net_wifi_label, sizeof(net_wifi_label));
    util_submenu_add(sm, net_lan_label, NetLan, util_submenu_cb, app);
    util_submenu_add(sm, net_wifi_label, NetWifi, util_submenu_cb, app);
    util_submenu_add(sm, "Join WiFi", NetJoin, util_submenu_cb, app);
    util_submenu_add(sm, "Ping Host", NetPing, util_submenu_cb, app);
    submenu_set_selected_item(
        sm, scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneNetwork));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool ssh_runner_scene_network_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == EvtChanged) {
        if(app_take_changes(app) & (SESSION_CHG_NET | SESSION_CHG_LINK)) {
            iface_label(app, ProtoIfaceLan, net_lan_label, sizeof(net_lan_label));
            iface_label(app, ProtoIfaceWifi, net_wifi_label, sizeof(net_wifi_label));
            submenu_change_item_label(app->submenu, NetLan, net_lan_label);
            submenu_change_item_label(app->submenu, NetWifi, net_wifi_label);
        }
        return true;
    }
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneNetwork, event.event);
    switch(event.event) {
    case NetLan:
    case NetWifi:
        app->net_iface = event.event == NetLan ? ProtoIfaceLan : ProtoIfaceWifi;
        scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneNetIface, 0);
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneNetIface);
        break;
    case NetJoin:
        scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneWifiScan, 0);
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneWifiScan);
        break;
    case NetPing:
        app->ping_via = PING_VIA_ALL;
        util_start_input(app, InputPingHost, PING_DEFAULT_HOST);
        break;
    }
    return true;
}

void ssh_runner_scene_network_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
}

/* ---------------- Interface settings ----------------
 * Row ids, not positions: with Mode = DHCP the static rows are hidden (they do not apply)
 * and come back in place for Static, so Mode keeps its position (stock network editors).
 * The first row is read-only status: the live address when there is one, else why not;
 * it has no arrows and OK does nothing on it. */

enum {
    IfAddress,
    IfMode,
    IfIp,
    IfMask,
    IfGateway,
    IfDns,
    IfPingGw,
    IfApply,
    IfCount,
};

#define EVT_IF_MODE (EvtBase + 200)

static const char* const mode_names[] = {"DHCP", "Static"};
#define if_vals ((char(*)[64])util_scratch) /* 5 rows: "Static IP      192.168.1.50" */
static bool if_applying;
static uint8_t if_rows[IfCount]; /* list position -> row id */
static uint8_t if_row_count;

static uint8_t if_position(uint8_t row) {
    for(uint8_t i = 0; i < if_row_count; i++)
        if(if_rows[i] == row) return i;
    return 0;
}

static void if_mode_changed(VariableItem* it) {
    SshRunnerApp* app = variable_item_get_context(it);
    app->net_cfg.mode = variable_item_get_current_value_index(it);
    variable_item_set_current_value_text(it, mode_names[app->net_cfg.mode]);
    /* rebuilt from the scene, not from inside the list's own callback */
    view_dispatcher_send_custom_event(app->view_dispatcher, EVT_IF_MODE);
}

static void if_add(
    SshRunnerApp* app,
    uint8_t row,
    const char* label,
    uint8_t count,
    VariableItemChangeCallback cb,
    VariableItem** out) {
    *out = variable_item_list_add(app->var_list, label, count, cb, app);
    if_rows[if_row_count++] = row;
}

/* "Static IP 192.168.1.50" in the label, no value column: drawn whole, no marquee */
static void if_ip_item(SshRunnerApp* app, uint8_t row, const char* label, uint32_t ip, char* buf) {
    char ipt[16];
    if(ip)
        ip4_format(ip, ipt, sizeof(ipt));
    else
        snprintf(ipt, sizeof(ipt), "-");
    util_label_value(app->var_list, buf, sizeof(if_vals[0]), label, ipt, app);
    if_rows[if_row_count++] = row;
}

static void if_build(SshRunnerApp* app) {
    Session* s = app->session;
    const ProtoIfaceStatus* st = app->net_iface == ProtoIfaceLan ? &s->net.lan : &s->net.wifi;
    VariableItemList* vl = app->var_list;
    variable_item_list_reset(vl);
    variable_item_list_set_enter_callback(vl, util_var_list_enter_cb, app);
    if_row_count = 0;
    VariableItem* it;
    /* the live address in the label with no value: the stock list then draws the label
     * whole (a value is cut to 37 px, "192.168.1" read as another address) */
    static char if_status[64];
    if(s->link.state == BackendLinkUp && (st->flags & PROTO_NET_IP)) {
        char ip[16];
        ip4_format(st->ip, ip, sizeof(ip));
        util_label_flush(if_status, sizeof(if_status), "Address", ip);
    } else {
        snprintf(
            if_status,
            sizeof(if_status),
            "%s",
            s->link.state != BackendLinkUp   ? "No board" :
            !(st->flags & PROTO_NET_PRESENT) ? "Not present" :
            !(st->flags & PROTO_NET_LINK)    ? (app->net_iface == ProtoIfaceLan ? "No cable" :
                                                                                  "Not joined") :
                                               "Waiting for an address");
    }
    if_add(app, IfAddress, if_status, 1, NULL, &it);
    if_add(app, IfMode, "Mode", 2, if_mode_changed, &it);
    variable_item_set_current_value_index(it, app->net_cfg.mode);
    variable_item_set_current_value_text(it, mode_names[app->net_cfg.mode]);
    if(app->net_cfg.mode == ProtoIpStatic) {
        if_ip_item(app, IfIp, "Static IP", app->net_cfg.ip, if_vals[1]);
        if_ip_item(app, IfMask, "Netmask", app->net_cfg.mask, if_vals[2]);
        if_ip_item(app, IfGateway, "Gateway", app->net_cfg.gw, if_vals[3]);
        if_ip_item(app, IfDns, "DNS", app->net_cfg.dns, if_vals[4]);
    }
    /* up with a gateway: Ping, prefilled with it, through this interface alone */
    if(s->link.state == BackendLinkUp && (st->flags & PROTO_NET_IP) && st->gw)
        if_add(app, IfPingGw, "Ping", 1, NULL, &it);
    if_add(app, IfApply, "Apply", 1, NULL, &it);
}

void ssh_runner_scene_net_iface_on_enter(void* context) {
    SshRunnerApp* app = context;
    if(scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneNetIface) == 0) {
        /* first entry: start from what the interface has now */
        Session* s = app->session;
        const ProtoIfaceStatus* st = app->net_iface == ProtoIfaceLan ? &s->net.lan : &s->net.wifi;
        memset(&app->net_cfg, 0, sizeof(app->net_cfg));
        app->net_cfg.iface = app->net_iface;
        app->net_cfg.mode = st->mode;
        app->net_cfg.ip = st->ip;
        app->net_cfg.mask = st->mask ? st->mask : 0xFFFFFF00;
        app->net_cfg.gw = st->gw;
        app->net_cfg.dns = st->gw;
        scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneNetIface, IfMode + 1);
    }
    if_applying = false;
    if_build(app);
    uint32_t row = scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneNetIface) - 1;
    variable_item_list_set_selected_item(app->var_list, if_position((uint8_t)row));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewVarList);
}

static void start_ip_input(SshRunnerApp* app, InputTarget t, uint32_t ip) {
    char buf[16] = "";
    if(ip) ip4_format(ip, buf, sizeof(buf));
    util_start_input(app, t, buf);
}

bool ssh_runner_scene_net_iface_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == EvtChanged) {
        uint32_t chg = app_take_changes(app);
        if(if_applying && (chg & APP_CHG_OPDONE)) {
            if_applying = false;
            app_notify_success(app);
            app_show_message(
                app,
                "Applied",
                app->net_cfg.mode == ProtoIpDhcp ? "Using DHCP" : "Static address set",
                SshRunnerSceneNetwork);
        } else if(if_applying && (chg & APP_CHG_ERROR)) {
            if_applying = false;
            app_notify_error(app);
            app_show_error(
                app,
                "Not Applied",
                app->last_error.msg[0] ? app->last_error.msg : "ESP32 not responding",
                0);
        } else if(!if_applying && (chg & SESSION_CHG_NET)) {
            uint8_t row = if_rows[variable_item_list_get_selected_item_index(app->var_list)];
            if_build(app);
            variable_item_list_set_selected_item(app->var_list, if_position(row));
        }
        return true;
    }
    if(event.event == EVT_IF_MODE) {
        if_build(app);
        variable_item_list_set_selected_item(app->var_list, if_position(IfMode));
        return true;
    }
    if(event.event >= EvtBase || event.event >= if_row_count) return false;
    uint8_t row = if_rows[event.event];
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneNetIface, row + 1);
    switch(row) {
    case IfPingGw: {
        const ProtoIfaceStatus* st = app->net_iface == ProtoIfaceLan ? &app->session->net.lan :
                                                                       &app->session->net.wifi;
        app->ping_via = app->net_iface;
        start_ip_input(app, InputPingHost, st->gw);
        break;
    }
    case IfIp:
        start_ip_input(app, InputIp, app->net_cfg.ip);
        break;
    case IfMask:
        start_ip_input(app, InputMask, app->net_cfg.mask);
        break;
    case IfGateway:
        start_ip_input(app, InputGateway, app->net_cfg.gw);
        break;
    case IfDns:
        start_ip_input(app, InputDns, app->net_cfg.dns);
        break;
    case IfApply:
        if(app->net_cfg.mode == ProtoIpStatic && (!app->net_cfg.ip || !app->net_cfg.mask)) {
            app_notify_error(app);
            app_show_error(app, "Incomplete", "Set Static IP and Netmask", 0);
            break;
        }
        app->net_cfg.iface = app->net_iface;
        if(app->backend.ops->net_set_cfg(app->backend.self, &app->net_cfg)) {
            if_applying = true;
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewLoading);
        } else {
            app_notify_error(app);
            app_show_error(app, "Not Applied", "ESP32 busy or not connected", 0);
        }
        break;
    default:
        break;
    }
    return true;
}

void ssh_runner_scene_net_iface_on_exit(void* context) {
    SshRunnerApp* app = context;
    variable_item_list_reset(app->var_list);
}

/* ---------------- WiFi scan & join ----------------
 * scene state: 0 = start scan, 1 = list, 2 = joining */

static int wifi_pct(int8_t rssi) {
    int pct = 2 * (rssi + 100); /* -100 dBm = 0 %, -50 dBm and better = 100 % */
    return pct < 0 ? 0 : pct > 100 ? 100 : pct;
}

/* The signal in its own column, flush right, not glued to the SSID: four bars and a lock
 * for secured networks, as wireless UIs show them. */
static void wifi_list(SshRunnerApp* app) {
    if(!app->scan || app->scan->count == 0) {
        Submenu* sm = app->submenu;
        util_submenu_reset(sm);
        submenu_set_header(sm, "Choose Network");
        util_submenu_add(sm, "No networks found", 900, util_submenu_cb, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
        return;
    }
    /* ~600 B most runs never need: allocated on first use and kept (never removed while it
     * may be the current view) until the app exits */
    if(!app->wifi_list) {
        app->wifi_list = wifi_list_view_alloc();
        view_dispatcher_add_view(
            app->view_dispatcher, ViewWifiList, wifi_list_view_get_view(app->wifi_list));
    }
    wifi_list_view_reset(app->wifi_list, "Choose Network");
    for(uint8_t i = 0; i < app->scan->count && i < WIFI_LIST_MAX; i++) {
        const ProtoScanEntry* e = &app->scan->entries[i];
        int pct = wifi_pct(e->rssi);
        uint8_t bars = pct >= 75 ? 4 : pct >= 50 ? 3 : pct >= 25 ? 2 : pct > 0 ? 1 : 0;
        wifi_list_view_add(app->wifi_list, e->ssid, bars, e->auth != 0);
    }
    wifi_list_view_set_callback(app->wifi_list, util_submenu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWifiList);
}

static char wifi_text[64];

void ssh_runner_scene_wifi_scan_on_enter(void* context) {
    SshRunnerApp* app = context;
    uint32_t st = scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneWifiScan);
    if(st == 0) {
        if(app->scan) scan_list_reset(app->scan);
        if(!app->backend.ops->wifi_scan(app->backend.self)) {
            app_notify_error(app);
            app_show_error(
                app, "Cannot Scan", "ESP32 busy or not connected", SshRunnerSceneNetwork);
            return;
        }
        util_progress_start(app, "WiFi", false, UTIL_SPINNER, "Scanning...", "Back cancels");
    } else if(st == 2) {
        text_ascii(app->wifi_ssid, wifi_text, sizeof(wifi_text), TEXT_TOFU);
        app_blink_start(app);
        util_progress_start(app, wifi_text, true, UTIL_SPINNER, "Joining...", "Back cancels");
    } else {
        wifi_list(app);
    }
}

static void wifi_join_open(SshRunnerApp* app) {
    app->join_known = false;
    ProtoWifiJoin j;
    memset(&j, 0, sizeof(j));
    snprintf(j.ssid, sizeof(j.ssid), "%s", app->wifi_ssid);
    bool ok = app->backend.ops->wifi_join(app->backend.self, &j);
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneWifiScan, ok ? 2 : 1);
    ssh_runner_scene_wifi_scan_on_enter(app);
}

bool ssh_runner_scene_wifi_scan_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    uint32_t st = scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneWifiScan);
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == EvtChanged) {
        uint32_t chg = app_take_changes(app);
        if(st == 0 || st == 2) util_progress_tick(app); /* scanning, joining: the spinner */
        if(st == 0 && (chg & APP_CHG_SCAN) && app->scan && app->scan->done) {
            util_progress_stop(app);
            scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneWifiScan, 1);
            wifi_list(app);
        } else if(st == 2 && (chg & SESSION_CHG_NET)) {
            util_progress_stop(app);
            app_blink_stop(app);
            scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneWifiScan, 1);
            Session* s = app->session;
            if(session_iface_up(s, ProtoIfaceWifi) &&
               strcmp(s->net.wifi_ssid, app->wifi_ssid) == 0) {
                /* address first, as in Network; address and SSID are technical (FontKeyboard) */
                char mono[PROTO_STR_SSID + 20]; /* copied by the message scene */
                char ip[16];
                ip4_format(s->net.wifi.ip, ip, sizeof(ip));
                char ssid[PROTO_STR_SSID + 1];
                snprintf(
                    mono,
                    sizeof(mono),
                    "%s\n%s",
                    ip,
                    text_ascii(app->wifi_ssid, ssid, sizeof(ssid), TEXT_TOFU));
                app_notify_success(app);
                app_show_message_mono(app, "WiFi Connected", NULL, mono, SshRunnerSceneNetwork);
            } else {
                /* as phones say it, from the board's reason; an older board gives none */
                const ProtoJoinResult r = app->join_known ? (ProtoJoinResult)app->join.result :
                                                            ProtoJoinFailed;
                app_notify_error(app);
                if(r == ProtoJoinWrongPassword)
                    app_show_error(app, "Wrong Password", "The network refused\nthe password", 0);
                else if(r == ProtoJoinNotFound)
                    app_show_error(app, "Network Not Found", "Out of range or\nswitched off", 0);
                else if(app->join_known) {
                    /* the board's reason code, as an OS shows an error code: it tells a
                     * weak signal from a router that turns the board away */
                    char why[48]; /* copied by the message scene */
                    snprintf(
                        why,
                        sizeof(why),
                        "Check the signal\nand try again (code %u)",
                        app->join.reason);
                    app_show_error(app, "Could Not Join", why, 0);
                } else
                    app_show_error(app, "Could Not Join", "Check the password\nand signal", 0);
            }
        } else if(chg & APP_CHG_ERROR) {
            app_blink_stop(app);
            app_notify_error(app);
            app_show_error(app, "No Answer", "ESP32 not responding", SshRunnerSceneNetwork);
        }
        return true;
    }
    if(st != 1 || !app->scan || event.event >= app->scan->count) return true;
    const ProtoScanEntry* e = &app->scan->entries[event.event];
    snprintf(app->wifi_ssid, sizeof(app->wifi_ssid), "%s", e->ssid);
    if(e->auth)
        util_start_input(app, InputWifiPassword, NULL);
    else
        wifi_join_open(app);
    return true;
}

void ssh_runner_scene_wifi_scan_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_progress_stop(app);
    app_blink_stop(app);
    util_submenu_reset(app->submenu);
    progress_view_reset(app->progress);
    if(app->wifi_list) wifi_list_view_reset(app->wifi_list, NULL);
}

/* ---------------- Ping ----------------
 * From the Network menu: every interface with an address, one after the other, each bound to
 * its own interface, so the result says which of them reach the host. From LAN or WiFi:
 * that interface alone. */

static char ping_host[PROTO_STR_HOST + 1];

/* where the ping was started: the Network menu, or LAN / WiFi */
static uint32_t ping_home(SshRunnerApp* app) {
    return app->ping_via == PING_VIA_ALL ? SshRunnerSceneNetwork : SshRunnerSceneNetIface;
}

static const char* iface_name(uint8_t iface) {
    return iface == ProtoIfaceLan ? "LAN" : "WiFi";
}

/* Starts the next interface still to do; false when none is left. */
static bool ping_next(SshRunnerApp* app) {
    for(uint8_t i = 0; i < 2; i++) {
        if(!(app->ping_todo & (1u << i))) continue;
        app->ping_todo &= (uint8_t) ~(1u << i);
        ProtoPingReq r;
        memset(&r, 0, sizeof(r));
        snprintf(r.host, sizeof(r.host), "%s", ping_host);
        r.count = 4;
        r.transport = i == ProtoIfaceLan ? ProtoTransportLan : ProtoTransportWifi;
        app->ping_done = false;
        if(!app->backend.ops->ping(app->backend.self, &r)) return false;
        char status[24];
        snprintf(status, sizeof(status), "Pinging via %s...", iface_name(i));
        util_progress_set(app, NULL, status);
        return true;
    }
    return false;
}

static void ping_show(SshRunnerApp* app) {
    char* lines = util_scratch;
    size_t n = 0;
    bool any = false;
    lines[0] = 0;
    /* plain words: no "4/4" or "min/avg/max" in a proportional font */
    for(uint8_t i = 0; i < 2; i++) {
        if(!(app->ping_have & (1u << i))) continue;
        const ProtoPingResp* r = &app->ping_res[i];
        if(r->received) {
            any = true;
            n += snprintf(
                lines + n,
                UTIL_SCRATCH_SIZE - n,
                "%s%s: %u of %u, average %u ms",
                n ? "\n" : "",
                iface_name(i),
                r->received,
                r->sent,
                r->avg_ms);
        } else {
            n += snprintf(
                lines + n, UTIL_SCRATCH_SIZE - n, "%s%s: no reply", n ? "\n" : "", iface_name(i));
        }
        if(n >= UTIL_SCRATCH_SIZE) break;
    }
    util_text_screen(app, "Ping", !any, lines, ping_host, true, NULL, "OK", NULL);
    if(any)
        app_notify_success(app);
    else
        app_notify_error(app);
}

void ssh_runner_scene_ping_on_enter(void* context) {
    SshRunnerApp* app = context;
    Session* s = app->session;
    snprintf(ping_host, sizeof(ping_host), "%s", app->input);
    app->ping_todo = 0;
    app->ping_have = 0;
    if(s->link.state == BackendLinkUp) {
        if((app->ping_via == PING_VIA_ALL || app->ping_via == ProtoIfaceLan) &&
           (s->net.lan.flags & PROTO_NET_IP))
            app->ping_todo |= 1u << ProtoIfaceLan;
        if((app->ping_via == PING_VIA_ALL || app->ping_via == ProtoIfaceWifi) &&
           (s->net.wifi.flags & PROTO_NET_IP))
            app->ping_todo |= 1u << ProtoIfaceWifi;
    }
    if(!app->ping_todo) {
        app_notify_error(app);
        app_show_error(
            app,
            "Cannot Ping",
            s->link.state != BackendLinkUp ? "ESP32 not connected" :
            app->ping_via == PING_VIA_ALL  ? "No network: LAN and WiFi have no address" :
                                             "This interface has no address",
            ping_home(app));
        return;
    }
    util_progress_start(app, ping_host, true, UTIL_SPINNER, "Pinging...", "Back cancels");
    if(!ping_next(app)) {
        util_progress_stop(app);
        app_notify_error(app);
        app_show_error(app, "Ping Failed", "ESP32 busy or not connected", ping_home(app));
    }
}

bool ssh_runner_scene_ping_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        if(app->ping_todo || !app->ping_done) app->backend.ops->cancel(app->backend.self);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, ping_home(app));
        return true;
    }
    if(event.type == SceneManagerEventTypeCustom && event.event == EVT_DIALOG_CENTER) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, ping_home(app));
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom || event.event != EvtChanged) return false;
    uint32_t chg = app_take_changes(app);
    if(chg & APP_CHG_PING) {
        uint8_t i = app->ping.iface < 2 ? app->ping.iface : 0;
        app->ping_res[i] = app->ping;
        app->ping_have |= (uint8_t)(1u << i);
        if(ping_next(app)) return true;
        util_progress_stop(app);
        ping_show(app);
    } else if(chg & APP_CHG_ERROR) {
        util_progress_stop(app);
        app_notify_error(app);
        app_show_error(app, "No Answer", "ESP32 not responding", ping_home(app));
    } else {
        util_progress_tick(app);
    }
    return true;
}

void ssh_runner_scene_ping_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_progress_stop(app);
    progress_view_reset(app->progress);
    text_view_reset(app->text_view);
}

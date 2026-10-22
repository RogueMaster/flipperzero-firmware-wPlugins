#include "../sigroam.h"

#include "../src/sr_dialect.h"
#include "../src/sr_marauder_join.h"
#include "../src/sr_marauder_upload.h"
#include "../src/sr_scan_ctl.h"
#include "../src/sr_uart_cap.h"

#include <stdio.h>
#include <string.h>

/*
 * Factory Marauder only. Joins the selected Wi-Fi with `join -a -p`.
 * Marauder manages up to five saved profiles. Does not send `upload` itself.
 *
 * Sequence: stop, scanall for one full dual-band sweep (51 x 250 ms),
 * stop, then list -a. Stopping at the first AP only covers channels 1..8.
 * The post-stop list is the one shown. StopScan does not clear access_points.
 *
 * Connected is not a timer. The next free command slot sends `info` as a
 * non-disconnecting barrier. No failure before its echo plus the selected
 * `Using SSID:` implies joinWiFi reached WL_CONNECTED in v1.17.0. The CLI
 * does not report whether the credential was persisted.
 */

enum {
    JoinEvPrev = 1,
    JoinEvNext = 2,
    JoinEvPick = 3,
    JoinEvPass = 4,
};

enum {
    PhBlocked = 0,
    PhStop1,
    PhScan,
    PhList1,
    PhStop2,
    PhList2,
    PhPick,
    PhPass,
    PhVerify,
    PhJoin,
    PhJoined,
    PhFail,
};

typedef struct {
    uint16_t index;
    char ssid[SR_JOIN_SSID_MAX + 1u];
} JoinAp;

static unsigned s_phase;
static uint32_t s_phase_ms;
static uint32_t s_stop_rev;
static uint32_t s_hunt_ms;
static bool s_need_send;
static bool s_check_queued;
static bool s_check_sent;
static char s_pending[SR_WORKER_CMD_MAX];
static char s_psk[SR_JOIN_PASS_MAX + 1u];
static char s_why[24];
static JoinAp s_shown[SR_JOIN_AP_CAP];
static size_t s_shown_n;
static JoinAp s_second[SR_JOIN_AP_CAP];
static size_t s_cursor;
static uint32_t s_draw_key;

static void join_wipe(char* p, size_t n) {
    size_t i;

    if(p == NULL) {
        return;
    }
    for(i = 0u; i < n; i++) {
        p[i] = '\0';
    }
}

static void join_fail(SigRoamApp* app, const char* why) {
    size_t i;

    s_phase = PhFail;
    s_need_send = false;
    join_wipe(s_psk, sizeof(s_psk));
    join_wipe(s_pending, sizeof(s_pending));
    for(i = 0u; i < sizeof(s_why); i++) {
        s_why[i] = '\0';
    }
    if(why != NULL) {
        for(i = 0u; i + 1u < sizeof(s_why) && why[i] != '\0'; i++) {
            s_why[i] = why[i];
        }
    }
    sr_join_listen(SrJoinListenOff);
    sr_uart_cap_mark("fap-not-connected");
    (void)app;
}

static void join_queue(const char* cmd) {
    size_t n = 0u;

    if(cmd == NULL) {
        s_need_send = false;
        return;
    }
    while(cmd[n] != '\0') {
        n++;
    }
    if(n == 0u || n >= sizeof(s_pending)) {
        s_need_send = false;
        return;
    }
    memcpy(s_pending, cmd, n);
    s_pending[n] = '\0';
    s_need_send = true;
}

static void join_copy_listen(JoinAp* dst, size_t* nout) {
    size_t n;
    size_t i;

    n = sr_join_ap_count();
    if(n > (size_t)SR_JOIN_AP_CAP) {
        n = (size_t)SR_JOIN_AP_CAP;
    }
    for(i = 0u; i < n; i++) {
        if(!sr_join_ap_at(i, &dst[i].index, dst[i].ssid, sizeof(dst[i].ssid))) {
            n = i;
            break;
        }
    }
    *nout = n;
}

static bool join_flush(SigRoamApp* app) {
    if(!s_need_send) {
        return true;
    }
    if(app->worker == NULL || !sr_worker_send_cmd(app->worker, s_pending)) {
        return false;
    }
    s_need_send = false;
    join_wipe(s_pending, sizeof(s_pending));
    return true;
}

static void join_button(GuiButtonType result, InputType type, void* context);

static void join_draw(SigRoamApp* app) {
    char text[96];
    uint32_t key;
    const char* ssid;

    if(s_phase == PhPass) {
        return;
    }
    text[0] = '\0';
    if(s_phase == PhBlocked) {
        snprintf(text, sizeof(text), "Stop scan first\nthen WiFi");
    } else if(s_phase == PhStop1 || s_phase == PhStop2) {
        snprintf(text, sizeof(text), "Stopping");
    } else if(s_phase == PhScan) {
        snprintf(text, sizeof(text), "Scanning");
    } else if(s_phase == PhList1 || s_phase == PhList2) {
        snprintf(text, sizeof(text), "Listing");
    } else if(s_phase == PhVerify) {
        snprintf(text, sizeof(text), "Checking AP");
    } else if(s_phase == PhJoin) {
        snprintf(text, sizeof(text), "Joining");
    } else if(s_phase == PhJoined) {
        /* v1.17.0 saveWifiCredential: a new SSID only fits under five profiles. */
        snprintf(text, sizeof(text), "Connected\nWiFi active\nSaved if slot free");
    } else if(s_phase == PhFail) {
        snprintf(text, sizeof(text), "Not connected\n%s", s_why[0] != '\0' ? s_why : "failed");
    } else if(s_shown_n == 0u) {
        snprintf(text, sizeof(text), "No AP");
    } else {
        ssid = s_shown[s_cursor].ssid;
        if(ssid[0] == '\0') {
            ssid = "(hidden)";
        }
        snprintf(
            text,
            sizeof(text),
            "Join WiFi\n%u/%u\n%.18s",
            (unsigned)(s_cursor + 1u),
            (unsigned)s_shown_n,
            ssid);
    }
    key = s_phase * 1000u + (uint32_t)s_cursor + (uint32_t)s_shown_n * 50u;
    {
        const char* p = text;
        while(*p != '\0') {
            key = (key ^ (uint8_t)*p) * 16777619u;
            p++;
        }
    }
    if(key == s_draw_key) {
        return;
    }
    s_draw_key = key;
    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget, 0, 0, SR_CANVAS_W, (uint8_t)(SR_CANVAS_H - 14), text);
    if(s_phase == PhPick && s_shown_n > 0u) {
        widget_add_button_element(app->widget, GuiButtonTypeLeft, "Prev", join_button, app);
        widget_add_button_element(app->widget, GuiButtonTypeCenter, "OK", join_button, app);
        widget_add_button_element(app->widget, GuiButtonTypeRight, "Next", join_button, app);
    }
}

static void join_button(GuiButtonType result, InputType type, void* context) {
    SigRoamApp* app = context;
    uint32_t ev;

    if(app == NULL || type != InputTypeShort || s_phase != PhPick) {
        return;
    }
    if(result == GuiButtonTypeLeft) {
        ev = JoinEvPrev;
    } else if(result == GuiButtonTypeRight) {
        ev = JoinEvNext;
    } else if(result == GuiButtonTypeCenter) {
        ev = JoinEvPick;
    } else {
        return;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, ev);
}

static bool join_psk_validator(const char* text, FuriString* error, void* context) {
    (void)context;
    if(!sr_join_password_ok(text)) {
        if(error != NULL) {
            furi_string_set(error, "bad password");
        }
        return false;
    }
    return true;
}

static void join_psk_done(void* context) {
    SigRoamApp* app = context;

    if(app != NULL) {
        view_dispatcher_send_custom_event(app->view_dispatcher, JoinEvPass);
    }
}

static void join_show_pass(SigRoamApp* app) {
    s_phase = PhPass;
    s_draw_key = 0u;
    join_wipe(s_psk, sizeof(s_psk));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "WiFi password");
    text_input_set_minimum_length(app->text_input, (size_t)SR_JOIN_PASS_MIN);
    text_input_set_result_callback(
        app->text_input, join_psk_done, app, s_psk, sizeof(s_psk), false);
    text_input_set_validator(app->text_input, join_psk_validator, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewTextInput);
}

static void join_show_widget(SigRoamApp* app) {
    s_draw_key = 0u;
    join_draw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewWidget);
}

static void join_arm(unsigned phase, unsigned listen, const char* cmd, uint32_t now) {
    s_phase = phase;
    s_phase_ms = now;
    sr_join_listen(listen);
    join_queue(cmd);
}

static bool join_aged(uint32_t now, uint32_t ms) {
    return (uint32_t)(now - s_phase_ms) >= ms;
}

static void join_use_shown_from(const JoinAp* src, size_t n) {
    size_t i;

    if(n > (size_t)SR_JOIN_AP_CAP) {
        n = (size_t)SR_JOIN_AP_CAP;
    }
    for(i = 0u; i < n; i++) {
        s_shown[i] = src[i];
    }
    s_shown_n = n;
    s_cursor = 0u;
}

void sigroam_scene_join_on_enter(void* context) {
    SigRoamApp* app = context;

    s_need_send = false;
    s_check_queued = false;
    s_check_sent = false;
    s_shown_n = 0u;
    s_cursor = 0u;
    s_draw_key = 0u;
    s_why[0] = '\0';
    join_wipe(s_psk, sizeof(s_psk));
    join_wipe(s_pending, sizeof(s_pending));
    sr_join_listen(SrJoinListenOff);
    if(!sr_dialect_is_generic_marauder(&app->model.firmware) ||
       app->model.session == SrSessionRunning) {
        s_phase = PhBlocked;
    } else {
        s_stop_rev = app->model.wifi_stop_rev;
        join_arm(PhStop1, SrJoinListenOff, sr_join_cmd_stop(), furi_get_tick());
    }
    join_show_widget(app);
}

bool sigroam_scene_join_on_event(void* context, SceneManagerEvent event) {
    SigRoamApp* app = context;
    uint32_t now;

    if(event.type == SceneManagerEventTypeBack) {
        if(s_phase == PhPass) {
            s_phase = PhPick;
            join_wipe(s_psk, sizeof(s_psk));
            join_show_widget(app);
            return true;
        }
        return false;
    }
    if(event.type == SceneManagerEventTypeCustom) {
        if(s_phase == PhPick && s_shown_n > 0u && event.event == JoinEvPrev) {
            s_cursor = (s_cursor == 0u) ? (s_shown_n - 1u) : (s_cursor - 1u);
            join_draw(app);
            return true;
        }
        if(s_phase == PhPick && s_shown_n > 0u && event.event == JoinEvNext) {
            s_cursor = (s_cursor + 1u) % s_shown_n;
            join_draw(app);
            return true;
        }
        if(s_phase == PhPick && event.event == JoinEvPick) {
            join_show_pass(app);
            return true;
        }
        if(s_phase == PhPass && event.event == JoinEvPass) {
            if(!sr_join_password_ok(s_psk)) {
                join_fail(app, "Bad password");
                join_show_widget(app);
                return true;
            }
            join_arm(PhVerify, SrJoinListenList, sr_join_cmd_list(), furi_get_tick());
            join_show_widget(app);
            return true;
        }
        return false;
    }
    if(event.type != SceneManagerEventTypeTick) {
        return false;
    }
    now = furi_get_tick();
    if(s_phase == PhPass || s_phase == PhBlocked || s_phase == PhJoined || s_phase == PhFail ||
       s_phase == PhPick) {
        return true;
    }
    if(s_phase == PhJoin && sr_join_saw_using() &&
       !sr_join_using_matches(s_shown[s_cursor].ssid)) {
        join_fail(app, "AP changed");
        join_draw(app);
        return true;
    }
    if(s_phase == PhJoin && sr_join_receipt() == SrJoinReceiptFail) {
        join_fail(app, sr_join_fail_why(SrJoinReceiptFail));
        join_draw(app);
        return true;
    }
    if(s_phase == PhJoin && join_aged(now, (uint32_t)SR_JOIN_CHECK_TIMEOUT_MS)) {
        join_fail(app, "No join receipt");
        join_draw(app);
        return true;
    }
    if(!join_flush(app)) {
        join_draw(app);
        return true;
    }
    if(s_phase == PhStop1) {
        if(app->model.wifi_stop_rev != s_stop_rev ||
           join_aged(now, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
            s_hunt_ms = now;
            join_arm(PhScan, SrJoinListenScan, sr_join_cmd_scan(), now);
        }
    } else if(s_phase == PhScan) {
        if(sr_join_saw_scan() || join_aged(now, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
            join_arm(PhList1, SrJoinListenList, sr_join_cmd_list(), now);
        }
    } else if(s_phase == PhList1) {
        if(sr_join_saw_list_end() || join_aged(now, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
            /* Keep scanning for a full dual-band sweep before the final stop and list. */
            if((uint32_t)(now - s_hunt_ms) < (uint32_t)SR_JOIN_SWEEP_MS) {
                s_phase = PhScan;
                s_phase_ms = now;
                s_need_send = false;
                sr_join_listen(SrJoinListenOff);
            } else {
                s_stop_rev = app->model.wifi_stop_rev;
                join_arm(PhStop2, SrJoinListenOff, sr_join_cmd_stop(), now);
            }
        }
    } else if(s_phase == PhStop2) {
        if(app->model.wifi_stop_rev != s_stop_rev ||
           join_aged(now, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
            join_arm(PhList2, SrJoinListenList, sr_join_cmd_list(), now);
        }
    } else if(s_phase == PhList2) {
        if(sr_join_saw_list_end() || join_aged(now, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
            size_t second_n = 0u;

            join_copy_listen(s_second, &second_n);
            sr_join_listen(SrJoinListenOff);
            if(second_n == 0u) {
                join_fail(app, "No fresh AP list");
            } else {
                join_use_shown_from(s_second, second_n);
                s_phase = PhPick;
            }
        }
    } else if(s_phase == PhVerify) {
        if(sr_join_saw_list_end()) {
            if(!sr_join_ap_matches(s_shown[s_cursor].index, s_shown[s_cursor].ssid)) {
                join_fail(app, "AP changed");
            } else if(
                sr_join_build(s_shown[s_cursor].index, s_psk, s_pending, sizeof(s_pending)) ==
                0u) {
                join_fail(app, "Bad password");
            } else {
                join_wipe(s_psk, sizeof(s_psk));
                s_need_send = true;
                s_check_queued = false;
                s_check_sent = false;
                s_phase = PhJoin;
                s_phase_ms = now;
                sr_uart_cap_arm(true);
                sr_uart_cap_mark("join");
                sr_join_listen(SrJoinListenJoin);
                sr_join_expect_index(s_shown[s_cursor].index);
            }
        } else if(join_aged(now, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
            join_fail(app, "List timeout");
        }
    } else if(s_phase == PhJoin) {
        unsigned receipt = sr_join_receipt();

        if(!s_check_sent) {
            if(!s_check_queued) {
                sr_join_expect_check();
                join_queue(sr_join_cmd_check());
                s_check_queued = true;
            } else if(!s_need_send) {
                s_check_sent = true;
            }
        } else if(receipt == SrJoinReceiptJoined) {
            sr_join_listen(SrJoinListenOff);
            sr_uart_cap_mark("fap-connected");
            sr_mau_up_note_joined(s_shown[s_cursor].ssid);
            s_phase = PhJoined;
        }
    }
    join_draw(app);
    return true;
}

void sigroam_scene_join_on_exit(void* context) {
    SigRoamApp* app = context;

    if(app != NULL && app->mtx != NULL &&
       furi_mutex_acquire(app->mtx, FuriWaitForever) == FuriStatusOk) {
        sr_join_listen(SrJoinListenOff);
        furi_mutex_release(app->mtx);
    } else {
        sr_join_listen(SrJoinListenOff);
    }
    s_need_send = false;
    join_wipe(s_psk, sizeof(s_psk));
    join_wipe(s_pending, sizeof(s_pending));
    if(app != NULL) {
        widget_reset(app->widget);
        if(app->text_input != NULL) {
            text_input_reset(app->text_input);
        }
    }
}

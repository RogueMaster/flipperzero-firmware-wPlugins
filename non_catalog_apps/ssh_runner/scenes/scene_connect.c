/* Connecting progress, host-key prompt (policy: docs/SECURITY.md) and its options.
 * SPDX-License-Identifier: GPL-3.0-only */
#include "scene_util.h"
#include <ssh_runner_icons.h>
#include <furi_hal_rtc.h>

enum {
    ConnectStateNew = 0,
    ConnectStateRunning = 1,
    ConnectStateFailed = 2,
};

static char conn_header[40];

static void connect_progress(SshRunnerApp* app) {
    char label[48];
    util_profile_label(&app->profile, label, sizeof(label));
    snprintf(conn_header, sizeof(conn_header), "%s", label); /* the view fits it with "..." */
    /* the alias or user@host: a name, so FontKeyboard like everywhere else */
    util_progress_start(
        app, conn_header, true, UTIL_SPINNER, session_status_text(app->session), "Back cancels");
}

static size_t key_read(void* ctx, uint32_t offset, uint8_t* buf, size_t len) {
    SshRunnerApp* app = ctx;
    if(!app->key_file || !storage_file_seek(app->key_file, offset, true)) return 0;
    return storage_file_read(app->key_file, buf, len);
}

static void close_key(SshRunnerApp* app) {
    if(app->key_file) {
        storage_file_close(app->key_file);
        storage_file_free(app->key_file);
        app->key_file = NULL;
    }
}

/* Why a key host cannot connect before anything is sent; false if it can. */
static bool key_unusable(SshRunnerApp* app, const KeyEntry* k, bool missing) {
    const HostProfile* p = &app->profile;
    char* m = app->session->msg;
    size_t ms = sizeof(app->session->msg);
    const char* name = missing ? profile_key_name(p, app->settings.default_key) : k->name;
    /* the file name goes under the host in FontKeyboard (show_failure) */
    snprintf(app->key_name, sizeof(app->key_name), "%s", name ? name : "");
    if(missing && !name)
        snprintf(m, ms, "No default key is set.");
    else if(missing)
        snprintf(m, ms, "Key file missing.");
    else if(k->problem)
        snprintf(m, ms, "%s", k->problem);
    else
        return false;
    app->key_failed = true;
    return true;
}

static bool start_ssh(SshRunnerApp* app) {
    ProtoSshConnect c;
    memset(&c, 0, sizeof(c));
    profile_id(&app->profile, c.profile, sizeof(c.profile));
    snprintf(c.user, sizeof(c.user), "%s", app->profile.target.user);
    snprintf(c.host, sizeof(c.host), "%s", app->profile.target.host);
    c.port = app->profile.target.port;
    c.transport = app->profile.transport;
    c.auth = profile_wire_auth(&app->profile);
    c.policy = app->profile.policy;
    /* Remembering needs a saved alias to key the secret by. */
    c.flags = (app->profile.remember && app->profile_saved && c.auth == ProtoAuthPassword) ?
                  PROTO_CONN_REMEMBER :
                  0;
    /* Quick Connect on the default key: a refused key falls back to the password, as in
     * OpenSSH; saved hosts stay strict */
    if(!app->profile_saved && app->profile.auth == ProfileAuthDefault)
        c.flags |= PROTO_CONN_KEY_FALLBACK;
    session_reset(app->session);
    close_key(app);
    app->key_failed = false;
    if(c.auth == ProtoAuthKey) {
        bool missing;
        const KeyEntry* k = keys_for_profile(app, &app->profile, &missing);
        if(key_unusable(app, k, missing)) return false;
        uint32_t size = 0;
        app->key_file = store_key_open(app, k->name, &size);
        if(!app->key_file) {
            snprintf(app->session->msg, sizeof(app->session->msg), "Cannot read keys/%s", k->name);
            return false;
        }
        if(k->pub_known) snprintf(c.key_id, sizeof(c.key_id), "%s", k->fp);
        snprintf(
            app->key_name, sizeof(app->key_name), "%s", k->name); /* passphrase prompt title */
        if(!k->has_pub) {
            /* the ESP32 returns its public key; the app writes <name>.pub */
            c.flags |= PROTO_CONN_WANT_PUB;
            app->key_job = KeyJobConnectPub;
        }
        if(!app->backend.ops->ssh_connect_key(app->backend.self, &c, size, key_read, app)) {
            close_key(app);
            snprintf(app->session->msg, sizeof(app->session->msg), "ESP32 busy or not connected");
            return false;
        }
    } else if(!app->backend.ops->ssh_connect(app->backend.self, &c)) {
        snprintf(app->session->msg, sizeof(app->session->msg), "ESP32 busy or not connected");
        return false;
    }
    app->session->phase = SessionConnecting;
    app->session->ssh_state = ProtoSshConnecting;
    return true;
}

static void show_failure(SshRunnerApp* app) {
    scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneConnect, ConnectStateFailed);
    app_blink_stop(app);
    app_notify_error(app);
    static char text[PROTO_STR_MSG + 1];
    static char target[48 + KEY_NAME_MAX + 8];
    snprintf(text, sizeof(text), "%s", session_failure_text(app->session));
    util_profile_label(&app->profile, target, sizeof(target));
    if(app->key_failed && app->key_name[0]) {
        /* which key file: under the host, in FontKeyboard */
        size_t n = strlen(target);
        snprintf(target + n, sizeof(target) - n, "\nkeys/%s", app->key_name);
    }
    /* a missing key file is fixed in the host's Auth, not by trying again */
    bool edit = app->key_failed && app->profile_saved;
    util_text_screen(
        app,
        "Connection Failed",
        true,
        text,
        target,
        true,
        "Cancel",
        NULL,
        edit ? "Edit Host" : "Retry");
}

static void record_recent(SshRunnerApp* app) {
    RecentEntry e;
    memset(&e, 0, sizeof(e));
    e.target = app->profile.target;
    e.transport = app->profile.transport;
    if(app->profile_saved) snprintf(e.alias, sizeof(e.alias), "%s", app->profile.alias);
    recent_push(&app->recent, &e);
    store_save_recent(app);
}

/* React to the current session phase; returns true if the scene changed. */
static bool connect_follow(SshRunnerApp* app) {
    Session* s = app->session;
    switch(s->phase) {
    case SessionHostkey:
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneHostkey);
        return true;
    case SessionPassword:
        util_start_input(app, InputPassword, NULL);
        return true;
    case SessionPassphrase:
        util_start_input(app, InputPassphrase, NULL);
        return true;
    case SessionReady:
        close_key(app); /* streamed and parsed: the ESP32 has wiped its copy */
        app_blink_stop(app);
        app_notify_success(app);
        record_recent(app);
        scene_manager_set_scene_state(app->scene_manager, SshRunnerSceneRunMenu, 0);
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneRunMenu);
        return true;
    case SessionFailed:
        close_key(app);
        if(s->reason == ProtoReasonHostkeyCancelled) {
            /* the user said no; nothing to report */
            app_end_session(app);
        } else {
            show_failure(app);
        }
        return true;
    case SessionClosed:
        app_end_session(app);
        return true;
    default:
        return false;
    }
}

void ssh_runner_scene_connect_on_enter(void* context) {
    SshRunnerApp* app = context;
    uint32_t state = scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneConnect);
    if(state == ConnectStateFailed) {
        show_failure(app);
        return;
    }
    if(state == ConnectStateNew) {
        if(!start_ssh(app)) {
            app_notify_error(app);
            app->session->phase = SessionFailed;
            show_failure(app);
            return;
        }
        scene_manager_set_scene_state(
            app->scene_manager, SshRunnerSceneConnect, ConnectStateRunning);
    }
    app_blink_start(app);
    connect_progress(app); /* spinner and seconds run while the host is silent */
    connect_follow(app);
}

bool ssh_runner_scene_connect_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    uint32_t state = scene_manager_get_scene_state(app->scene_manager, SshRunnerSceneConnect);
    if(event.type == SceneManagerEventTypeBack) {
        app_end_session(app);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(state == ConnectStateFailed) {
        if(event.event == EVT_DIALOG_RIGHT && app->key_failed && app->profile_saved) {
            /* Edit host, on its Auth row */
            app_end_session(app);
            scene_manager_set_scene_state(
                app->scene_manager, SshRunnerSceneHostEdit, HOST_EDIT_AUTH_ROW);
            scene_manager_next_scene(app->scene_manager, SshRunnerSceneHostEdit);
        } else if(event.event == EVT_DIALOG_RIGHT) {
            scene_manager_set_scene_state(
                app->scene_manager, SshRunnerSceneConnect, ConnectStateNew);
            ssh_runner_scene_connect_on_enter(app);
        } else if(event.event == EVT_DIALOG_LEFT) {
            app_end_session(app);
        }
        return true;
    }
    if(event.event == EvtChanged) {
        uint32_t chg = app_take_changes(app);
        if(chg & (SESSION_CHG_PHASE | SESSION_CHG_PROMPT)) {
            if(connect_follow(app)) return true;
        }
        if(chg & SESSION_CHG_STATUS) {
            util_progress_set(app, NULL, session_status_text(app->session));
        } else {
            util_progress_tick(app);
        }
        return true;
    }
    return false;
}

void ssh_runner_scene_connect_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_progress_stop(app);
    progress_view_reset(app->progress);
    text_view_reset(app->text_view);
}

/* ---------------- Host-key prompt ----------------
 * Unknown key: [Cancel] [Trust]. Different key: warning + [Cancel] [Options].
 * There is no centre button, so a stray OK press never trusts a key. A TextView is
 * used instead of DialogEx: the full fingerprint needs a scrollable, word-wrapped area
 * above a fixed button bar (monospace, broken at clean boundaries by core/wrap.c). */

static void decide(SshRunnerApp* app, uint8_t decision) {
    ProtoHostkeyDecision d;
    memset(&d, 0, sizeof(d));
    d.decision = decision;
    if(decision == ProtoDecisionAdd || decision == ProtoDecisionReplace) {
        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);
        snprintf(d.label, sizeof(d.label), "%02u%02u%02u", dt.year % 100, dt.month, dt.day);
    }
    app->backend.ops->hostkey_decision(app->backend.self, &d);
    app->session->phase = SessionConnecting;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SshRunnerSceneConnect);
}

void ssh_runner_scene_hostkey_on_enter(void* context) {
    SshRunnerApp* app = context;
    char hk_text[200]; /* TextView copies both: no static buffers (FAP RAM) */
    char hk_fp[400];
    const ProtoHostkeyPrompt* pr = &app->session->prompt;
    TextView* tv = app->text_view;
    text_view_reset(tv);
    app_blink_stop(app);
    char label[48];
    util_profile_label(&app->profile, label, sizeof(label));
    const char* type = key_type_short(pr->key_type);
    size_t n = 0;
    if(pr->kind == ProtoHostkeyUnknown) {
        /* the key type in the title, so host, "SHA256:" and the first fingerprint line all
         * fit above the buttons; host and fingerprint in FontKeyboard as on Public key */
        n += snprintf(hk_text + n, sizeof(hk_text) - n, "New %s key", type);
        text_view_set_header(tv, hk_text, NULL);
        snprintf(hk_fp, sizeof(hk_fp), "%s\n%s", label, pr->fp);
        text_view_set_text(tv, 0, hk_fp, TextViewFontMono);
        text_view_set_buttons(tv, "Cancel", NULL, "Trust");
    } else {
        /* which host first (the most important fact on this screen), then the short
         * warning, then the fingerprints by scrolling */
        text_view_set_header(tv, "Host Key Changed", &I_Alert_9x8);
        text_view_set_text(tv, 0, label, TextViewFontMono);
        n += snprintf(hk_text + n, sizeof(hk_text) - n, "Its %s key changed.", type);
        n += snprintf(
            hk_text + n,
            sizeof(hk_text) - n,
            app->profile.auth == ProfileAuthPassword ?
                " If you didn't expect this, don't send your password." :
                " If you didn't expect this, cancel.");
        text_view_set_text(tv, 1, hk_text, TextViewFontBody);
        /* no blank line before it: the view must never look finished while text follows */
        size_t m = snprintf(hk_fp, sizeof(hk_fp), "New:\n%s", pr->fp);
        if(pr->count) {
            m += snprintf(hk_fp + m, sizeof(hk_fp) - m, "\nTrusted (%u):", pr->trusted_total);
            for(uint8_t i = 0; i < pr->count; i++) {
                int r = snprintf(hk_fp + m, sizeof(hk_fp) - m, "\n%s", pr->trusted[i].fp);
                if(r < 0 || (size_t)r >= sizeof(hk_fp) - m) { /* full: keep the whole lines */
                    hk_fp[m] = 0;
                    break;
                }
                m += (size_t)r;
            }
        }
        text_view_set_text(tv, 2, hk_fp, TextViewFontMono);
        text_view_set_buttons(tv, "Cancel", NULL, "Options");
    }
    text_view_set_callback(tv, util_text_view_cb, app);
    notification_message(app->notifications, &sequence_single_vibro);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewText);
}

bool ssh_runner_scene_hostkey_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        decide(app, ProtoDecisionCancel);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == EVT_DIALOG_LEFT) {
        decide(app, ProtoDecisionCancel);
    } else if(event.event == EVT_DIALOG_RIGHT) {
        if(app->session->prompt.kind == ProtoHostkeyUnknown) {
            decide(app, ProtoDecisionAdd);
        } else {
            scene_manager_next_scene(app->scene_manager, SshRunnerSceneHostkeyOptions);
        }
    }
    return true;
}

void ssh_runner_scene_hostkey_on_exit(void* context) {
    SshRunnerApp* app = context;
    text_view_reset(app->text_view);
}

/* ---------------- Host-key options (mismatch) ---------------- */

enum {
    HkOptCancel,
    HkOptOnce,
    HkOptAdd,
    HkOptReplace,
};

void ssh_runner_scene_hostkey_options_on_enter(void* context) {
    SshRunnerApp* app = context;
    Submenu* sm = app->submenu;
    util_submenu_reset(sm);
    submenu_set_header(sm, "Host Key Changed");
    /* Cancel first: the cursor starts on the safe choice. */
    util_submenu_add(sm, "Cancel", HkOptCancel, util_submenu_cb, app);
    util_submenu_add(sm, "Connect Once", HkOptOnce, util_submenu_cb, app);
    util_submenu_add(sm, "Trust & Add", HkOptAdd, util_submenu_cb, app);
    util_submenu_add(sm, "Replace All", HkOptReplace, util_submenu_cb, app);
    submenu_set_selected_item(sm, HkOptCancel);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

void hostkey_decide_replace(SshRunnerApp* app) {
    decide(app, ProtoDecisionReplace);
}

bool ssh_runner_scene_hostkey_options_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event >= EvtBase) return false;
    switch(event.event) {
    case HkOptCancel:
        decide(app, ProtoDecisionCancel);
        break;
    case HkOptOnce:
        decide(app, ProtoDecisionOnce);
        break;
    case HkOptAdd:
        decide(app, ProtoDecisionAdd);
        break;
    case HkOptReplace:
        app->confirm_action = ConfirmReplaceKeys;
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneConfirm);
        break;
    }
    return true;
}

void ssh_runner_scene_hostkey_options_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_submenu_reset(app->submenu);
}

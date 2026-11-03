/* Settings > Security (Tier 1 "Protect Secrets"): status, the warning, the typed word,
 * progress and result. SPDX-License-Identifier: GPL-3.0-only */
#include "scene_util.h"
#include <ssh_runner_icons.h>

#define SEC_RELINK_MS 30000u /* the ESP32 restarts to erase the plaintext copy */

static bool link_up(const SshRunnerApp* app) {
    return app->session->link.state == BackendLinkUp;
}

/* ---------------- Security: status ---------------- */

static void security_show(SshRunnerApp* app) {
    char* body = util_scratch;
    const char* right = NULL;
    const char* virt = app->sec.efuse_virtual ? "\nTest build: virtual eFuses." : "";
    if(!link_up(app))
        snprintf(body, UTIL_SCRATCH_SIZE, "No ESP32 board connected.");
    else if(!app->sec_known)
        snprintf(body, UTIL_SCRATCH_SIZE, "Asking the ESP32...");
    else if(app->sec.tier >= ProtoSecTierProtect)
        snprintf(
            body,
            UTIL_SCRATCH_SIZE,
            "Secrets are encrypted\nwith a key in the ESP32.\nPermanent. Updates\nstill work.%s",
            virt);
    else {
        snprintf(body, UTIL_SCRATCH_SIZE, "Saved secrets on the ESP32 are not encrypted.%s", virt);
        right = "Protect";
    }
    util_text_screen(app, "Security", false, body, NULL, true, NULL, NULL, right);
}

void ssh_runner_scene_security_on_enter(void* context) {
    SshRunnerApp* app = context;
    app->sec_known = false;
    if(link_up(app)) app->backend.ops->sec_get_status(app->backend.self);
    security_show(app);
}

bool ssh_runner_scene_security_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == EVT_DIALOG_RIGHT && app->sec_known && app->sec.tier == ProtoSecTierNone) {
        app->confirm_action = ConfirmProtectSecrets;
        scene_manager_next_scene(app->scene_manager, SshRunnerSceneConfirm);
        return true;
    }
    if(event.event == EvtChanged && (app_take_changes(app) & (APP_CHG_SEC | SESSION_CHG_LINK))) {
        if(link_up(app) && !app->sec_known) app->backend.ops->sec_get_status(app->backend.self);
        security_show(app);
    }
    return event.event == EvtChanged;
}

void ssh_runner_scene_security_on_exit(void* context) {
    SshRunnerApp* app = context;
    text_view_reset(app->text_view);
}

/* ---------------- the typed word, then progress ---------------- */

void sec_enable_start(SshRunnerApp* app) {
    ProtoSecEnable m = {.tier = ProtoSecTierProtect};
    snprintf(m.word, sizeof(m.word), "%s", app->input);
    memset(app->input, 0, sizeof(app->input));
    memset(&app->sec_state, 0, sizeof(app->sec_state));
    app->sec_relinked = false;
    app->sec_restart_at = 0;
    bool ok = app->backend.ops->sec_enable(app->backend.self, m.word);
    if(!ok) {
        app_notify_error(app);
        app_show_error(
            app, "Not Protected", "ESP32 busy or not connected", SshRunnerSceneSecurity);
        return;
    }
    scene_manager_next_scene(app->scene_manager, SshRunnerSceneSecProgress);
}

static const char* sec_phase_text(uint8_t phase) {
    switch(phase) {
    case ProtoSecPhaseBurn:
        return "Writing the key...";
    case ProtoSecPhaseMigrate:
        return "Encrypting secrets...";
    case ProtoSecPhaseErase:
        return "Restarting the ESP32...";
    default:
        return "Starting...";
    }
}

void ssh_runner_scene_sec_progress_on_enter(void* context) {
    SshRunnerApp* app = context;
    util_progress_start(
        app, "Protect Secrets", false, UTIL_SPINNER, sec_phase_text(0), "Don't power off");
}

static void sec_done(SshRunnerApp* app, bool ok, const char* text) {
    util_progress_stop(app);
    if(ok) {
        app_notify_success(app);
        app_show_message(app, "Secrets Protected", text, SshRunnerSceneSecurity);
    } else {
        app_notify_error(app);
        app_show_error(app, "Not Protected", text, SshRunnerSceneSecurity);
    }
}

bool ssh_runner_scene_sec_progress_on_event(void* context, SceneManagerEvent event) {
    SshRunnerApp* app = context;
    if(event.type == SceneManagerEventTypeBack) return true; /* cannot be stopped half-way */
    if(event.type != SceneManagerEventTypeCustom || event.event != EvtChanged) return false;
    uint32_t chg = app_take_changes(app);
    util_progress_tick(app);
    const ProtoSecState* st = &app->sec_state;
    if(chg & APP_CHG_ERROR && app->last_error.request == PROTO_T_SEC_ENABLE) {
        sec_done(
            app, false, app->last_error.msg[0] ? app->last_error.msg : "The ESP32 refused it.");
        return true;
    }
    if(chg & APP_CHG_SEC) {
        if(st->result == ProtoSecFailed) {
            sec_done(app, false, st->msg);
            return true;
        }
        if(st->phase == ProtoSecPhaseDone && st->result == ProtoSecOk) {
            sec_done(
                app,
                true,
                st->msg[0] ? st->msg :
                             "Secrets are now encrypted.\nPermanent. Updates still work.");
            return true;
        }
        if(st->phase == ProtoSecPhaseErase && !app->sec_restart_at)
            app->sec_restart_at = furi_get_tick();
        /* after the restart: the status says whether it worked */
        if(app->sec_known && app->sec_restart_at && app->sec.tier >= ProtoSecTierProtect) {
            sec_done(app, true, "Secrets are now encrypted.\nPermanent. Updates still work.");
            return true;
        }
        util_progress_set(app, NULL, sec_phase_text(st->phase));
    }
    if(app->sec_relinked) {
        app->sec_relinked = false;
        app->sec_known = false;
        app->backend.ops->sec_get_status(app->backend.self);
    }
    if(app->sec_restart_at &&
       furi_get_tick() - app->sec_restart_at > furi_ms_to_ticks(SEC_RELINK_MS))
        sec_done(app, false, "The ESP32 did not come back. Check Settings > Security.");
    return true;
}

void ssh_runner_scene_sec_progress_on_exit(void* context) {
    SshRunnerApp* app = context;
    util_progress_stop(app);
    progress_view_reset(app->progress);
}

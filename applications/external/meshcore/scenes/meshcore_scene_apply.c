/*
 * scene_apply — push a preset onto the node, then check it actually took.
 *
 * The checking is the point. Sending SET_RADIO_PARAMS and drawing a tick is
 * easy and worthless: a node can answer OK and keep its old settings, and the
 * failure only shows up in a field, as a network that does not form. So every
 * step is judged against a re-read SELF_INFO / DEVICE_INFO, and a step that
 * cannot be judged says so instead of showing a tick.
 *
 * All of the thinking is in config/meshcore_apply.c, which is pure and tested
 * on a host. This file sends bytes and draws.
 */
#include "../config/meshcore_apply.h"
#include "../meshcore_cfg.h"

#define MESHCORE_APPLY_EVENT_DONE 0x410u
#define MESHCORE_APPLY_WORKER_STACK 2048u

typedef enum {
    MeshCoreApplyResultSkipped, /* the preset says nothing about this step */
    MeshCoreApplyResultOk,
    MeshCoreApplyResultRefused, /* the node answered with an error */
    MeshCoreApplyResultSilent, /* the node did not answer at all */
    MeshCoreApplyResultMismatch, /* it answered OK and did not change */
    MeshCoreApplyResultUnverifiable, /* applied, but the node cannot be asked */
} MeshCoreApplyResult;

static const char* meshcore_scene_apply_mark(MeshCoreApplyResult result) {
    switch(result) {
    case MeshCoreApplyResultOk:
        return "[ok]";
    case MeshCoreApplyResultSkipped:
        return "[--]";
    case MeshCoreApplyResultRefused:
        return "[NO]";
    case MeshCoreApplyResultSilent:
        return "[??]";
    case MeshCoreApplyResultMismatch:
        return "[!!]";
    case MeshCoreApplyResultUnverifiable:
        return "[~ ]";
    default:
        return "[  ]";
    }
}

static int32_t meshcore_scene_apply_worker(void* context) {
    MeshCoreApp* app = context;
    const MeshCorePreset* preset = &app->presets.items[app->preset_index].preset;

    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t event;

    /* Send everything first, then re-read once and judge all of it. Re-reading
     * after each step would triple the traffic and tell us nothing more. */
    for(size_t step = 0; step < MeshCoreApplyStepCount; step++) {
        if(!meshcore_apply_step_applies(preset, (MeshCoreApplyStep)step)) {
            app->apply_result[step] = MeshCoreApplyResultSkipped;
            continue;
        }

        size_t len = meshcore_apply_build(preset, (MeshCoreApplyStep)step, payload, sizeof(payload));
        if(len == 0) {
            app->apply_result[step] = MeshCoreApplyResultSkipped;
            continue;
        }

        if(meshcore_session_request(
               app->session, payload, len, MC_RESP_OK, &event, MESHCORE_LINK_TIMEOUT_MS)) {
            app->apply_result[step] = MeshCoreApplyResultOk;
        } else if(event.code == MC_RESP_ERR) {
            app->apply_result[step] = MeshCoreApplyResultRefused;
        } else {
            app->apply_result[step] = MeshCoreApplyResultSilent;
        }
    }

    /* Now the part that makes the ticks mean something. */
    size_t len = mc_cmd_app_start(payload, sizeof(payload), MESHCORE_LINK_APP_NAME);
    bool self_ok = len != 0 && meshcore_session_request(
                                   app->session,
                                   payload,
                                   len,
                                   MC_RESP_SELF_INFO,
                                   &event,
                                   MESHCORE_LINK_TIMEOUT_MS);
    if(self_ok) {
        if(app->apply_result[MeshCoreApplyRadio] == MeshCoreApplyResultOk &&
           !meshcore_apply_verify_radio(preset, &event.u.self_info)) {
            app->apply_result[MeshCoreApplyRadio] = MeshCoreApplyResultMismatch;
        }
        if(app->apply_result[MeshCoreApplyName] == MeshCoreApplyResultOk &&
           !meshcore_apply_verify_name(preset, &event.u.self_info)) {
            app->apply_result[MeshCoreApplyName] = MeshCoreApplyResultMismatch;
        }
        if(app->apply_result[MeshCoreApplyTxPower] == MeshCoreApplyResultOk &&
           !meshcore_apply_verify_tx(preset, &event.u.self_info)) {
            app->apply_result[MeshCoreApplyTxPower] = MeshCoreApplyResultMismatch;
        }
    } else {
        /* The re-read never came back. The node accepted the command but we
         * cannot see the result, which is exactly what [~] means — leaving an
         * [ok] here would be the false tick the whole verify pass exists to
         * avoid. */
        if(app->apply_result[MeshCoreApplyRadio] == MeshCoreApplyResultOk) {
            app->apply_result[MeshCoreApplyRadio] = MeshCoreApplyResultUnverifiable;
        }
        if(app->apply_result[MeshCoreApplyName] == MeshCoreApplyResultOk) {
            app->apply_result[MeshCoreApplyName] = MeshCoreApplyResultUnverifiable;
        }
        if(app->apply_result[MeshCoreApplyTxPower] == MeshCoreApplyResultOk) {
            app->apply_result[MeshCoreApplyTxPower] = MeshCoreApplyResultUnverifiable;
        }
    }

    len = mc_cmd_device_query(payload, sizeof(payload), MESHCORE_LINK_PROTO_VER);
    bool dev_ok = len != 0 && meshcore_session_request(
                                  app->session,
                                  payload,
                                  len,
                                  MC_RESP_DEVICE_INFO,
                                  &event,
                                  MESHCORE_LINK_TIMEOUT_MS);
    if(dev_ok) {
        if(app->apply_result[MeshCoreApplyPathHash] == MeshCoreApplyResultOk) {
            bool checkable = false;
            bool matched =
                meshcore_apply_verify_path_hash(preset, &event.u.device_info, &checkable);
            if(!checkable) {
                /* Firmware older than v10 does not report it. Claiming a tick
                 * there would be a lie the operator cannot catch. */
                app->apply_result[MeshCoreApplyPathHash] = MeshCoreApplyResultUnverifiable;
            } else if(!matched) {
                app->apply_result[MeshCoreApplyPathHash] = MeshCoreApplyResultMismatch;
            }
        }
    } else if(app->apply_result[MeshCoreApplyPathHash] == MeshCoreApplyResultOk) {
        app->apply_result[MeshCoreApplyPathHash] = MeshCoreApplyResultUnverifiable;
    }

    /* One line to the log so the outcome survives in last_run.log — the screen
     * needs the device in hand, and this is the only record of what a given
     * Apply actually did to the node. */
    meshcore_log_printf(
        app->log,
        "apply %.20s: radio %s name %s hash %s",
        preset->name,
        meshcore_scene_apply_mark(app->apply_result[MeshCoreApplyRadio]),
        meshcore_scene_apply_mark(app->apply_result[MeshCoreApplyName]),
        meshcore_scene_apply_mark(app->apply_result[MeshCoreApplyPathHash]));

    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_APPLY_EVENT_DONE);
    return 0;
}

static void meshcore_scene_apply_show(MeshCoreApp* app, bool done) {
    const MeshCorePresetEntry* entry = &app->presets.items[app->preset_index];
    char text[512];
    size_t used = 0;

    used += (size_t)snprintf(text + used, sizeof(text) - used, "\e#%.24s\n", entry->preset.name);

    if(!meshcore_preset_store_usable(entry)) {
        snprintf(
            text + used,
            sizeof(text) - used,
            "Not applied.\n\n%s\n%s\n\nFix the file and reopen Profiles.",
            entry->file,
            entry->error);
        widget_reset(app->widget);
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
        return;
    }

    char freq[MESHCORE_PRESET_FIELD_LEN];
    char bw[MESHCORE_PRESET_FIELD_LEN];
    meshcore_preset_format_freq(entry->preset.freq_khz, freq, sizeof(freq));
    meshcore_preset_format_bw(entry->preset.bw_hz, bw, sizeof(bw));

    used += (size_t)snprintf(
        text + used,
        sizeof(text) - used,
        "%s  %s\nSF%u  CR4/%u  hash %u\n\n",
        freq,
        bw,
        (unsigned)entry->preset.sf,
        (unsigned)entry->preset.cr,
        (unsigned)entry->preset.path_hash_bytes);

    if(!done) {
        snprintf(text + used, sizeof(text) - used, "Applying...");
    } else {
        for(size_t step = 0; step < MeshCoreApplyStepCount && used < sizeof(text); step++) {
            used += (size_t)snprintf(
                text + used,
                sizeof(text) - used,
                "%s %s\n",
                meshcore_scene_apply_mark(app->apply_result[step]),
                meshcore_apply_step_name((MeshCoreApplyStep)step));
        }

        /* The legend earns its space: [ok] and [~ ] mean genuinely different
         * things and the difference is the whole point of verifying. */
        snprintf(
            text + used,
            sizeof(text) - used,
            "\n[ok] set and confirmed\n"
            "[~ ] set, node cannot\n     confirm it\n"
            "[!!] said ok, did not\n     change\n"
            "[NO] refused\n"
            "[??] no answer\n"
            "[--] not in this preset");
    }

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
}

void meshcore_scene_apply_on_enter(void* context) {
    MeshCoreApp* app = context;

    meshcore_scene_apply_show(app, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);

    if(!meshcore_preset_store_usable(&app->presets.items[app->preset_index])) return;

    if(app->session == NULL || !meshcore_session_is_running(app->session)) {
        /* Nothing to apply to. Said plainly rather than by a screen of [??]. */
        widget_reset(app->widget);
        widget_add_text_scroll_element(
            app->widget,
            0,
            0,
            128,
            64,
            "\e#Not connected\n"
            "Run Connect first, then\n"
            "come back.\n\n"
            "The node must be plugged\n"
            "in and rebooted after\n"
            "plugging in.");
        return;
    }

    for(size_t i = 0; i < MeshCoreApplyStepCount; i++) {
        app->apply_result[i] = MeshCoreApplyResultSkipped;
    }

    app->worker = furi_thread_alloc_ex(
        "MeshCoreApply", MESHCORE_APPLY_WORKER_STACK, meshcore_scene_apply_worker, app);
    furi_thread_start(app->worker);
}

bool meshcore_scene_apply_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == MESHCORE_APPLY_EVENT_DONE) {
        if(app->worker) {
            furi_thread_join(app->worker);
            furi_thread_free(app->worker);
            app->worker = NULL;
        }
        meshcore_scene_apply_show(app, true);
        return true;
    }

    return false;
}

void meshcore_scene_apply_on_exit(void* context) {
    MeshCoreApp* app = context;

    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    widget_reset(app->widget);
}

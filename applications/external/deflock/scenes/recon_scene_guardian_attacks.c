// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "../recon_app_i.h"
#include "../helpers/attack_triage.h"
#include "../helpers/report_fmt.h" // fmt_mac

#include <stdarg.h>
#include <string.h>

// Net Guardian -> Left -> this screen: the detail behind the "Atk N" counter.
//
// WHY IT EXISTS. The HUD showed a bare count, which answers the wrong question.
// The one an operator actually has is "is my network under attack right now, or
// did the router just reboot?" -- and a number cannot tell those apart. This
// screen turns the counters the companion already reports (no new sensor, no
// transmit) into a verdict and an action:
//
//   - WHAT it is (deauth flood / beacon-probe-BLE spam / evil twin)
//   - WHETHER it is real (attack_triage_status: brief blip vs sustained+fresh)
//   - the TARGET and how long it has run
//   - what to actually DO about it (attack_advice)
//
// Rebuilt on every tick so the span and freshness stay live. Content is short
// and usually one screenful; the scroll element handles the busy case.

#define ATK_TEXT_MAX 640

static void atk_append(char* buf, size_t cap, size_t* pos, const char* fmt, ...) {
    if(*pos >= cap) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *pos, cap - *pos, fmt, ap);
    va_end(ap);
    if(n > 0) *pos += ((size_t)n < cap - *pos) ? (size_t)n : (cap - *pos);
}

/** Split a long advice string onto ~20-char lines so it does not clip. */
static void atk_append_wrapped(char* buf, size_t cap, size_t* pos, const char* text) {
    const int width = 20;
    int line = 0;
    for(const char* p = text; *p;) {
        // find the next break at or before `width`
        int take = 0, last_space = -1;
        while(p[take] && take < width) {
            if(p[take] == ' ') last_space = take;
            take++;
        }
        if(p[take] && last_space > 0) take = last_space;
        atk_append(buf, cap, pos, " %.*s\n", take, p);
        p += take;
        while(*p == ' ')
            p++;
        if(++line > 6) break; // never let one advice block run away
    }
}

static AttackKind atk_kind_from_label(const char* label, bool ble) {
    if(!label) return ble ? AttackKindBleSpam : AttackKindOther;
    if(strstr(label, "ble") || strstr(label, "BLE")) return AttackKindBleSpam;
    if(strstr(label, "beacon")) return AttackKindBeaconFlood;
    if(strstr(label, "probe")) return AttackKindProbeFlood;
    return AttackKindOther;
}

static void atk_build(ReconApp* app) {
    char text[ATK_TEXT_MAX];
    size_t pos = 0;
    text[0] = '\0';

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    uint32_t now = furi_get_tick();
    unsigned shown = 0;

    // (1) Deauth floods -- one per attacked BSSID.
    for(size_t i = 0; i < app->deauth_count; i++) {
        const DeauthTarget* d = &app->deauth[i];
        AttackStatus st =
            attack_triage_status(d->count, d->first_tick, d->last_tick, now, 0, 0, 0);
        if(st == AttackStatusEnded) continue; // don't clutter with what stopped
        char mac[18];
        fmt_mac(mac, sizeof(mac), d->bssid);
        uint32_t secs = (d->last_tick - d->first_tick) / 1000;
        atk_append(
            text,
            sizeof(text),
            &pos,
            "[%s] %s\n",
            attack_status_str(st),
            attack_kind_str(AttackKindDeauth));
        atk_append(text, sizeof(text), &pos, " Tgt %s ch%u\n", mac, d->channel);
        atk_append(
            text,
            sizeof(text),
            &pos,
            " %lu frames  %lu:%02lu\n",
            (unsigned long)d->count,
            (unsigned long)(secs / 60),
            (unsigned long)(secs % 60));
        atk_append_wrapped(text, sizeof(text), &pos, attack_advice(AttackKindDeauth));
        atk_append(text, sizeof(text), &pos, "--\n");
        shown++;
    }

    // (2) The active attack-tool signature (beacon/probe/BLE flood), if fresh.
    if(app->esp_attack_tick) {
        AttackStatus st = attack_triage_status(
            app->esp_attack_value, app->esp_attack_first_tick, app->esp_attack_tick, now, 1, 0, 0);
        if(st != AttackStatusEnded) {
            AttackKind k = atk_kind_from_label(app->esp_attack_kind, app->esp_attack_ble);
            uint32_t secs = (app->esp_attack_tick - app->esp_attack_first_tick) / 1000;
            atk_append(
                text, sizeof(text), &pos, "[%s] %s\n", attack_status_str(st), attack_kind_str(k));
            atk_append(
                text,
                sizeof(text),
                &pos,
                " rate %lu  %lu:%02lu\n",
                (unsigned long)app->esp_attack_value,
                (unsigned long)(secs / 60),
                (unsigned long)(secs % 60));
            atk_append_wrapped(text, sizeof(text), &pos, attack_advice(k));
            atk_append(text, sizeof(text), &pos, "--\n");
            shown++;
        }
    }

    // (3) Evil twins -- a clone of a network's SSID with mismatched security.
    for(size_t i = 0; i < app->wifi_count && shown < 6; i++) {
        if(!app->wifi[i].rogue) continue;
        atk_append(text, sizeof(text), &pos, "[!] %s\n", attack_kind_str(AttackKindEvilTwin));
        atk_append(
            text,
            sizeof(text),
            &pos,
            " SSID \"%.16s\"\n",
            app->wifi[i].ssid[0] ? app->wifi[i].ssid : "(hidden)");
        atk_append_wrapped(text, sizeof(text), &pos, attack_advice(AttackKindEvilTwin));
        atk_append(text, sizeof(text), &pos, "--\n");
        shown++;
    }

    bool evidence = app->settings.guard_evidence;
    uint32_t logged = app->guard_evidence_n;
    furi_mutex_release(app->mutex);

    if(shown == 0) {
        atk_append(
            text,
            sizeof(text),
            &pos,
            "No active attacks.\n \nNet Guardian is\nwatching. Deauth\nfloods, evil twins\n"
            "and flood tools\nshow here when live.\n");
    }
    atk_append(
        text,
        sizeof(text),
        &pos,
        " \nEvidence log: %s (%lu)\n",
        evidence ? "on" : "off",
        (unsigned long)logged);

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
}

void recon_scene_guardian_attacks_on_enter(void* context) {
    ReconApp* app = context;
    atk_build(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

bool recon_scene_guardian_attacks_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    // Live refresh: the numbers (span, freshness) change every second.
    if(event.type == SceneManagerEventTypeTick) {
        atk_build(app);
        return true;
    }
    return false;
}

void recon_scene_guardian_attacks_on_exit(void* context) {
    ReconApp* app = context;
    widget_reset(app->widget);
}

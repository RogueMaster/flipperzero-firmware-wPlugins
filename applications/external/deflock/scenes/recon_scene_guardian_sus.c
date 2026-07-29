// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"

#include <stdlib.h> // malloc/free for the scene-scoped snapshot

// Net Guardian -> OK -> this list: the devices that tripped (or could trip) the
// Guardian -- BLE Flippers, opt-in "anomaly" unknown devices, and Wi-Fi rogue /
// evil-twin APs. Selecting one MARKS it (so it joins the report + Locator pool)
// and jumps straight into the Locator homing HUD on it.
//
// Note: deauth/attack *sources* aren't listed -- attackers spoof their MAC, so
// they aren't reliably locatable. This list is the markable, locatable subset.

#define SUS_MAX 64

typedef struct {
    uint8_t mac[6];
    uint8_t kind; // 'w' / 'b'
    uint8_t ch;
    char label[28];
    uint8_t src; // 0 = ble[], 1 = wifi[]
    uint16_t idx; // index into that array (frozen while this scene is up)
} SusTarget;

/**
 * Scene-scoped snapshot, allocated on entry and freed on exit.
 *
 * This used to be a static array, which cost 2560 bytes of BSS for the entire
 * run of the app to serve one scene you are usually not in. The Locator and
 * DeFlock-handoff scenes had the same pattern; together they held ~5.9 KB
 * permanently out of a ~256 KB budget.
 *
 * NULL is a supported state, not an error path to bolt on later: if the
 * allocation fails the scene renders as an empty list rather than crashing, and
 * sus_add() simply drops entries. Every consumer below checks.
 */
static SusTarget* s_sus;
static size_t s_sus_count;

static void sus_add(
    const uint8_t* mac,
    uint8_t kind,
    uint8_t ch,
    const char* label,
    uint8_t src,
    uint16_t idx) {
    if(!s_sus || s_sus_count >= SUS_MAX) return;
    SusTarget* t = &s_sus[s_sus_count++];
    memcpy(t->mac, mac, 6);
    t->kind = kind;
    t->ch = ch;
    snprintf(t->label, sizeof(t->label), "%s", label);
    t->src = src;
    t->idx = idx;
}

static void recon_scene_guardian_sus_cb(void* context, uint32_t index) {
    ReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void recon_scene_guardian_sus_on_enter(void* context) {
    ReconApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    s_sus_count = 0;
    // Freed in on_exit. malloc (not furi_alloc) so a failure is recoverable:
    // sus_add() and the render below both tolerate NULL and show an empty list.
    if(!s_sus) s_sus = malloc(sizeof(SusTarget) * SUS_MAX);

    uint32_t now = furi_get_tick();
    char buf[28];
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    for(size_t i = 0; i < app->ble_count && s_sus_count < SUS_MAX; i++) {
        const BleDevice* d = &app->ble[i];
        bool flip = (d->cat == BleCatFlipper);
        bool anom = recon_ble_is_anomaly(d, now);
        if(!flip && !anom) continue;
        if(flip) {
            if(d->name[0])
                snprintf(buf, sizeof(buf), "Flipper %s", d->name);
            else
                snprintf(
                    buf, sizeof(buf), "Flipper %02X%02X%02X", d->addr[3], d->addr[4], d->addr[5]);
        } else {
            snprintf(
                buf,
                sizeof(buf),
                "Unknown %02X%02X%02X %ddB",
                d->addr[3],
                d->addr[4],
                d->addr[5],
                d->rssi);
        }
        sus_add(d->addr, 'b', 0, buf, 0, (uint16_t)i);
    }
    for(size_t i = 0; i < app->wifi_count && s_sus_count < SUS_MAX; i++) {
        const WifiAp* a = &app->wifi[i];
        if(!a->rogue) continue;
        if(a->ssid[0])
            snprintf(buf, sizeof(buf), "Rogue AP %s", a->ssid);
        else
            snprintf(
                buf, sizeof(buf), "Rogue AP %02X%02X%02X", a->bssid[3], a->bssid[4], a->bssid[5]);
        sus_add(a->bssid, 'w', a->channel, buf, 1, (uint16_t)i);
    }
    furi_mutex_release(app->mutex);

    if(s_sus_count == 0) {
        submenu_set_header(submenu, "No suspicious devices");
        submenu_add_item(submenu, "(nothing flagged yet)", 0, recon_scene_guardian_sus_cb, app);
    } else {
        submenu_set_header(submenu, "Suspicious - OK to locate");
        for(size_t i = 0; i < s_sus_count; i++) {
            submenu_add_item(submenu, s_sus[i].label, i, recon_scene_guardian_sus_cb, app);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool recon_scene_guardian_sus_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t idx = event.event;
        if(s_sus && idx < s_sus_count) {
            SusTarget* t = &s_sus[idx];
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            // Mark the underlying entry (arrays are frozen while this scene is up)
            // so it also lands in the report + the Locator target list.
            if(t->src == 0 && t->idx < app->ble_count)
                app->ble[t->idx].marked = true;
            else if(t->src == 1 && t->idx < app->wifi_count)
                app->wifi[t->idx].marked = true;
            memcpy(app->locate_mac, t->mac, 6);
            app->locate_kind = t->kind;
            app->locate_ch = t->ch;
            snprintf(app->locate_label, sizeof(app->locate_label), "%s", t->label);
            furi_mutex_release(app->mutex);
            scene_manager_next_scene(app->scene_manager, ReconSceneLocatorHome);
        }
        return true;
    }
    return false;
}

void recon_scene_guardian_sus_on_exit(void* context) {
    ReconApp* app = context;
    submenu_reset(app->submenu);
    // Release the snapshot: this scene is one of several that each held a
    // 64-entry array, and they are mutually exclusive in practice.
    free(s_sus);
    s_sus = NULL;
    s_sus_count = 0;
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"

// Locator step 1: a picker listing every device currently MARKED across Flock /
// WiFi / BLE (the unified report-tag doubles as the Locator pool). Selecting one
// copies it into app->locate_* and opens the homing HUD.

#define LOC_MAX_TARGETS 64

typedef struct {
    uint8_t mac[6];
    uint8_t kind; // 'w' Wi-Fi / 'b' BLE
    uint8_t ch; // Wi-Fi channel (0 = BLE / hop)
    char label[28];
} LocTarget;

static LocTarget s_targets[LOC_MAX_TARGETS];
static size_t s_count;

static const char* loc_ble_kind(uint8_t cat) {
    switch(cat) {
    case BleCatFlock:
        return "Flock";
    case BleCatAirTag:
        return "AirTag";
    case BleCatTile:
        return "Tile";
    case BleCatSmartTag:
        return "Tag";
    case BleCatFindMyDevice:
        return "FindMy";
    case BleCatFlipper:
        return "Flipper";
    default:
        return "BLE";
    }
}

static void loc_add(const uint8_t* mac, uint8_t kind, uint8_t ch, const char* label) {
    if(s_count >= LOC_MAX_TARGETS) return;
    LocTarget* t = &s_targets[s_count++];
    memcpy(t->mac, mac, 6);
    t->kind = kind;
    t->ch = ch;
    snprintf(t->label, sizeof(t->label), "%s", label);
}

static void recon_scene_locator_cb(void* context, uint32_t index) {
    ReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void recon_scene_locator_on_enter(void* context) {
    ReconApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    s_count = 0;

    char buf[28];
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    for(size_t i = 0; i < app->flock_count && s_count < LOC_MAX_TARGETS; i++) {
        const FlockEntry* e = &app->flock[i];
        if(!e->marked) continue;
        if(e->ssid[0])
            snprintf(buf, sizeof(buf), "Flock %s", e->ssid);
        else
            snprintf(buf, sizeof(buf), "Flock %02X%02X%02X", e->mac[3], e->mac[4], e->mac[5]);
        loc_add(e->mac, (e->ftype == 'L') ? 'b' : 'w', e->channel, buf);
    }
    for(size_t i = 0; i < app->wifi_count && s_count < LOC_MAX_TARGETS; i++) {
        const WifiAp* a = &app->wifi[i];
        if(!a->marked) continue;
        if(a->ssid[0])
            snprintf(buf, sizeof(buf), "AP %s", a->ssid);
        else
            snprintf(buf, sizeof(buf), "AP %02X%02X%02X", a->bssid[3], a->bssid[4], a->bssid[5]);
        loc_add(a->bssid, 'w', a->channel, buf);
    }
    for(size_t i = 0; i < app->ble_count && s_count < LOC_MAX_TARGETS; i++) {
        const BleDevice* d = &app->ble[i];
        if(!d->marked) continue;
        const char* ty = loc_ble_kind(d->cat);
        if(d->name[0])
            snprintf(buf, sizeof(buf), "%s %s", ty, d->name);
        else
            snprintf(buf, sizeof(buf), "%s %02X%02X%02X", ty, d->addr[3], d->addr[4], d->addr[5]);
        loc_add(d->addr, 'b', 0, buf);
    }
    furi_mutex_release(app->mutex);

    if(s_count == 0) {
        submenu_set_header(submenu, "Locator - nothing marked");
        // Index 0 is a no-op here (on_event guards index < s_count == 0).
        submenu_add_item(
            submenu, "Mark a device in Flock/BLE/WiFi", 0, recon_scene_locator_cb, app);
    } else {
        submenu_set_header(submenu, "Locator - pick a target");
        for(size_t i = 0; i < s_count; i++) {
            submenu_add_item(submenu, s_targets[i].label, i, recon_scene_locator_cb, app);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool recon_scene_locator_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t idx = event.event;
        if(idx < s_count) {
            LocTarget* t = &s_targets[idx];
            furi_mutex_acquire(app->mutex, FuriWaitForever);
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

void recon_scene_locator_on_exit(void* context) {
    ReconApp* app = context;
    submenu_reset(app->submenu);
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "recon_app_i.h"
#include <furi_hal_power.h>
#include "helpers/esp_link.h"
#include "helpers/esp_parser.h" // esp_hexval, for the guarded-BSSID setting
#include "helpers/gps_link.h"
#include "helpers/gps_rpc.h"
#include "helpers/recon_report.h"
#include "helpers/sig_db.h"
#include "helpers/detect_rules.h"
#include "helpers/flock_store.h"
#include "helpers/scan_session.h"
#include "helpers/report_fmt.h"

#include <math.h>
#include <string.h>

#define RECON_TICK_MS 250

// Anti-stalking "following" gate + geotag hysteresis live as pure, host-tested
// rules in helpers/detect_rules.h (FOLLOW_MIN_*, WAYPOINT_GAP_M, the track fold
// and the AND-gate). recon_app.c below is the thin lock+array shell that
// snapshots inputs, calls those rules, and writes the results back.

// ---- shared data updates (called from worker threads) --------------------

void recon_app_report_flock(
    ReconApp* app,
    const uint8_t mac[6],
    const char* ssid,
    int8_t rssi,
    uint8_t channel,
    char ftype,
    FlockConfidence confidence,
    uint32_t ie_fp,
    FlockDevClass dev_class,
    bool hidden) {
    if(confidence == FlockConfidenceNone) return;

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    uint32_t now = furi_get_tick();
    FlockEntry* entry = NULL;
    for(size_t i = 0; i < app->flock_count; i++) {
        if(memcmp(app->flock[i].mac, mac, 6) == 0) {
            entry = &app->flock[i];
            break;
        }
    }

    if(!entry) {
        if(app->flock_count < RECON_FLOCK_MAX) {
            entry = &app->flock[app->flock_count++];
        } else {
            // Table full. Restored hits would otherwise block every new live
            // detection for the rest of the session, so reclaim the least
            // valuable ARCHIVED slot -- weakest evidence first, oldest to break
            // a tie (helpers/flock_store.h). A live entry is never evicted, so
            // with nothing archived this still degrades to the old "drop it".
            int victim = -1;
            for(size_t i = 0; i < app->flock_count; i++) {
                if(!app->flock[i].archived) continue;
                if(victim < 0 || flock_store_evict_better(
                                     (uint8_t)app->flock[i].confidence,
                                     app->flock[i].seen_epoch,
                                     (uint8_t)app->flock[victim].confidence,
                                     app->flock[victim].seen_epoch)) {
                    victim = (int)i;
                }
            }
            if(victim >= 0) entry = &app->flock[victim];
        }
        if(entry) {
            memset(entry, 0, sizeof(FlockEntry));
            memcpy(entry->mac, mac, 6);
            entry->first_tick = now;
            entry->lat = NAN;
            entry->lon = NAN;
            entry->heading = NAN;
            entry->count = 0;
        }
    }

    if(entry) {
        uint8_t prev_conf = (uint8_t)entry->confidence;
        // Captured BEFORE the flag is cleared below: it is what tells the alert
        // rule that this device's latch and confidence were restored from disk
        // rather than earned this session.
        bool was_archived = entry->archived;
        entry->count++;
        entry->last_tick = now;
        // Seen for real this session: it is a live detection again, not a stored
        // one, and it carries a fresh wall-clock timestamp for the next save.
        entry->archived = false;
        entry->seen_epoch = furi_hal_rtc_get_timestamp();
        if(rssi != 0) entry->rssi = rssi;
        if(channel != 0) entry->channel = channel;
        if(ftype) entry->ftype = ftype;
        if(confidence > entry->confidence) entry->confidence = confidence;
        // Keep the probe fingerprint so the detail screen can show it (for
        // seeding). Don't let a later fp-less sighting (BLE/beacon) wipe it.
        if(ie_fp != 0) entry->ie_fp = ie_fp;
        // Same sticky rule for the device class: ALPR is the default/absent
        // value, so only a positive acoustic identification writes it. A later
        // sighting that carries no class must not silently relabel a known
        // SoundThinking sensor as a camera.
        if(dev_class != FlockClassAlpr) entry->dev_class = (uint8_t)dev_class;
        // Likewise sticky: we saw this AP hide its name once, and a later probe
        // request from the same MAC (which carries no hidden flag at all) must
        // not erase that observation.
        if(hidden) entry->hidden = true;
        if(ssid && ssid[0] && entry->ssid[0] == '\0') {
            strncpy(entry->ssid, ssid, RECON_SSID_LEN - 1);
            entry->ssid[RECON_SSID_LEN - 1] = '\0';
        }
        // Geotag with the current fix per the hysteresis rule (haven't tagged yet,
        // or a meaningfully stronger sighting) -- see detect_rules.h.
        if(flock_geotag_should_update(
               app->gps_valid, !isnan(entry->lat), rssi, entry->geotag_rssi)) {
            entry->lat = app->gps_lat;
            entry->lon = app->gps_lon;
            entry->heading = app->gps_course;
            entry->geotag_rssi = rssi;
        }
        // Raise the detection alert on the first crossing to Likely-or-better
        // (issue #1). We only set the flag here -- this runs on the ESP worker
        // thread, so the actual notification is left to the GUI tick.
        if(flock_alert_should_fire_ex(
               prev_conf,
               (uint8_t)entry->confidence,
               entry->alerted,
               was_archived,
               now,
               app->alert_last_tick,
               app->alert_have_fired,
               flock_alert_min_conf_rung(app->settings.alert_min_conf))) {
            entry->alerted = true;
            app->alert_pending = true;
            app->alert_last_tick = now;
            app->alert_have_fired = true;
            // Same event, so the card can never announce a different device
            // from the one that just beeped. The MAC rather than the index:
            // the table can evict an archived slot out from under an index,
            // and pointing the card at the wrong row would be worse than
            // showing no card at all.
            memcpy(app->alert_card_mac, entry->mac, 6);
            app->alert_card_tick = now;
        }
    }

    furi_mutex_release(app->mutex);
}

/**
 * Grace period before we conclude the board is unpowered rather than merely
 * slow. The companion sends its banner within a few hundred ms of coming up, so
 * this only has to outlast a boot we did not cause. Deliberately not shorter:
 * declaring a live board dead and switching a second supply into it is the one
 * outcome this whole feature has to avoid.
 */
#define ESP_LINK_GRACE_MS 2500u

void recon_app_esp_power_tick(ReconApp* app) {
    // A rail we raised that is no longer up means the firmware's power service
    // saw a real fault and dropped it. Report it once, rather than sitting on
    // "waiting for ESP" forever behind a rail that is not actually on.
    //
    // The is_otg_enabled() read talks to the charger over I2C, so it happens
    // between the two locks and never underneath one -- same reason the enable
    // path below releases the mutex before touching the HAL.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool watch = app->otg_on_by_us && !app->otg_failed;
    furi_mutex_release(app->mutex);
    if(watch && !furi_hal_power_is_otg_enabled()) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->otg_on_by_us = false;
        app->otg_failed = true;
        furi_mutex_release(app->mutex);
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    bool act = false;
    // ONLY WHILE WE ARE ACTUALLY LISTENING. app->esp is the live UART session,
    // and it is opened by a scanning scene, not at app start. Without this gate
    // the grace period below would expire on the MAIN MENU -- where nothing has
    // opened the port yet, so esp_connected is false for a reason that has
    // nothing to do with power -- and we would energise the rail under a board
    // that is perfectly healthy on its own USB. That is precisely the case this
    // feature is supposed to never touch.
    if(!app->esp) app->esp_link_wait_tick = 0; // measure listening time, not wall time
    if(app->esp && app->settings.esp_auto_5v && !app->otg_attempted) {
        if(app->esp_connected) {
            // It is alive on somebody else's power. Nothing to do, ever, this run.
            app->otg_attempted = true;
        } else if(!app->esp_link_wait_tick) {
            app->esp_link_wait_tick = furi_get_tick();
        } else if((uint32_t)(furi_get_tick() - app->esp_link_wait_tick) >= ESP_LINK_GRACE_MS) {
            app->otg_attempted = true; // one shot regardless of the outcome
            act = true;
        }
    }
    furi_mutex_release(app->mutex);

    if(!act) return;

    // Outside the lock: these are HAL calls that talk to the charger IC over
    // I2C, and holding the app mutex across them would stall the ESP worker.
    // USB ATTACHED: DO NOTHING, AND SAY NOTHING.
    //
    // The charger cannot run the 5V boost while VBUS is being supplied by a
    // host -- OTG and charging are mutually exclusive on this part. Found the
    // hard way: on a tethered Flipper the enable silently does not take, and the
    // first version of this reported "5V refused" on a perfectly healthy device.
    // Every user who runs the app plugged into a laptop or a power bank would
    // have seen that, every time, for a fault that does not exist.
    //
    // Nothing is lost by standing down here. With VBUS present the header's 5V
    // is fed from it, so a board that needs 5V already has it. The case this
    // feature exists for -- h00die's -- is a Flipper on battery, where the boost
    // is the only source and does come up.
    if(furi_hal_power_get_usb_voltage() > 4.0f) {
        // RE-ARM rather than consume the one attempt. Standing down here is not
        // a decision about the board, it is a decision about this moment: unplug
        // the cable and the boost becomes available and relevant. Consuming the
        // attempt would mean a Flipper that was charging when the app opened
        // never powers the rail for the rest of the session, which is exactly
        // the "plug in to top up, then unplug and go" case.
        //
        // Resetting the wait tick too keeps the I2C read to once per grace
        // period rather than once per tick.
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->otg_attempted = false;
        app->esp_link_wait_tick = 0;
        furi_mutex_release(app->mutex);
        return;
    }

    if(furi_hal_power_is_otg_enabled()) {
        // The user switched it on themselves. Leave it entirely alone -- in
        // particular do NOT set otg_on_by_us, or exiting the app would turn off
        // a rail we never raised.
        return;
    }

    bool ok = furi_hal_power_enable_otg();
    // DO NOT READ THE FAULT REGISTER HERE. The first version did, reasoning that
    // a boost which comes up and instantly faults would otherwise look healthy.
    // On real hardware that check fired EVERY time, and the app reported
    // "5V refused" on a device whose rail switches on perfectly from the CLI.
    //
    // The charger LATCHES faults and clears them on read, so the first read after
    // any earlier toggling returns a stale bit that has nothing to do with the
    // enable just issued. It is also read microseconds after the boost was asked
    // to start, before it could have settled either way.
    //
    // A real fault is still caught, just not here. The firmware's own power
    // service polls furi_hal_power_check_otg_status() and drops the rail when one
    // occurs; the watchdog at the top of this function sees the rail vanish and
    // reports it then. Fewer things for this app to second-guess the platform on.

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(ok) {
        app->otg_on_by_us = true;
    } else {
        // Surfaced rather than retried. Retrying a boost that just faulted is
        // how you cook a board, and the operator can see the reason on screen.
        app->otg_failed = true;
    }
    furi_mutex_release(app->mutex);
}

void recon_app_esp_power_release(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool ours = app->otg_on_by_us;
    app->otg_on_by_us = false; // idempotent: a second call is a no-op
    furi_mutex_release(app->mutex);
    if(ours) furi_hal_power_disable_otg();
}

void recon_app_alert_tick(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool pending = app->alert_pending;
    app->alert_pending = false;
    uint8_t mode = app->settings.alert_mode;
    bool sound = app->settings.sound;
    furi_mutex_release(app->mutex);

    // Fire outside the lock: notification_message queues work for the
    // notification service and must not stall the ESP worker behind it.
    if(pending) {
        recon_alert_fire(app->notifications, mode, sound);
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->alert_fired++;
        furi_mutex_release(app->mutex);
    }
}

void recon_app_set_esp_status(
    ReconApp* app,
    uint32_t frames,
    uint32_t hits,
    uint8_t channel,
    bool connected) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_connected = connected;
    // (0,0,0) is a keepalive/banner; don't clobber real counters with it.
    if(!(frames == 0 && hits == 0 && channel == 0)) {
        // The companion sends lifetime totals. Rebase per session so the count
        // restarts at 0 each scan. The first status line after a scene enter
        // captures the base; a drop (ESP rebooted -> counter reset) re-bases too.
        if(app->esp_rebase) {
            app->esp_frames_base = frames;
            app->esp_hits_base = hits;
            app->esp_rebase = false;
        }
        // A LIFETIME total can only fall if the board restarted. That was silently
        // absorbed by the rebase below, so the on-screen count just slid back
        // toward zero -- reported as a cosmetic oddity on a long drive when it
        // actually meant the ESP was resetting and dropping detections (issue #5).
        // Count it so the header can say so.
        if(frames < app->esp_frames_base) {
            app->esp_reboots++;
            app->esp_frames_prev = 0;
            app->esp_rate_tick = 0;
            app->esp_frame_rate = -1;
            app->esp_frames_base = frames;
        }
        if(hits < app->esp_hits_base) app->esp_hits_base = hits;
        app->esp_frames = frames - app->esp_frames_base;
        app->esp_hits = hits - app->esp_hits_base;
        app->esp_channel = channel;

        // Frames per second, sampled between status lines (~1 Hz). The cumulative
        // total answers "is the link up"; only a rate answers "is this thing
        // hearing anything right now", which is the question you have while
        // parked next to a camera that is not showing up.
        uint32_t now = furi_get_tick();
        if(app->esp_rate_tick) {
            uint32_t elapsed = now - app->esp_rate_tick;
            if(elapsed >= 500) { // don't divide a burst of lines into a wild rate
                app->esp_frame_rate = esp_frames_rate(app->esp_frames_prev, frames, elapsed);
                app->esp_frames_prev = frames;
                app->esp_rate_tick = now;
            }
        } else {
            app->esp_frames_prev = frames;
            app->esp_rate_tick = now;
        }
    }
    furi_mutex_release(app->mutex);
}

void recon_app_set_esp_lines(ReconApp* app, uint32_t lines) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_lines = lines;
    app->esp_connected = true;
    furi_mutex_release(app->mutex);
}

void recon_app_set_esp_proto(ReconApp* app, uint8_t version, bool mismatch) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_proto_version = version;
    app->esp_proto_mismatch = mismatch;
    furi_mutex_release(app->mutex);
}

void recon_app_set_esp_dropped(ReconApp* app, uint32_t dropped) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_dropped_lines = dropped;
    furi_mutex_release(app->mutex);
}

void recon_app_set_esp_link_state(ReconApp* app, EspLinkState state) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_link_state = (uint8_t)state;
    furi_mutex_release(app->mutex);
}

void recon_app_set_gps_relay(ReconApp* app, bool on, int16_t pin, uint32_t baud) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->gps_relay = on ? ReconGpsRelayOn : ReconGpsRelayOff;
    app->gps_relay_pin = pin;
    app->gps_relay_baud = baud;
    furi_mutex_release(app->mutex);
}

void recon_app_gps_relay_pending(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->gps_relay = ReconGpsRelayUnknown;
    app->gps_relay_pin = -1;
    app->gps_cfg_tick = furi_get_tick();
    furi_mutex_release(app->mutex);
}

void recon_app_set_chip(
    ReconApp* app,
    const char* target,
    uint8_t gpio_count,
    uint64_t gps_pin_mask,
    bool has_5ghz) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    strncpy(app->esp_chip, target ? target : "", sizeof(app->esp_chip) - 1);
    app->esp_chip[sizeof(app->esp_chip) - 1] = '\0';
    app->esp_gpio_count = gpio_count;
    app->esp_gps_pin_mask = gps_pin_mask;
    app->esp_has_5ghz = has_5ghz;
    furi_mutex_release(app->mutex);
}

void recon_app_set_band(ReconApp* app, uint8_t sel, uint16_t channels) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_band_actual = sel;
    app->esp_band_channels = channels;
    furi_mutex_release(app->mutex);
}

void recon_app_request_gps_cfg(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->gps_cfg_resend = true;
    furi_mutex_release(app->mutex);
}

void recon_app_gps_cfg_tick(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool want = app->gps_cfg_resend;
    app->gps_cfg_resend = false;
    furi_mutex_release(app->mutex);
    // Sent from the GUI thread only. The ESP worker raises the flag; if it did
    // the furi_hal_serial_tx itself it would race the commands this same thread
    // sends on entering a scan scene, on one UART handle.
    if(want && app->esp) {
        esp_link_send_band(app->esp);
        esp_link_send_gps_cfg(app->esp);
    }
}

void recon_app_ble_scan_done(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_ble_scans++;
    furi_mutex_release(app->mutex);
}

void recon_app_ble_begin(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->ble_scanning = true;
    app->ble_done = false;
    app->esp_connected = true;
    furi_mutex_release(app->mutex);
}

void recon_app_ble_add(
    ReconApp* app,
    const uint8_t addr[6],
    const char* name,
    int8_t rssi,
    uint8_t cat,
    uint16_t company,
    const uint8_t* mfg,
    size_t mfg_len,
    bool raven_gatt) {
    // Decode the Flock 0x09C8 external-battery advert: extract the device serial
    // and a model guess. The serial/battery advert is shared Falcon/Raven and
    // stays Generic; a Raven is only asserted when the companion saw its
    // Raven-specific GATT services (raven_gatt) -- see flock_ble_model_ex.
    // Done outside the lock (pure string work, no app state).
    // Flipper Zero detection (app-side, firmware-independent): a stock Flipper
    // advertises its BLE GAP name as "Flipper <name>" -- the signature every
    // reference Flipper detector keys on. Only claim it when nothing stronger
    // already classified the device (a real tracker keeps its category).
    if(cat == BleCatUnknown && name && strncmp(name, "Flipper", 7) == 0) {
        cat = BleCatFlipper;
    }

    // Every advert counts toward the BLE liveness figure, whether or not it is a
    // device we care about or one we already have. The Flock screen showed
    // NOTHING about BLE, so in flockcombo mode a working BLE half and one that
    // never ran looked identical -- and BLE is usually the easy detection on
    // these cameras (issue #5). Counted separately from ble_count, which is the
    // deduplicated table and stops growing once the area is stale.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_ble_seen++;
    furi_mutex_release(app->mutex);

    char serial[RECON_BLE_SERIAL_LEN] = "";
    uint8_t model = FlockBleModelUnknown;
    if(cat == BleCatFlock) {
        flock_ble_extract_serial(mfg, mfg_len, name, serial, sizeof(serial));
        model = (uint8_t)flock_ble_model_ex(serial, name, raven_gatt);
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    uint32_t now = furi_get_tick();
    BleDevice* e = NULL;
    for(size_t i = 0; i < app->ble_count; i++) {
        if(memcmp(app->ble[i].addr, addr, 6) == 0) {
            e = &app->ble[i];
            break;
        }
    }
    if(!e && app->ble_count < RECON_BLE_MAX) {
        e = &app->ble[app->ble_count++];
        memset(e, 0, sizeof(BleDevice));
        memcpy(e->addr, addr, 6);
        e->first_lat = app->gps_valid ? app->gps_lat : NAN;
        e->first_lon = app->gps_valid ? app->gps_lon : NAN;
        e->first_tick = now;
        e->last_tick = now;
    }
    if(e) {
        e->count++;
        e->rssi = rssi;
        // Freshness is a SIGHTING fact, not a location fact: this must advance on
        // every sighting, with or without a GPS fix. It used to live inside the
        // `if(app->gps_valid)` block below, so with GPS off (the default) it never
        // advanced past entry creation and every last_tick consumer silently
        // expired ~90 s into a session -- the WATCHSCORE flipper_near signal, the
        // Guardian "Flip N" counter, the anomaly freshness window, and the BLE
        // detail "FOLLOWING ... over %lus" readout (which always printed 0s).
        e->last_tick = now;
        if(cat) e->cat = cat;
        e->company = company;
        if(name && name[0] && e->name[0] == '\0') {
            strncpy(e->name, name, RECON_SSID_LEN - 1);
            e->name[RECON_SSID_LEN - 1] = '\0';
        }
        if(serial[0] && e->serial[0] == '\0') {
            strncpy(e->serial, serial, RECON_BLE_SERIAL_LEN - 1);
            e->serial[RECON_BLE_SERIAL_LEN - 1] = '\0';
        }
        if(model && model != FlockBleModelUnknown) e->model = model;
        if(app->gps_valid) {
            e->last_lat = app->gps_lat;
            e->last_lon = app->gps_lon;
        }
    }
    furi_mutex_release(app->mutex);

    // A BLE-classified Flock/Raven device is also a Flock detection -> merge it
    // into the Flock list (ftype 'L' = BLE) so it shows alongside WiFi hits,
    // gets geotagged, and lands in reports. (Done after releasing the mutex;
    // recon_app_report_flock takes it itself.)
    if(cat == BleCatFlock || cat == BleCatAxon) {
        // Flock tells (0x09C8 battery, Penguin naming, Raven GATT) are ALPR-class;
        // Axon's own SIG company id is body/in-car kit, which is a different thing
        // entirely and must not be reported as a camera on a pole. SoundThinking
        // is a WiFi-side OUI match only -- no BLE signature for it is known.
        //
        // Re-derive the rung rather than asserting Confirmed. The companion also
        // sets cat=1 on a bare OUI-prefix match against SHARED silicon-vendor
        // ranges, so a hardcoded Confirmed here announced ordinary ESP32 hardware
        // as a confirmed camera. See flock_ble_confidence().
        recon_app_report_flock(
            app,
            addr,
            name,
            rssi,
            0,
            'L',
            flock_ble_confidence(company, name, raven_gatt),
            0,
            (cat == BleCatAxon) ? FlockClassBodycam : FlockClassAlpr,
            false);
    }
}

void recon_app_ble_end(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->ble_scanning = false;
    app->ble_done = true;
    furi_mutex_release(app->mutex);
}

void recon_app_wifi_begin(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->wifi_count = 0;
    app->wifi_scanning = true;
    app->wifi_done = false;
    app->esp_connected = true;
    furi_mutex_release(app->mutex);
}

void recon_app_wifi_add(
    ReconApp* app,
    const uint8_t bssid[6],
    const char* ssid,
    int8_t rssi,
    uint8_t channel,
    uint8_t authmode,
    uint8_t pairwise,
    bool wps) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->wifi_count < RECON_WIFI_MAX) {
        WifiAp* ap = &app->wifi[app->wifi_count++];
        memset(ap, 0, sizeof(WifiAp));
        memcpy(ap->bssid, bssid, 6);
        if(ssid) {
            strncpy(ap->ssid, ssid, RECON_SSID_LEN - 1);
            ap->ssid[RECON_SSID_LEN - 1] = '\0';
        }
        ap->rssi = rssi;
        ap->channel = channel;
        ap->authmode = authmode;
        ap->pairwise = pairwise;
        ap->wps = wps;
    }
    furi_mutex_release(app->mutex);
}

void recon_app_wifi_end(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->wifi_scanning = false;
    app->wifi_done = true;
    furi_mutex_release(app->mutex);
}

#define LOCATE_TREND_DB 2 /**< dB vs the smoothed average before we call it warmer/colder */

void recon_app_set_locate_rssi(ReconApp* app, int8_t rssi) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->locate_rssi = rssi;
    app->locate_tick = furi_get_tick();
    app->locate_have = true;
    app->esp_connected = true;
    // Fold peak/EMA/trend on EVERY LOC line, not just on redraw -- LOC arrives
    // faster than the display ticks, so folding in the draw callback dropped
    // transient peaks between frames. rssi >= 0 is the reset/invalid sentinel.
    if(rssi < 0) {
        if(!app->locate_init) {
            app->locate_peak = rssi;
            app->locate_ema = (float)rssi;
            app->locate_trend = 0;
            app->locate_init = true;
        } else {
            if(rssi > app->locate_peak) app->locate_peak = rssi;
            float d = (float)rssi - app->locate_ema;
            app->locate_trend =
                (int8_t)((d >= LOCATE_TREND_DB) ? 1 : (d <= -LOCATE_TREND_DB ? -1 : 0));
            // Weighted 50/50, not 70/30. On the primary target -- a Flock camera
            // -- readings land about once every 1.6 s, because the camera hops
            // channels while probing and we listen on one. At 0.3 a new sample
            // took five readings (~8 s) to move the needle, so the meter lagged
            // far behind where the operator was standing. Each sample is scarce
            // here, so each one has to count for more.
            app->locate_ema = app->locate_ema * 0.5f + (float)rssi * 0.5f;
        }
    }
    furi_mutex_release(app->mutex);
}

// ---- settings ------------------------------------------------------------

static void recon_settings_defaults(ReconApp* app) {
    app->settings.backend = EspBackendCompanion;
    app->settings.esp_band = ReconEspBand24; // detection first -- see ReconEspBand
    app->settings.esp_uart = FuriHalSerialIdUsart;
    app->settings.gps_uart = FuriHalSerialIdLpuart;
    app->settings.esp_baud = 115200;
    app->settings.gps_baud = 9600;
    app->settings.marauder_cmd = 0; // sniffprobe
    app->settings.gps_enabled = false; // off by default
    app->settings.gps_source = ReconGpsSourceFlipper; // the wiring the docs describe
    app->settings.esp_gps_pin = 16; // a common GPS RX on ESP32 carrier boards
    app->settings.sound = true;
    app->settings.alert_mode = ReconAlertVibro; // haptic-first, like the ELEVATED alert
    app->settings.alert_min_conf = AlertConfLikely; // precision over recall stays the default
    app->settings.flash_fast = false; // safe 115200 by default
    app->settings.esp_auto_5v = true; // a board on the header is dead without it
    app->settings.save_hits = false; // privacy: a hit log is a record of where you have been
    app->settings.log_serials = false; // privacy: don't catalogue police asset serials by default
}

void recon_settings_save(ReconApp* app) {
    recon_report_ensure_dirs(app);
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, RECON_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* s = furi_string_alloc();
        furi_string_printf(
            s,
            "backend=%d\nesp_band=%d\nesp_uart=%d\ngps_uart=%d\nesp_baud=%lu\ngps_baud=%lu\nmarauder_cmd=%d\ngps_enabled=%d\ngps_source=%d\nesp_gps_pin=%d\nsound=%d\nflash_fast=%d\nlog_serials=%d\nalert_mode=%d\nalert_min_conf=%d\nsave_hits=%d\nesp_auto_5v=%d\n",
            app->settings.backend,
            app->settings.esp_band,
            app->settings.esp_uart,
            app->settings.gps_uart,
            (unsigned long)app->settings.esp_baud,
            (unsigned long)app->settings.gps_baud,
            app->settings.marauder_cmd,
            app->settings.gps_enabled ? 1 : 0,
            app->settings.gps_source,
            app->settings.esp_gps_pin,
            app->settings.sound ? 1 : 0,
            app->settings.flash_fast ? 1 : 0,
            app->settings.log_serials ? 1 : 0,
            app->settings.alert_mode,
            app->settings.alert_min_conf,
            app->settings.save_hits ? 1 : 0,
            app->settings.esp_auto_5v ? 1 : 0);

        storage_file_write(file, furi_string_get_cstr(s), furi_string_size(s));
        furi_string_free(s);
    }
    storage_file_close(file);
    storage_file_free(file);
}

/**
 * @param raw  the value text: everything after the FIRST '=' on the line. Two
 *             settings are strings rather than numbers, and splitting on the
 *             first '=' is what lets an SSID legally containing '=' round-trip.
 */
static void recon_settings_apply_kv(ReconApp* app, const char* key, long val, const char* raw) {
    (void)raw; // cameras-only: the only string-valued key (guard_ssid) was removed
    if(strcmp(key, "backend") == 0)
        app->settings.backend = (val == EspBackendGeneric) ? EspBackendGeneric :
                                                             EspBackendCompanion;
    else if(strcmp(key, "esp_uart") == 0)
        app->settings.esp_uart = (val == FuriHalSerialIdLpuart) ? FuriHalSerialIdLpuart :
                                                                  FuriHalSerialIdUsart;
    else if(strcmp(key, "gps_uart") == 0)
        app->settings.gps_uart = (val == FuriHalSerialIdUsart) ? FuriHalSerialIdUsart :
                                                                 FuriHalSerialIdLpuart;
    else if(strcmp(key, "esp_baud") == 0 && (val == 115200 || val == 921600))
        app->settings.esp_baud = (uint32_t)val; // clamp to known-valid; corrupt -> keep default
    else if(strcmp(key, "gps_baud") == 0 && (val == 9600 || val == 57600 || val == 115200))
        app->settings.gps_baud = (uint32_t)val;
    else if(strcmp(key, "marauder_cmd") == 0 && val >= 0 && val < 4)
        app->settings.marauder_cmd = (uint8_t)val;
    else if(strcmp(key, "gps_enabled") == 0)
        app->settings.gps_enabled = (val != 0);
    else if(strcmp(key, "esp_band") == 0 && val >= 0 && val < ReconEspBandCount)
        app->settings.esp_band = (uint8_t)val;
    else if(strcmp(key, "gps_source") == 0 && val >= 0 && val < ReconGpsSourceCount)
        app->settings.gps_source = (uint8_t)val; // corrupt value -> keep the default
    else if(strcmp(key, "esp_gps_pin") == 0 && val > 1 && val != 3 && val < 48)
        app->settings.esp_gps_pin = (uint8_t)val; // same range the companion accepts
    else if(strcmp(key, "sound") == 0)
        app->settings.sound = (val != 0);
    else if(strcmp(key, "flash_fast") == 0)
        app->settings.flash_fast = (val != 0);
    else if(strcmp(key, "log_serials") == 0)
        app->settings.log_serials = (val != 0);
    else if(strcmp(key, "alert_mode") == 0 && val >= 0 && val < ReconAlertModeCount)
        app->settings.alert_mode = (uint8_t)val; // corrupt value -> keep the default
    else if(strcmp(key, "alert_min_conf") == 0 && val >= 0 && val < AlertConfCount)
        app->settings.alert_min_conf = (uint8_t)val; // ditto -- range-checked, not trusted
    else if(strcmp(key, "save_hits") == 0)
        app->settings.save_hits = (val != 0);
    else if(strcmp(key, "esp_auto_5v") == 0)
        app->settings.esp_auto_5v = (val != 0);
}

void recon_settings_load(ReconApp* app) {
    recon_settings_defaults(app);
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, RECON_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // One read covers the whole file. The settings file is 14 short key=value
        // lines (~190 B today); keep generous headroom so adding keys later can't
        // silently truncate the load (anything past the buffer is dropped).
        char buf[512];
        size_t n = storage_file_read(file, buf, sizeof(buf) - 1);
        buf[n] = '\0';
        char* line = buf;
        while(line && *line) {
            char* nl = strchr(line, '\n');
            if(nl) *nl = '\0';
            char* eq = strchr(line, '=');
            if(eq) {
                *eq = '\0';
                recon_settings_apply_kv(app, line, strtol(eq + 1, NULL, 10), eq + 1);
            }
            line = nl ? nl + 1 : NULL;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

// ---- persisted hits (issue #2) -------------------------------------------
//
// Detections used to live only in RAM, so closing the app discarded them. The
// record format (and its host tests) live in helpers/flock_store.h; this is the
// file I/O, kept next to the settings load/save because it is the same idiom.
//
// Off by default: a hit log is a durable record of where you have been, which is
// exactly the sort of trail a tool for people evading surveillance should not
// create without being asked.

/** Chunk size for the streaming line reader. One record is < 192 B; this is a
 *  read granularity, not a line limit. */
#define HITS_CHUNK 256

/** Copy one FlockEntry into the POD record the store module serialises. */
static void recon_hits_rec_from_entry(FlockStoreRec* r, const FlockEntry* e) {
    memset(r, 0, sizeof(*r));
    memcpy(r->mac, e->mac, 6);
    strncpy(r->ssid, e->ssid, FLOCK_STORE_SSID_LEN - 1);
    r->ssid[FLOCK_STORE_SSID_LEN - 1] = '\0';
    r->rssi = e->rssi;
    r->channel = e->channel;
    r->ftype = e->ftype;
    r->conf = (uint8_t)e->confidence;
    r->dev_class = e->dev_class;
    r->hidden = e->hidden;
    r->ie_fp = e->ie_fp;
    r->lat = e->lat;
    r->lon = e->lon;
    r->heading = e->heading;
    r->count = e->count;
    r->marked = e->marked;
    r->epoch = e->seen_epoch;
}

void recon_hits_save(ReconApp* app) {
    if(!app->settings.save_hits) return;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    size_t total = app->flock_count;
    furi_mutex_release(app->mutex);
    // An empty table must NEVER remove the file from here.
    //
    // v0.53 made it do exactly that, reasoning that deleting the last entry
    // should delete the store. But this function runs on every scan-session exit,
    // so ANY code path that emptied the table in memory became permanent data
    // loss on disk -- and Net Guardian emptied it on entry. That combination cost
    // a user a drive's worth of detections with no way to get them back, which is
    // strictly worse than the stale file the delete was meant to avoid.
    //
    // Removal is now an explicit consequence of the operator deleting something,
    // and lives in recon_hits_save_after_delete() alone.
    if(total == 0) return;

    recon_report_ensure_dirs(app);

    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, RECON_HITS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, FLOCK_STORE_SCHEMA "\n", strlen(FLOCK_STORE_SCHEMA) + 1);
        storage_file_write(file, FLOCK_STORE_HEADER "\n", strlen(FLOCK_STORE_HEADER) + 1);

        // One record at a time, straight to the card. Never assemble the file in
        // RAM -- same reason the report writers stream (see recon_report.c).
        // Snapshotting per entry also means the lock is never held across an SD
        // write, so a still-running ESP worker can't stall behind the filesystem.
        char line[FLOCK_STORE_LINE_MAX];
        for(size_t i = 0; i < total; i++) {
            FlockStoreRec rec;
            bool have = false;
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            if(i < app->flock_count) {
                recon_hits_rec_from_entry(&rec, &app->flock[i]);
                have = true;
            }
            furi_mutex_release(app->mutex);
            if(!have) continue;

            size_t n = flock_store_fmt_line(line, sizeof(line), &rec);
            if(n) storage_file_write(file, line, n);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

void recon_hits_save_after_delete(ReconApp* app) {
    if(!app->settings.save_hits) return;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    size_t total = app->flock_count;
    furi_mutex_release(app->mutex);

    // The ONLY place an empty table removes the store, because here the emptiness
    // is the operator's explicit choice: they just deleted the last entry. Left
    // as a plain save, the stale file would restore everything on next launch and
    // the deletion would look like it had not worked.
    if(total == 0) {
        storage_common_remove(app->storage, RECON_HITS_PATH);
        return;
    }
    recon_hits_save(app);
}

/** Append one parsed record to the detection table as an archived entry. */
static void recon_hits_add(ReconApp* app, const FlockStoreRec* r) {
    if(app->flock_count >= RECON_FLOCK_MAX) return;
    FlockEntry* e = &app->flock[app->flock_count++];
    memset(e, 0, sizeof(FlockEntry));
    memcpy(e->mac, r->mac, 6);
    strncpy(e->ssid, r->ssid, RECON_SSID_LEN - 1);
    e->ssid[RECON_SSID_LEN - 1] = '\0';
    e->rssi = r->rssi;
    e->channel = r->channel;
    e->ftype = r->ftype;
    e->confidence = (FlockConfidence)r->conf;
    e->dev_class = r->dev_class;
    e->hidden = r->hidden;
    e->ie_fp = r->ie_fp;
    e->lat = r->lat;
    e->lon = r->lon;
    e->heading = r->heading;
    // The stored coordinate came from the sighting whose RSSI we saved, so seed
    // the hysteresis with it -- otherwise a weak first sighting this session
    // would immediately overwrite a good geotag.
    e->geotag_rssi = isnan(r->lat) ? 0 : r->rssi;
    e->count = r->count;
    e->marked = r->marked;
    e->seen_epoch = r->epoch;
    e->archived = true;
    // A restored hit must not buzz: the alert announces a NEW detection, and the
    // user has already been told about this one (possibly days ago).
    e->alerted = true;
    // first_tick / last_tick stay 0 and are meaningless for an archived entry.
    // Everything that ages an entry must gate on `archived` instead.
}

void recon_hits_load(ReconApp* app) {
    if(!app->settings.save_hits) return;

    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, RECON_HITS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char chunk[HITS_CHUNK];
        char line[FLOCK_STORE_LINE_MAX];
        size_t li = 0;
        bool overlong = false; // this line blew the buffer -> drop it whole
        bool schema_seen = false;
        bool abort = false;
        size_t n;

        // Streaming line splitter: storage has no getline, and reading the whole
        // file (up to ~11 KB) into RAM to split it would defeat the point.
        while(!abort && (n = storage_file_read(file, chunk, sizeof(chunk))) > 0) {
            for(size_t i = 0; i < n && !abort; i++) {
                char c = chunk[i];
                if(c != '\n') {
                    if(li < sizeof(line) - 1) {
                        line[li++] = c;
                    } else {
                        overlong = true;
                    }
                    continue;
                }
                line[li] = '\0';
                li = 0;
                bool bad = overlong;
                overlong = false;
                if(bad) continue;

                // Strip a CR so a file edited on a PC still loads.
                size_t len = strlen(line);
                if(len && line[len - 1] == '\r') line[--len] = '\0';
                if(len == 0) continue;

                if(!schema_seen) {
                    // An unrecognised first line means "ignore this file", never
                    // "parse it anyway and get the columns wrong". v1 and v2 are
                    // both readable; anything newer is not.
                    if(!flock_store_schema_supported(line)) {
                        abort = true;
                        break;
                    }
                    schema_seen = true;
                    continue;
                }
                if(line[0] == '#' || strcmp(line, FLOCK_STORE_HEADER) == 0) continue;

                FlockStoreRec rec;
                if(flock_store_parse_line(line, &rec)) recon_hits_add(app, &rec);
                if(app->flock_count >= RECON_FLOCK_MAX) abort = true; // table full
            }
        }
        // A final record with no trailing newline (a truncated write) still loads.
        if(!abort && schema_seen && li && !overlong) {
            line[li] = '\0';
            size_t len = strlen(line);
            if(len && line[len - 1] == '\r') line[--len] = '\0';
            FlockStoreRec rec;
            if(len && line[0] != '#' && flock_store_parse_line(line, &rec))
                recon_hits_add(app, &rec);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
}

void recon_hits_clear(ReconApp* app) {
    storage_common_remove(app->storage, RECON_HITS_PATH);

    // Drop the restored entries too. Leaving them on screen after "clear" would
    // imply the file is gone when the data plainly is not.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    size_t w = 0;
    for(size_t i = 0; i < app->flock_count; i++) {
        if(app->flock[i].archived) continue;
        if(w != i) app->flock[w] = app->flock[i];
        w++;
    }
    app->flock_count = w;
    if(app->selected >= (int)w) app->selected = w ? (int)w - 1 : 0;
    furi_mutex_release(app->mutex);
}

// ---- view dispatcher glue ------------------------------------------------

static bool recon_custom_event_callback(void* context, uint32_t event) {
    ReconApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool recon_back_event_callback(void* context) {
    ReconApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void recon_tick_event_callback(void* context) {
    ReconApp* app = context;
    // Announce a pending detection alert here, ONCE, for every scene.
    //
    // This used to be each scanning scene's job, and the Locator forgot: Lock In
    // (issue #6) keeps the ESP link live on the Locator screen, but that scene
    // was the only scanning one with no recon_app_alert_tick() call, so a camera
    // found while homing in set alert_pending and nothing ever consumed it. It
    // was reported as "alerts don't work" by the same person who asked for Lock
    // In (issue #5) -- the two features shipped in the same release and one
    // silently disabled the other.
    //
    // Hoisted to the dispatcher tick rather than adding a sixth per-scene call,
    // because "remember to call this in every new scene" is what failed. The
    // GUI thread owns delivery either way; the ESP worker only sets the flag.
    recon_app_alert_tick(app);
    // Bring the companion's power up if it never answered. Here for the same
    // reason the alert tick is here: every scene gets it, and no new scene has
    // to remember anything.
    recon_app_esp_power_tick(app);
    // Same worker-raises / GUI-delivers split, for the same reason: the companion
    // announces itself with a banner on every boot, and the relay config has to be
    // re-sent when it does (a board still coming up misses the one the scan
    // session sent). Doing that transmit on the worker thread would race this
    // thread's own commands on the same UART handle.
    recon_app_gps_cfg_tick(app);
    // Phone GPS: re-ask for the location stream while it is not delivering. Same
    // hoisted-to-the-dispatcher reasoning as the two above -- the ordinary case is
    // that the operator opens a scan screen and connects the phone afterwards, and
    // that must not need a per-scene call somebody forgets to add. A no-op when
    // the phone source is not selected, and on firmware without the service.
    gps_rpc_tick(app->gps_rpc);
    scene_manager_handle_tick_event(app->scene_manager);
}

// ---- lifecycle -----------------------------------------------------------

static ReconApp* recon_app_alloc(void) {
    ReconApp* app = malloc(sizeof(ReconApp));
    memset(app, 0, sizeof(ReconApp));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->fw_log = furi_string_alloc();
    app->gps_lat = NAN;
    app->gps_lon = NAN;
    app->gps_course = NAN;

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    recon_settings_load(app);

    // Restore previously saved detections (opt-in). Before the view dispatcher
    // exists, so the first screen already shows them.
    recon_hits_load(app);

    // Optional SD-loaded extra signatures, merged over the built-ins. Fail-safe:
    // a missing/malformed file leaves sig_db NULL and the built-ins intact.
    app->sig_db = sig_db_load(app->storage);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&recon_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, recon_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, recon_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, recon_tick_event_callback, RECON_TICK_MS);

    app->submenu = submenu_alloc();
    app->var_item_list = variable_item_list_alloc();
    app->widget = widget_alloc();
    app->popup = popup_alloc();
    app->flock_view = flock_view_alloc();
    flock_view_set_app(app->flock_view, app);
    app->flock_detail_view = flock_detail_view_alloc();
    flock_detail_view_set_app(app->flock_detail_view, app);
    app->flock_map_view = flock_map_view_alloc();
    flock_map_view_set_app(app->flock_map_view, app);
    app->deflock_qr_view = deflock_qr_view_alloc();
    deflock_qr_view_set_app(app->deflock_qr_view, app);
    app->locator_view = locator_view_alloc();
    locator_view_set_app(app->locator_view, app);

    view_dispatcher_add_view(
        app->view_dispatcher, ReconViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher,
        ReconViewVarItemList,
        variable_item_list_get_view(app->var_item_list));
    view_dispatcher_add_view(app->view_dispatcher, ReconViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(app->view_dispatcher, ReconViewPopup, popup_get_view(app->popup));
    view_dispatcher_add_view(
        app->view_dispatcher, ReconViewFlock, flock_view_get_view(app->flock_view));
    view_dispatcher_add_view(
        app->view_dispatcher,
        ReconViewFlockDetail,
        flock_detail_view_get_view(app->flock_detail_view));
    view_dispatcher_add_view(
        app->view_dispatcher, ReconViewFlockMap, flock_map_view_get_view(app->flock_map_view));
    view_dispatcher_add_view(
        app->view_dispatcher, ReconViewDeflockQr, deflock_qr_view_get_view(app->deflock_qr_view));
    view_dispatcher_add_view(
        app->view_dispatcher, ReconViewLocator, locator_view_get_view(app->locator_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void recon_app_free(ReconApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewFlock);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewFlockDetail);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewFlockMap);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewDeflockQr);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewLocator);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    popup_free(app->popup);
    flock_view_free(app->flock_view);
    flock_detail_view_free(app->flock_detail_view);
    flock_map_view_free(app->flock_map_view);
    deflock_qr_view_free(app->deflock_qr_view);
    locator_view_free(app->locator_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    sig_db_free(app->sig_db); // clears the extra-signature registration first
    furi_string_free(app->fw_log);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t recon_site_survey_app(void* arg) {
    UNUSED(arg);
    ReconApp* app = recon_app_alloc();

    scene_manager_next_scene(app->scene_manager, ReconSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    // Backstop for any exit path that does not pass back through the Main Menu:
    // the link is owned by the app, not by a scene, so the app is what must
    // guarantee the UART is released and the hits are written. Runs while
    // app->storage and app->mutex are still alive, and no-ops if the menu
    // already tore the session down.
    scan_session_stop(app);
    // Same backstop reasoning as the UART: the rail is owned by the app, so the
    // app is what must put it back. Leaving a boost converter running after exit
    // would quietly flatten the battery of someone who never switched it on.
    recon_app_esp_power_release(app);

    recon_app_free(app);
    return 0;
}

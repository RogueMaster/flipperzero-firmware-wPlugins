// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "recon_app_i.h"
#include <furi_hal_power.h>
#include <furi/core/memmgr_heap.h>
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
#include "helpers/open_drone_id.h"

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
    bool hidden,
    uint8_t probe_rate) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    // Counted BEFORE the confidence gate, so the diagnostic can separate "the
    // companion reported nothing" from "it reported plenty and we binned it".
    app->diag_flock_msgs++;
    if(confidence == FlockConfidenceNone) {
        app->diag_rej_conf++;
        furi_mutex_release(app->mutex);
        return;
    }

    // THE OPERATOR'S OWN HARDWARE, dropped before it can reach the table, be
    // geotagged, beep, or land in a report. Above every scoring tier on purpose:
    // the ladder answers "does this look like a camera", and this answers "the
    // person holding the Flipper went and looked, and it is their own router".
    // No confidence rung outranks that, including Confirmed -- an SSID pattern
    // is a guess about a stranger's device and this is a fact about the
    // operator's own. Counted so diag.csv can still say the device was heard,
    // which keeps an exclusion distinguishable from a radio that went deaf.
    if(flock_is_excluded(
           mac,
           ie_fp,
           app->ignore_macs,
           app->ignore_mac_count,
           app->ignore_fps,
           app->ignore_fp_count)) {
        app->diag_rej_ignored++;
        furi_mutex_release(app->mutex);
        return;
    }

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
            if(victim >= 0)
                entry = &app->flock[victim];
            else
                app->diag_rej_full++; // table full of live rows; this one is lost
        }
        if(entry) {
            memset(entry, 0, sizeof(FlockEntry));
            memcpy(entry->mac, mac, 6);
            entry->first_tick = now;
            // Weaker than any real reading, so the first sighting always wins
            // the channel. memset leaves this 0, and RSSI is negative dBm, so
            // 0 would mean "nothing can ever beat it" and the channel would be
            // frozen at whatever the first frame happened to carry.
            entry->chan_rssi = INT8_MIN;
            entry->lat = NAN;
            entry->lon = NAN;
            entry->heading = NAN;
            // Remote ID fields, NAN rather than 0 for the same reason as above:
            // 0/0 is a real place and would plot as one.
            entry->op_lat = NAN;
            entry->op_lon = NAN;
            entry->count = 0;
        }
    }

    if(entry) {
        app->diag_accepted++;
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
        // rssi tracks the LATEST sighting (it is a live proximity reading), but
        // the channel must track the STRONGEST one -- see FlockEntry.chan_rssi
        // for the measurement. Taking the latest let an off-channel fringe
        // capture overwrite the real channel, and `locate` then parked the
        // Locator's radio on it.
        if(channel != 0 && (rssi == 0 || rssi >= entry->chan_rssi)) {
            entry->channel = channel;
            if(rssi != 0) entry->chan_rssi = rssi;
            // Kept with the channel, from the SAME sighting, for the same
            // reason: both describe the moment the device was closest, and a
            // pair taken from two different moments describes neither.
            entry->probe_rate = probe_rate;
        }
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
        // First non-empty name wins, with ONE exception, below.
        //
        // Sticky is right for WiFi and must stay that way: on a probe request the
        // "ssid" is the network the device is LOOKING FOR, not its own name, so
        // letting a later sighting overwrite a beacon's real SSID with a probe
        // target would actively corrupt the row.
        //
        // BLE has no such ambiguity -- there the name is the device's own GAP
        // name -- and there it bit us. A device that advertises a stack default
        // first ("ESP32" from BLEDevice::init("")) and only later announces
        // "Penguin-..." was stuck displaying the meaningless name forever. That
        // is exactly how a correctly-Confirmed unit read as an unrelated gadget
        // on the bench and cost a day chasing a false positive that never was.
        // So on BLE only, a Flock-shaped name may replace a non-Flock-shaped one.
        // Monotonic by construction: Flock-shaped never reverts to generic, so it
        // cannot flap. The operator's own `label` is a separate field and always
        // wins in the UI regardless (views/flock_view.c).
        if(ssid && ssid[0]) {
            bool upgrade = (ftype == 'L') && entry->ssid[0] != '\0' &&
                           flock_ble_name_should_replace(entry->ssid, ssid);
            if(entry->ssid[0] == '\0' || upgrade) {
                strncpy(entry->ssid, ssid, RECON_SSID_LEN - 1);
                entry->ssid[RECON_SSID_LEN - 1] = '\0';
            }
        }
        // Geotag with the current fix per the hysteresis rule (haven't tagged yet,
        // or a meaningfully stronger sighting) -- see detect_rules.h.
        // A Remote ID position is the AIRCRAFT saying where IT is, to GPS
        // accuracy. Our geotag is where the OBSERVER was standing. Letting the
        // weaker fact overwrite the stronger one would silently turn a real
        // aircraft position into our own, which is both wrong and unnoticeable.
        if(!entry->pos_broadcast &&
           flock_geotag_should_update(
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

    // Something in the table changed, so the copy on the card is now stale.
    // recon_hits_autosave_tick() flushes it on an interval; before that existed
    // the only write was scan_session_stop(), and a flat battery mid-scan took
    // the whole session with it.
    app->hits_dirty = true;

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
    // USB ATTACHED: TRY ANYWAY, BUT STAY QUIET IF IT REFUSES.
    //
    // This used to return early whenever VBUS was above 4 V, on the stated
    // reasoning that "with VBUS present the header's 5V is fed from it, so a
    // board that needs 5V already has it".
    //
    // THAT REASONING IS WRONG, and it was measured wrong on 2026-09-07. With the
    // Flipper tethered to a PC the header was dead: the companion answered
    // nothing and the scan header read "ESP 0/s" with zero frames. A single
    // `power 5v 1` on the CLI brought the board up immediately, same cable, same
    // session. The Flipper does not pass VBUS through to pin 1; that rail is the
    // charger's boost either way, and asking for it works while plugged in.
    //
    // The cost of the old behaviour was the whole feature, for anyone who works
    // with the Flipper plugged in: the rail is off after every power cycle, the
    // app refused to raise it while tethered, and the board simply never came up.
    //
    // So try. The one thing to preserve from the old note is the SILENCE: the
    // charger genuinely cannot boost while it is drawing from VBUS on some
    // supplies, and the first version of this feature reported "5V refused" on
    // healthy hardware every time a user was charging. A refusal with VBUS
    // present is expected, so it re-arms quietly instead of raising a fault.
    bool vbus = furi_hal_power_get_usb_voltage() > 4.0f;

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
    } else if(vbus) {
        // Expected while the charger is drawing from VBUS. Re-arm silently so it
        // comes up the moment the cable is pulled, and do NOT show a fault: this
        // is the false "5V refused" that the old stand-down was written to avoid.
        app->otg_attempted = false;
        app->esp_link_wait_tick = 0;
    } else {
        // On battery a refusal is real. Surfaced rather than retried: retrying a
        // boost that just faulted is how you cook a board, and the operator can
        // see the reason on screen.
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
    if(connected) {
        app->esp_rx_tick = furi_get_tick();
        app->esp_lost = false;
    }
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
    app->esp_rx_tick = furi_get_tick();
    app->esp_lost = false;
    furi_mutex_release(app->mutex);
}

// How long the companion may go silent before the app stops claiming it is
// connected. The board sends a status line about once a second and BLE/survey
// traffic on top, so five seconds of nothing is a dead link, not a quiet one.
#define RECON_ESP_SILENT_MS 5000u
// A receiver reports at 1 Hz; five missed fixes is unplugged, not between
// sentences.
#define RECON_GPS_STALE_MS  5000u

void recon_app_liveness_tick(ReconApp* app) {
    uint32_t now = furi_get_tick();
    bool lost_now = false;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    // Companion. esp_connected was only ever set back to false on scene entry,
    // so a board that browned out or a header that worked loose mid-drive kept
    // showing "ESP" and the last frame rate, frozen, for as long as the screen
    // was up -- the detector reporting itself healthy while hearing nothing.
    if(app->esp && app->esp_connected && app->esp_rx_tick &&
       (now - app->esp_rx_tick) >= RECON_ESP_SILENT_MS) {
        app->esp_connected = false;
        app->esp_lost = true;
        app->esp_frame_rate = -1; // no honest rate from a silent board
        app->esp_rate_tick = 0;
        lost_now = true;
    }
    // GPS. Same failure, same shape: see gps_publish_fix().
    if(app->gps_valid && app->gps_fix_tick && (now - app->gps_fix_tick) >= RECON_GPS_STALE_MS) {
        app->gps_valid = false;
        app->gps_sats = 0;
    }
    furi_mutex_release(app->mutex);
    // One pulse, outside the lock, on the transition only. Not the configured
    // alert: that means "camera", and this means "your detector just stopped".
    if(lost_now && app->notifications) {
        notification_message(app->notifications, &sequence_single_vibro);
    }
}

void recon_app_set_esp_proto(ReconApp* app, uint8_t version, bool mismatch) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_proto_version = version;
    app->esp_proto_mismatch = mismatch;
    furi_mutex_release(app->mutex);
}

size_t recon_app_purge_excluded(ReconApp* app) {
    if(!app) return 0;
    size_t removed = 0;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    size_t w = 0;
    for(size_t i = 0; i < app->flock_count; i++) {
        const FlockEntry* e = &app->flock[i];
        bool gone = false;
        for(size_t k = 0; k < app->ignore_mac_count && !gone; k++) {
            gone = memcmp(app->ignore_macs[k], e->mac, 6) == 0;
        }
        for(size_t k = 0; e->ie_fp != 0 && k < app->ignore_fp_count && !gone; k++) {
            gone = app->ignore_fps[k] == e->ie_fp;
        }
        if(gone) {
            removed++;
            continue;
        }
        if(w != i) app->flock[w] = app->flock[i];
        w++;
    }
    app->flock_count = w;
    if(app->selected >= (int)w) app->selected = w ? (int)w - 1 : 0;
    if(removed) app->hits_dirty = true;
    furi_mutex_release(app->mutex);
    return removed;
}

bool recon_app_exclude_device(ReconApp* app, const uint8_t* mac, uint32_t fp, bool* out_by_fp) {
    if(out_by_fp) *out_by_fp = false;
    if(!app || !mac) return false;

    // CARD FIRST, THEN THE LOCK. The write reaches the storage service, and the
    // ESP worker takes app->mutex on every companion line, so holding the mutex
    // across a filesystem round-trip stalls the radio path behind the SD card --
    // the same discipline the hit menu uses for a learned signature.
    bool by_fp = false;
    bool ok = sig_db_ignore_add(app->storage, mac, fp, &by_fp);
    if(!ok) return false;

    uint32_t fps[SIG_IGNORED_MAX_FPS];
    uint8_t macs[SIG_IGNORED_MAX_MACS][6];
    size_t nf = 0, nm = 0;
    sig_db_ignored_read(
        app->storage, fps, SIG_IGNORED_MAX_FPS, macs, SIG_IGNORED_MAX_MACS, &nf, &nm);

    // RE-READ RATHER THAN APPEND IN PLACE, so memory and card cannot disagree.
    // What the gate enforces is then exactly what survives a restart, and a
    // write that was refused (a generic skeleton, a full table) cannot leave a
    // phantom entry live for the rest of the session.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    memcpy(app->ignore_fps, fps, sizeof(uint32_t) * nf);
    memcpy(app->ignore_macs, macs, sizeof(uint8_t[6]) * nm);
    app->ignore_fp_count = nf;
    app->ignore_mac_count = nm;
    furi_mutex_release(app->mutex);

    // AND TAKE IT OUT OF WHAT IS ALREADY ON THE LIST. The gate only stops the
    // NEXT sighting; the device's existing row -- and any other row carrying
    // the same fingerprint under a different address -- stayed in the table, in
    // hits.csv and in every report written from them.
    recon_app_purge_excluded(app);

    if(out_by_fp) *out_by_fp = by_fp;
    return true;
}

void recon_app_clear_exclusions(ReconApp* app) {
    if(!app) return;
    sig_db_forget_ignored(app->storage);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->ignore_fp_count = 0;
    app->ignore_mac_count = 0;
    furi_mutex_release(app->mutex);
}

void recon_app_set_esp_sig_revision(ReconApp* app, const char* revision) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    snprintf(app->esp_sig_revision, sizeof(app->esp_sig_revision), "%s", revision ? revision : "");
    app->esp_sig_mismatch = app->esp_sig_revision[0] != '\0' &&
                            strcmp(app->esp_sig_revision, FDF_SIGNATURE_REVISION) != 0;
    furi_mutex_release(app->mutex);
}

void recon_app_set_esp_sigtest(ReconApp* app, uint32_t hash, bool pass) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_sigtest_hash = hash;
    app->esp_sigtest_seen = true;
    app->esp_sigtest_pass = pass;
    furi_mutex_release(app->mutex);
}

void recon_app_set_ble_tell(ReconApp* app, const uint8_t mac[6], uint8_t tell) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    for(size_t i = 0; i < app->flock_count; i++) {
        if(memcmp(app->flock[i].mac, mac, 6) == 0) {
            // Sticky like the other evidence fields: a later sighting that only
            // saw the weaker signal must not overwrite the stronger one already
            // recorded. FlockBleTell is ordered strongest-first, so a lower
            // non-zero value wins.
            if(tell != 0 && (app->flock[i].ble_tell == 0 || tell < app->flock[i].ble_tell)) {
                app->flock[i].ble_tell = tell;
            }
            break;
        }
    }
    furi_mutex_release(app->mutex);
}

void recon_app_report_remote_id(
    ReconApp* app,
    const uint8_t addr[6],
    int8_t rssi,
    const uint8_t* payload,
    size_t payload_len) {
    OdidReport rep;
    odid_report_init(&rep);
    // Decode BEFORE taking the lock: this is pure work on a stack buffer and the
    // mutex is also held by the UI thread on every redraw.
    if(!odid_parse_ble_service_data(payload, payload_len, &rep)) return;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->diag_flock_msgs++;

    uint32_t now = furi_get_tick();
    FlockEntry* entry = NULL;
    for(size_t i = 0; i < app->flock_count; i++) {
        if(memcmp(app->flock[i].mac, addr, 6) == 0) {
            entry = &app->flock[i];
            break;
        }
    }

    if(!entry) {
        if(app->flock_count < RECON_FLOCK_MAX) {
            entry = &app->flock[app->flock_count++];
        } else {
            // Same eviction rule as recon_app_report_flock(): reclaim the least
            // valuable ARCHIVED row, never a live one.
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
            if(victim >= 0)
                entry = &app->flock[victim];
            else
                app->diag_rej_full++;
        }
        if(entry) {
            memset(entry, 0, sizeof(FlockEntry));
            memcpy(entry->mac, addr, 6);
            entry->first_tick = now;
            entry->lat = NAN;
            entry->lon = NAN;
            entry->heading = NAN;
            entry->op_lat = NAN;
            entry->op_lon = NAN;
        }
    }
    if(!entry) {
        furi_mutex_release(app->mutex);
        return;
    }

    app->diag_accepted++;
    uint8_t prev_conf = (uint8_t)entry->confidence;
    // Captured BEFORE the flag is cleared, same as recon_app_report_flock(): a
    // row restored from hits.csv loads with alerted=true, so without this the
    // alert rule saw "already announced" and a drone first heard today, after
    // a restart, never alerted at all -- issue #5's bug on the one class whose
    // detection is most time-critical.
    bool was_archived = entry->archived;
    entry->count++;
    entry->last_tick = now;
    entry->archived = false;
    if(rssi != 0 && (entry->rssi == 0 || rssi > entry->rssi)) entry->rssi = rssi;
    entry->ftype = 'L'; // arrived over BLE
    entry->dev_class = (uint8_t)FlockClassDrone;

    // CONFIRMED, and this is not the usual kind of claim. Every other detection
    // in the app is an inference from a shared vendor prefix or from frame
    // behaviour. This is the aircraft transmitting its own identity because 14
    // CFR Part 89 requires it to. What is confirmed is "a Remote ID broadcast was
    // received", i.e. something is flying and announcing itself -- NOT that it is
    // a police drone, which no signature could establish. The class says
    // "unmanned aircraft" and stops there.
    if((uint8_t)FlockConfidenceConfirmed > (uint8_t)entry->confidence) {
        entry->confidence = FlockConfidenceConfirmed;
    }

    // MERGE, never replace. One BLE legacy advert carries ONE message and the
    // aircraft cycles types, so the serial arrives in one advert and the operator
    // position in another. Overwriting on each advert would make both fields
    // flicker and would never show them together.
    if(rep.have_id) {
        if(rep.uas_id[0]) {
            strncpy(entry->ssid, rep.uas_id, sizeof(entry->ssid) - 1);
            entry->ssid[sizeof(entry->ssid) - 1] = '\0';
        }
        entry->ua_type = rep.ua_type;
    }
    if(!isnan(rep.lat) && !isnan(rep.lon)) {
        // The AIRCRAFT's own position, which is a strictly better fact than our
        // geotag of where we were standing. pos_broadcast records the difference
        // so the map and the report can say which one they are showing, and so
        // the geotag path stops overwriting it.
        entry->lat = rep.lat;
        entry->lon = rep.lon;
        entry->pos_broadcast = true;
    }
    if(!isnan(rep.op_lat) && !isnan(rep.op_lon)) {
        entry->op_lat = rep.op_lat;
        entry->op_lon = rep.op_lon;
    }

    entry->seen_epoch = furi_hal_rtc_get_timestamp();

    // Alert on the same path as every other detection -- set the flag here and
    // let the GUI tick raise it, because this runs on the ESP worker thread.
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
        memcpy(app->alert_card_mac, entry->mac, 6);
        app->alert_card_tick = now;
    }

    app->hits_dirty = true;
    furi_mutex_release(app->mutex);
}

void recon_app_survey_add(
    ReconApp* app,
    const uint8_t mac[6],
    uint32_t fp,
    int8_t rssi,
    uint8_t channel,
    uint16_t count,
    uint32_t fp2,
    const char* sig) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    SurveyEntry* e = NULL;
    // The count this row had after the PREVIOUS dump (0 = first time seen).
    // The companion sends running totals every poll, so "count went up" is the
    // only thing that says the device was actually heard again since.
    uint16_t prev_count = 0;
    bool known = false;
    for(size_t i = 0; i < app->survey_count; i++) {
        if(memcmp(app->survey[i].mac, mac, 6) == 0) {
            e = &app->survey[i];
            prev_count = e->count;
            known = true;
            break;
        }
    }
    if(!e) {
        if(app->survey_count < RECON_SURVEY_MAX) {
            e = &app->survey[app->survey_count++];
            memset(e, 0, sizeof(SurveyEntry));
            memcpy(e->mac, mac, 6);
        } else {
            // Full: drop the least-seen row rather than the newest. A persistent
            // emitter is the interesting one, and a camera is persistent.
            size_t victim = 0;
            for(size_t i = 1; i < app->survey_count; i++) {
                if(app->survey[i].count < app->survey[victim].count) victim = i;
            }
            if(app->survey[victim].count < count) {
                e = &app->survey[victim];
                memset(e, 0, sizeof(SurveyEntry));
                memcpy(e->mac, mac, 6);
            }
        }
    }
    if(e) {
        // The companion sends running totals, so take its value rather than
        // incrementing -- a dump repeated every interval would otherwise multiply
        // the count by the number of dumps.
        e->count = count;
        e->fp = fp;
        // Channel moves WITH the RSSI, never on its own. The companion now pairs
        // the two (see survey_note), and taking its channel while keeping a
        // different sighting's RSSI would pull the pair apart again on this
        // side. Air Survey feeds the Locator through Flag MAC, so a fringe
        // off-channel value here costs a hunt -- same failure as the detection
        // table's chan_rssi.
        if(rssi > e->rssi || e->rssi == 0) {
            e->rssi = rssi;
            e->channel = channel;
        }
        if(fp2) e->fp2 = fp2;
        // First non-empty wins: the signature describes the DEVICE, not the
        // sighting, so re-copying an identical 52-character string on every dump
        // would be pure work. An empty one means firmware older than v0.96.
        if(sig && sig[0] && !e->sig[0]) {
            strncpy(e->sig, sig, RECON_SURVEY_SIG_LEN - 1);
            e->sig[RECON_SURVEY_SIG_LEN - 1] = 0;
        }
    }
    furi_mutex_release(app->mutex);

    // THE SURVEY IS THE ONLY PLACE A LEARNED FINGERPRINT CAN EVER FIRE.
    //
    // The companion scores on OUI and SSID alone and returns early on conf == 0
    // (flock_companion.ino), and it computes the IE fingerprint AFTER that gate.
    // So a camera on a randomised or unlisted address is dropped on the ESP and
    // never reaches this side at all -- which meant a fingerprint from
    // signatures.json or learned.txt could only ever match a device we had
    // already recognised some other way. It could not fire on the one class of
    // device it exists for, and no amount of teaching would change that.
    //
    // The survey is not gated: the companion records every wildcard-probe
    // emitter, matched or not, and ships it on request. So the fingerprint the
    // operator taught us gets its comparison here, against the only feed that
    // carries the devices in question.
    //
    // Done AFTER the unlock: recon_app_report_flock takes the same mutex and it
    // is not recursive.
    // A PINNED ADDRESS lands here for the same reason a fingerprint does: the
    // camera it exists for has no vendor prefix, so the companion never forwards
    // it and the survey is the only feed carrying it.
    FlockConfidence fp_conf = flock_ie_fp_confidence(fp, mac);
    FlockConfidence pin_conf = flock_mac_pin_confidence(mac);
    if(pin_conf > fp_conf) fp_conf = pin_conf;
    // ONLY WHEN IT WAS HEARD AGAIN. Every 10 s dump repeats every row, so
    // reporting unconditionally re-announced a device that left minutes ago on
    // each poll: its sighting count climbed, its "last seen" stayed fresh, and
    // its row kept showing live bars for a transmitter that was long gone. A
    // row whose running total has not moved has not been heard since last time.
    if(known && count <= prev_count) return;
    if(fp_conf != FlockConfidenceNone) {
        // 'F' is the "probe-fp" source label, matching what the companion-line
        // parser stamps on a fingerprint match. No SSID, because a survey row has
        // none, and no class beyond the ALPR default, because a fingerprint says
        // "this stack" and never "this kind of device".
        recon_app_report_flock(
            app, mac, "", rssi, channel, 'F', fp_conf, fp, FlockClassAlpr, false, 0);
    }
}

static void recon_survey_write(ReconApp* app, bool append_log) {
    // Written even with ZERO rows, on purpose. "No file" is indistinguishable
    // from "the feature is broken" -- which is exactly how this landed on issue
    // #25, where short sessions produced nothing and the reporter could not tell
    // whether it had run. A header with no rows is a real answer: the survey ran
    // and nothing was probing.
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, RECON_APP_FOLDER);
    File* file = storage_file_alloc(storage);
    // Truncating rather than appending: this is a snapshot of what was in the air
    // during the last session, not a running history, and a stale row from a
    // different street would be actively misleading when hunting one camera.
    if(storage_file_open(file, RECON_SURVEY_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* out = furi_string_alloc();
        furi_string_cat_str(
            out,
            "# FlipDeFlock probe survey -- every wildcard-probe transmitter seen, matched or not\n"
            "# No SSID and no position. A high count next to a camera you can see is that camera.\n"
            "# ie_fp2 folds in the capability IE CONTENTS, not just their tag+length like ie_fp.\n"
            "# ie_sig is the same probe written out readably: an ordered IE tag list, with vendor\n"
            "# elements expanded. It is LAST on the row because it contains commas.\n"
            "mac,rssi,channel,ie_fp,count,ie_fp2,ie_sig\n");
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        for(size_t i = 0; i < app->survey_count; i++) {
            SurveyEntry* e = &app->survey[i];
            furi_string_cat_printf(
                out,
                "%02X:%02X:%02X:%02X:%02X:%02X,%d,%u,%08lx,%u,%08lx,%s\n",
                e->mac[0],
                e->mac[1],
                e->mac[2],
                e->mac[3],
                e->mac[4],
                e->mac[5],
                e->rssi,
                e->channel,
                (unsigned long)e->fp,
                (unsigned)e->count,
                (unsigned long)e->fp2,
                e->sig);
        }
        furi_mutex_release(app->mutex);
        storage_file_write(file, furi_string_get_cstr(out), furi_string_size(out));
        furi_string_free(out);
    }
    storage_file_close(file);
    storage_file_free(file);

    // The LOG APPEND IS NOT IDEMPOTENT, so it only happens at session end.
    // survey.csv above is a truncate-and-rewrite snapshot and can be repeated
    // safely; survey_log.csv appends a row group per session, and repeating it
    // would file the same stop several times over.
    if(append_log) recon_survey_log_append(app, storage);
    furi_record_close(RECORD_STORAGE);
}

void recon_survey_save(ReconApp* app) {
    recon_survey_write(app, true);
}

/**
 * Periodic snapshot of survey.csv while a scan is still running.
 *
 * WHY. Until now the survey reached the card only from scan_session_stop(), so
 * a flat battery or a wedged device mid-stop took the whole session's survey
 * with it -- while the DETECTIONS from that same session survived, because
 * recon_hits_autosave_tick() was added for exactly this failure and the survey
 * was never given the same treatment. Losing the survey is the worse half: it
 * is the file that explains a stop, and on a drive it is the thing that cost a
 * trip to collect.
 *
 * Only the snapshot is written. survey.csv is per-session by design, so its
 * contents at any moment ARE the session so far, and the file is a few KB, so
 * rewriting it on an interval costs nothing worth counting.
 */
void recon_survey_autosave_tick(ReconApp* app) {
    if(!app->esp) return; // no live session, nothing to snapshot
    uint32_t now = furi_get_tick();
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    size_t rows = app->survey_count;
    uint32_t last = app->survey_last_autosave;
    furi_mutex_release(app->mutex);
    if(rows == 0) return; // nothing measured yet; do not truncate a good file
    // Seeded on the first tick that HAS rows, so the interval is measured from
    // "there is something worth saving" rather than from scan start.
    if(last != 0 && (now - last) < RECON_SURVEY_AUTOSAVE_MS) return;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->survey_last_autosave = now;
    furi_mutex_release(app->mutex);
    if(last == 0) return; // first tick only seeds the clock
    recon_survey_write(app, false);
}

void recon_survey_log_append(ReconApp* app, void* storage_rec) {
    Storage* storage = storage_rec;
    // Nothing to add. An empty session is still recorded, in diag.csv, which is
    // the file that answers "did it run"; a session column with no rows under it
    // would only repeat that.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    size_t count = app->survey_count;
    uint32_t session = app->survey_session_epoch;
    furi_mutex_release(app->mutex);
    if(!count) return;

    // Rotate BEFORE appending, so the write that crosses the cap still lands in
    // the fresh file rather than being the last thing squeezed into a full one.
    // One generation only: the bound matters more than deep history, and the
    // recent drives are the ones anybody goes back to.
    File* probe = storage_file_alloc(storage);
    bool rotate = false;
    if(storage_file_open(probe, RECON_SURVEY_LOG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        rotate = storage_file_size(probe) >= RECON_SURVEY_LOG_MAX;
    }
    storage_file_close(probe);
    storage_file_free(probe);
    if(rotate) {
        storage_simply_remove(storage, RECON_SURVEY_LOG_OLD_PATH);
        storage_common_rename(storage, RECON_SURVEY_LOG_PATH, RECON_SURVEY_LOG_OLD_PATH);
    }

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, RECON_SURVEY_LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        FuriString* out = furi_string_alloc();
        if(storage_file_size(file) == 0) {
            furi_string_cat_str(
                out,
                "# FlipDeFlock survey log -- every session appended, newest last\n"
                "# session = scan start, as a unix time. Counts are PER SESSION, never\n"
                "# since the board booted, so they stay comparable within one row group.\n"
                "# No SSID and no position, same as survey.csv.\n"
                "# ie_fp2 folds in the capability IE contents; ie_sig is the same probe written\n"
                "# out readably. ie_sig is LAST on the row because it contains commas.\n"
                "session,mac,rssi,channel,ie_fp,count,ie_fp2,ie_sig\n");
        }
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        for(size_t i = 0; i < app->survey_count; i++) {
            SurveyEntry* e = &app->survey[i];
            furi_string_cat_printf(
                out,
                "%lu,%02X:%02X:%02X:%02X:%02X:%02X,%d,%u,%08lx,%u,%08lx,%s\n",
                (unsigned long)session,
                e->mac[0],
                e->mac[1],
                e->mac[2],
                e->mac[3],
                e->mac[4],
                e->mac[5],
                e->rssi,
                e->channel,
                (unsigned long)e->fp,
                (unsigned)e->count,
                (unsigned long)e->fp2,
                e->sig);
        }
        furi_mutex_release(app->mutex);
        storage_file_write(file, furi_string_get_cstr(out), furi_string_size(out));
        furi_string_free(out);
    }
    storage_file_close(file);
    storage_file_free(file);
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
    // HOLD THE RESEND WHILE THE LOCATOR IS HUNTING.
    //
    // `band` and `gpscfg` genuinely re-task the radio, so the companion is right
    // to cancel Locator mode when it sees them -- which means firing them mid-
    // hunt silently ends the hunt. The board stops streaming LOC and the meter
    // sits on "acquiring signal..." with nothing to explain it.
    //
    // It fires exactly when it does the most damage: the flag is raised by the
    // companion's boot banner, and opening the Locator on a fresh link is
    // precisely when the board is most likely to have just come up. The flag is
    // LEFT RAISED rather than dropped, so the relay config still gets re-sent --
    // one tick after the operator leaves this screen.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool hunting = app->locate_kind != 0;
    furi_mutex_release(app->mutex);
    if(hunting) return;

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
        // And the scan mode itself. The companion boots with g_scanning=true but
        // g_combo=false, i.e. Wi-Fi only; a brownout mid-drive therefore came
        // back up with BLE detection off and nothing on screen to say so. The
        // kickoff is an idempotent mode-select, so re-sending it is free.
        esp_link_send_kickoff(app->esp, NULL);
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
        // Only read the MANUFACTURER PAYLOAD as a Flock serial when it actually
        // is Flock's. The companion now sends mfghex for any cat=1 device that
        // carries manufacturer data -- not just 0x09C8 -- so a unit classified by
        // naming, Raven GATT or an OUI can arrive with some other vendor's blob,
        // and flock_ble_extract_serial() would happily report the longest
        // alphanumeric run in it as a device serial. The GAP-name fallback stays
        // unconditional: that path validates the name's own shape.
        bool flock_mfg = (company == FLOCK_BLE_COMPANY_ID);
        flock_ble_extract_serial(
            flock_mfg ? mfg : NULL, flock_mfg ? mfg_len : 0, name, serial, sizeof(serial));
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
    bool ble_new_row = false;
    if(e) {
        e->last_tick = now;
    } else if(app->ble_count < RECON_BLE_MAX) {
        e = &app->ble[app->ble_count++];
        ble_new_row = true;
    } else {
        // FULL: reuse the row heard longest ago, never a marked one. The table
        // is small on purpose (see RECON_BLE_MAX), and first-come-first-served
        // meant the first eight advertisers in range held it for the whole
        // session and nothing heard later could ever get a row.
        size_t v = RECON_BLE_MAX;
        for(size_t i = 0; i < app->ble_count; i++) {
            if(app->ble[i].marked) continue;
            if(v == RECON_BLE_MAX || app->ble[i].last_tick < app->ble[v].last_tick) v = i;
        }
        if(v < RECON_BLE_MAX) {
            e = &app->ble[v];
            ble_new_row = true;
        }
    }
    if(e && ble_new_row) {
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
        // Sticky, like cat/dev_class/ie_fp around it. A device advertises several
        // payloads in rotation, and only some carry manufacturer data; writing
        // this unconditionally let a later advert with none (which arrives as
        // BLE_COMPANY_NONE) erase a 0x09C8 we had already captured, throwing away
        // the strongest BLE evidence we get.
        if(company != BLE_COMPANY_NONE) e->company = company;
        // Same specificity upgrade as the Flock table above: a Flock-shaped name
        // may replace a generic one that was merely seen first. A single BLE
        // radio advertises several identities from ONE address (the bench emitter
        // does exactly this), so whichever advert happens to land first must not
        // get to name the device permanently.
        if(name && name[0]) {
            bool upgrade = e->name[0] != '\0' && flock_ble_name_should_replace(e->name, name);
            if(e->name[0] == '\0' || upgrade) {
                strncpy(e->name, name, RECON_SSID_LEN - 1);
                e->name[RECON_SSID_LEN - 1] = '\0';
            }
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
            // A Raven is Flock's ACOUSTIC gunshot sensor, positively identified
            // by its own GATT services, and flock_model_name() already spells it
            // "Flock Raven acoustic" for the acoustic class. Filing it as ALPR
            // put a microphone on the list as a plate reader.
            (cat == BleCatAxon) ? FlockClassBodycam :
            raven_gatt          ? FlockClassAcoustic :
                                  FlockClassAlpr,
            false,
            0); // BLE advert, not a probe request -- no probe rate exists
        // Record WHAT matched, alongside how sure we are. Two Confirmed rows can
        // rest on very different evidence -- 0x09C8 is the battery VENDOR's id,
        // the Raven GATT is Flock's own -- and the operator should be able to see
        // which. Does not touch the rung.
        recon_app_set_ble_tell(
            app, addr, (uint8_t)flock_ble_tell(company, name, raven_gatt, addr));
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
    // Beep AND vibrate. A camera you drove past is gone by the time you notice a
    // silent buzz in a pocket, and the whole point of the alert is to catch one
    // you were not watching the screen for. `sound` above still gates the beep,
    // and Flipper Notifications can silence it system-wide, so this is a louder
    // default rather than an unmutable one.
    app->settings.alert_mode = ReconAlertBoth;
    app->settings.alert_min_conf = AlertConfLikely; // precision over recall stays the default
    app->settings.flash_fast = false; // safe 115200 by default
    app->settings.esp_auto_5v = true; // a board on the header is dead without it
    // ON by default. This was off for privacy -- a hit log is a durable record of
    // where you have been -- but off by default meant the common case was losing a
    // whole drive's worth of detections on app exit, with nothing written to the
    // card and no warning that it had happened. Losing the data people go out to
    // collect is the worse failure. The toggle stays, and switching it back off
    // still deletes hits.csv, so opting out remains one switch away.
    app->settings.save_hits = true;
    app->settings.card_autodismiss = true; // unchanged behaviour by default
    app->settings.log_serials = false; // privacy: don't catalogue police asset serials by default
}

void recon_settings_save(ReconApp* app) {
    recon_report_ensure_dirs(app);
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, RECON_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* s = furi_string_alloc();
        furi_string_printf(
            s,
            "backend=%d\nesp_band=%d\nesp_uart=%d\ngps_uart=%d\nesp_baud=%lu\ngps_baud=%lu\nmarauder_cmd=%d\ngps_enabled=%d\ngps_source=%d\nesp_gps_pin=%d\nsound=%d\nflash_fast=%d\nlog_serials=%d\nalert_mode=%d\nalert_min_conf=%d\nsave_hits=%d\nesp_auto_5v=%d\ncard_autodismiss=%d\n",
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
            app->settings.esp_auto_5v ? 1 : 0,
            app->settings.card_autodismiss ? 1 : 0);

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
    else if(strcmp(key, "card_autodismiss") == 0)
        app->settings.card_autodismiss = (val != 0);
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
    r->op_lat = e->op_lat;
    r->op_lon = e->op_lon;
    r->ua_type = e->ua_type;
    r->hidden = e->hidden;
    r->ie_fp = e->ie_fp;
    r->lat = e->lat;
    r->lon = e->lon;
    r->heading = e->heading;
    r->count = e->count;
    r->marked = e->marked;
    r->confirmed = e->confirmed;
    snprintf(r->label, sizeof(r->label), "%s", e->label);
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

    // WRITE BESIDE, THEN REPLACE. This runs every 30 s during a scan, and it
    // used to truncate hits.csv in place -- so a flat battery or a crash in the
    // half-second the card was being written took the WHOLE history with it,
    // every previous session included, which is the one loss the autosave was
    // added to prevent. The temp file is complete and closed before the old
    // file is touched, and any failed write abandons it, leaving the previous
    // hits.csv exactly as it was.
    File* file = storage_file_alloc(app->storage);
    bool ok = false;
    if(storage_file_open(file, RECON_HITS_TMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(file, FLOCK_STORE_SCHEMA "\n", strlen(FLOCK_STORE_SCHEMA) + 1) ==
             strlen(FLOCK_STORE_SCHEMA) + 1;
        ok = ok &&
             storage_file_write(file, FLOCK_STORE_HEADER "\n", strlen(FLOCK_STORE_HEADER) + 1) ==
                 strlen(FLOCK_STORE_HEADER) + 1;

        // One record at a time, straight to the card. Never assemble the file in
        // RAM -- same reason the report writers stream (see recon_report.c).
        // Snapshotting per entry also means the lock is never held across an SD
        // write, so a still-running ESP worker can't stall behind the filesystem.
        char line[FLOCK_STORE_LINE_MAX];
        for(size_t i = 0; ok && i < total; i++) {
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
            if(n && storage_file_write(file, line, n) != n) ok = false;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    if(ok) {
        // The firmware's rename refuses an existing destination, so the old file
        // goes first. The gap between the two calls is microseconds, and the
        // replacement is already complete on the card when it opens.
        storage_common_remove(app->storage, RECON_HITS_PATH);
        storage_common_rename(app->storage, RECON_HITS_TMP_PATH, RECON_HITS_PATH);
    } else {
        storage_common_remove(app->storage, RECON_HITS_TMP_PATH);
    }
}

// How long a detection may sit in RAM before it reaches the card. The bound on
// what a flat battery or a crash can take with it. 30 s costs one rewrite of a
// <=64-row file per half minute during an active scan, which is nothing next to
// losing the drive.
#define RECON_HITS_AUTOSAVE_MS 30000u

// Short enough that a brief scan still collects something on its own, and far
// below anything that competes with detection traffic: a full 32-row dump is
// ~1.3 KB, which is about a tenth of a second of a 115200 link. The session-end
// dump in scan_session_stop() is what actually guarantees a file; this is the
// crash-safety net in between.
#define RECON_SURVEY_POLL_MS 10000u

void recon_survey_tick(ReconApp* app) {
    if(!app->esp) return; // no link, nothing to ask
    // NOT WHILE THE LOCATOR OWNS THE RADIO.
    //
    // This tick runs for EVERY scene, and on the companion any command that is
    // not `locate` cancels locate mode outright. So a poll fired ten seconds
    // into a hunt silently ended it: the board stopped streaming LOC, the meter
    // froze on "acquiring signal..." or decayed to "out of range", and nothing
    // on either side said why. Re-entering the Locator from a detection detail
    // kept the existing link, which left the poll clock already expired, so the
    // kill landed on the FIRST tick -- the Locator simply never worked at all
    // from that entry point.
    //
    // Measured on the bench: a WiFi target 30 cm away, beaconing on the locked
    // channel, never produced one reading across three attempts. With this skip
    // it locks on. The companion now also refuses to let `survey` cancel a hunt
    // (see flock_companion.ino), so this is belt and braces -- but the app must
    // not be asking for a table while it is asking the same radio to home.
    if(app->locate_kind) return;
    uint32_t now = furi_get_tick();
    if(app->survey_last_poll != 0 && (now - app->survey_last_poll) < RECON_SURVEY_POLL_MS) {
        return;
    }
    app->survey_last_poll = now;
    esp_link_send(app->esp, "survey");
}

void recon_hits_autosave_tick(ReconApp* app) {
    if(!app->settings.save_hits) return;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool dirty = app->hits_dirty;
    uint32_t last = app->hits_last_save;
    furi_mutex_release(app->mutex);

    if(!dirty) return;
    uint32_t now = furi_get_tick();
    // First flush of a session happens one full interval in, not instantly: a
    // scan that finds something in its first second would otherwise write on the
    // very next tick, before the table has settled.
    if(last != 0 && (now - last) < RECON_HITS_AUTOSAVE_MS) return;
    if(last == 0) {
        // Seed the clock on the first dirty tick so the interval is measured from
        // "first detection", not from app launch.
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->hits_last_save = now;
        furi_mutex_release(app->mutex);
        return;
    }

    // Clear the flag BEFORE writing. A detection that lands mid-write re-dirties
    // it and gets picked up next interval; clearing afterwards could swallow that
    // update instead, which is the one direction that loses data.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->hits_dirty = false;
    app->hits_last_save = now;
    furi_mutex_release(app->mutex);

    recon_hits_save(app);
}

void recon_diag_begin(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->diag_flock_msgs = 0;
    app->diag_accepted = 0;
    app->diag_rej_conf = 0;
    app->diag_rej_full = 0;
    app->diag_rej_ignored = 0;
    app->diag_start_epoch = furi_hal_rtc_get_timestamp();
    furi_mutex_release(app->mutex);
}

/**
 * True when diag.csv's first line is the CURRENT schema header, or the file does
 * not exist yet. False means it was written under an older column set and must
 * be rotated aside before anything new is appended.
 */
static bool recon_diag_header_current(Storage* storage) {
    const char* want = RECON_DIAG_HEADER_LINE;
    size_t want_len = strlen(want);
    File* file = storage_file_alloc(storage);
    bool current = true; // absent file -> nothing to rotate
    if(storage_file_open(file, RECON_DIAG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_size(file) > 0) {
            char buf[96];
            size_t n = want_len < sizeof(buf) ? want_len : sizeof(buf);
            size_t got = storage_file_read(file, buf, (uint16_t)n);
            current = (got == n) && (memcmp(buf, want, n) == 0);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return current;
}

void recon_diag_save(ReconApp* app) {
    // Never write a row for a session that never started (the Main Menu calls
    // scan_session_stop() on entry, including the one at launch).
    if(app->diag_start_epoch == 0) return;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    uint32_t start = app->diag_start_epoch;
    uint32_t msgs = app->diag_flock_msgs;
    uint32_t acc = app->diag_accepted;
    uint32_t rej_c = app->diag_rej_conf;
    uint32_t rej_f = app->diag_rej_full;
    // SUPPRESSED BY THE OPERATOR'S OWN EXCLUSION LIST. In a field report this is
    // the difference between "the radio heard nothing" and "it heard the thing
    // you are asking about and you told it to stay quiet". Somebody who excluded
    // a device weeks ago has no memory of doing it, and without this column a
    // report from that card cannot be explained from the outside.
    uint32_t rej_i = app->diag_rej_ignored;
    uint32_t lines = app->esp_lines;
    uint32_t dropped = app->esp_dropped_lines;
    uint32_t reboots = app->esp_reboots;
    uint32_t frames = app->esp_frames;
    uint32_t ehits = app->esp_hits;
    uint16_t band_ch = app->esp_band_channels;
    uint8_t band_act = app->esp_band_actual;
    uint8_t band_req = app->settings.esp_band;
    uint8_t backend = app->settings.backend;
    uint8_t proto = app->esp_proto_version;
    // The COMPANION's build, next to the app's. A field report that says only
    // which app version produced it answers half the question: the pair is what
    // was tested, and a companion left over from an older release is a leading
    // cause of "it detects nothing" reports. "-" means firmware older than v0.88,
    // which reported no build at all.
    char esp_build[12];
    snprintf(esp_build, sizeof(esp_build), "%s", app->esp_build[0] ? app->esp_build : "-");
    char sig_revision[16];
    snprintf(
        sig_revision,
        sizeof(sig_revision),
        "%s",
        app->esp_sig_revision[0] ? app->esp_sig_revision : "-");
    int sig_match = app->esp_sig_revision[0] ? (app->esp_sig_mismatch ? 0 : 1) : -1;
    int sigtest = app->esp_sigtest_seen ? (app->esp_sigtest_pass ? 1 : 0) : -1;
    uint32_t table = (uint32_t)app->flock_count;
    app->diag_start_epoch = 0; // one row per session, not one per teardown call
    furi_mutex_release(app->mutex);

    uint32_t end = furi_hal_rtc_get_timestamp();

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, RECON_APP_FOLDER);
    File* file = storage_file_alloc(storage);
    // ROTATE A STALE SCHEMA ASIDE FIRST.
    //
    // The header is only written when the file is empty, so a diag.csv created
    // under an older schema kept that header forever while the ROWS below it
    // changed shape. This project's own card ended up with a v1 header over a
    // mix of 19- and 20-column rows, and anyone parsing it by the header -- which
    // is the only thing a reader has -- mis-assigns every column after the
    // version. That is not hypothetical: it happened while reading a field
    // report, and the wrong reading survived several rounds of analysis.
    //
    // If the existing header is not the current one, move the whole file to
    // diag.old.csv and start fresh. Nothing is destroyed, one generation back is
    // kept, and every file that exists afterwards is internally consistent.
    if(!recon_diag_header_current(storage)) {
        storage_simply_remove(storage, RECON_DIAG_OLD_PATH);
        storage_common_rename(storage, RECON_DIAG_PATH, RECON_DIAG_OLD_PATH);
    }
    if(storage_file_open(file, RECON_DIAG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        FuriString* s = furi_string_alloc();
        if(storage_file_size(file) == 0) {
            furi_string_cat_str(
                s,
                RECON_DIAG_HEADER_LINE
                "start,end,dur_s,ver,esp_ver,backend,band_req,band_act,band_ch,proto,"
                "sig_expected,sig_actual,sig_match,sigtest,"
                "esp_lines,esp_dropped,esp_reboots,esp_frames,esp_hits,"
                "reports,accepted,rej_conf,rej_full,rej_ignored,table\n");
        }
        furi_string_cat_printf(
            s,
            "%lu,%lu,%lu,%s,%s,%u,%u,%u,%u,%u,%s,%s,%d,%d,"
            "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
            (unsigned long)start,
            (unsigned long)end,
            (unsigned long)(end - start),
            RECON_VERSION,
            esp_build,
            (unsigned)backend,
            (unsigned)band_req,
            (unsigned)band_act,
            (unsigned)band_ch,
            (unsigned)proto,
            FDF_SIGNATURE_REVISION,
            sig_revision,
            sig_match,
            sigtest,
            (unsigned long)lines,
            (unsigned long)dropped,
            (unsigned long)reboots,
            (unsigned long)frames,
            (unsigned long)ehits,
            (unsigned long)msgs,
            (unsigned long)acc,
            (unsigned long)rej_c,
            (unsigned long)rej_f,
            (unsigned long)rej_i,
            (unsigned long)table);
        storage_file_write(file, furi_string_get_cstr(s), furi_string_size(s));
        furi_string_free(s);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
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
    // hits.csv carries no separate channel-RSSI column, so seed it from the
    // stored reading: that row's channel and RSSI came from the same sighting.
    // Seeding INT8_MIN instead would let the first fringe capture of the new
    // session overwrite a channel earned at close range on the last drive.
    e->chan_rssi = r->rssi;
    e->ftype = r->ftype;
    e->confidence = (FlockConfidence)r->conf;
    e->dev_class = r->dev_class;
    e->hidden = r->hidden;
    // Remote ID. NAN for anything that is not an aircraft, or that was saved
    // before v4 -- never 0, which is a real place and would draw a pilot marker
    // in the Gulf of Guinea.
    e->op_lat = r->op_lat;
    e->op_lon = r->op_lon;
    e->ua_type = r->ua_type;
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
    e->confirmed = r->confirmed;
    snprintf(e->label, sizeof(e->label), "%s", r->label);
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

    // FINISH AN INTERRUPTED SWAP. recon_hits_save() removes hits.csv only after
    // hits.tmp is complete and closed, then renames it into place. Power lost
    // between those two calls leaves no hits.csv and a whole, valid hits.tmp --
    // so that combination can only mean "the rename did not happen", and
    // completing it here recovers every row. A hits.tmp NEXT TO a hits.csv is
    // the opposite case, a write that died part-way, and is left to be
    // overwritten by the next save.
    if(!storage_file_exists(app->storage, RECON_HITS_PATH) &&
       storage_file_exists(app->storage, RECON_HITS_TMP_PATH)) {
        storage_common_rename(app->storage, RECON_HITS_TMP_PATH, RECON_HITS_PATH);
    }

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
    storage_common_remove(app->storage, RECON_HITS_TMP_PATH);
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

bool recon_hits_clear_all(ReconApp* app) {
    // The file first, and only then the table. If the card refuses the delete,
    // emptying the screen anyway would show "cleared" over hits that come back
    // on the next launch.
    storage_common_remove(app->storage, RECON_HITS_TMP_PATH);
    FS_Error err = storage_common_remove(app->storage, RECON_HITS_PATH);
    if(err != FSE_OK && err != FSE_NOT_EXIST) return false;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->flock_count = 0;
    app->selected = 0;
    app->hits_dirty = false; // nothing left to flush
    furi_mutex_release(app->mutex);
    return true;
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
    // Notice when the companion or the GPS has gone silent. Both used to be
    // "up until a scene said otherwise", which is how a loose header connector
    // produced a drive's worth of confident-looking nothing.
    recon_app_liveness_tick(app);
    // Phone GPS: re-ask for the location stream while it is not delivering. Same
    // hoisted-to-the-dispatcher reasoning as the two above -- the ordinary case is
    // that the operator opens a scan screen and connects the phone afterwards, and
    // that must not need a per-scene call somebody forgets to add. A no-op when
    // the phone source is not selected, and on firmware without the service.
    gps_rpc_tick(app->gps_rpc);
    // Get detections onto the card while the scan is still running. Hoisted here
    // for the same reason as the three above: every scene gets it and no new
    // scene has to remember. Cheap -- it is a flag test on all but one tick in
    // 120, and a no-op entirely when Save hits is off.
    recon_hits_autosave_tick(app);
    // The survey needs the same protection, and for longer than hits did: a
    // stop's survey is the file that EXPLAINS the stop, and it took a drive to
    // collect. Hoisted here for the same reason as everything above -- every
    // scene gets it and no new scene has to remember.
    recon_survey_autosave_tick(app);
    // Pull the probe survey off the companion periodically. It is held in RAM on
    // the board and only moves when asked, so this is the one thing that puts it
    // on the card -- and it must happen DURING the session, because the link is
    // torn down before the save runs. 30 s: a bounded handful of lines, far below
    // anything that competes with detection traffic.
    recon_survey_tick(app);
    scene_manager_handle_tick_event(app->scene_manager);
}

// ---- lifecycle -----------------------------------------------------------

static ReconApp* recon_app_alloc(void) {
    ReconApp* app = malloc(sizeof(ReconApp));
    memset(app, 0, sizeof(ReconApp));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    // AFTER the mutex: recon_tables_acquire takes it, and furi_mutex_acquire on
    // a NULL handle faults.
    recon_tables_acquire(app);
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

    // Exclusions, read once into the fixed tables the detection gate reads. Same
    // fail-safe posture as everything else here: an absent or corrupt file
    // leaves both counts at 0, so a bad exclusions file can never suppress a
    // detection -- only the operator's own entries can.
    sig_db_ignored_read(
        app->storage,
        app->ignore_fps,
        SIG_IGNORED_MAX_FPS,
        app->ignore_macs,
        SIG_IGNORED_MAX_MACS,
        &app->ignore_fp_count,
        &app->ignore_mac_count);
    // The saved hits were restored a few lines up, before these lists existed,
    // so reconcile now: a device excluded in an earlier session must not come
    // back from hits.csv on every launch.
    recon_app_purge_excluded(app);

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
    app->text_input = text_input_alloc();
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
        app->view_dispatcher, ReconViewTextInput, text_input_get_view(app->text_input));
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

// Byte size of each table, rounded up so the next one starts 8-byte aligned.
#define TBL_ALIGN(n)  (((n) + 7u) & ~7u)
#define TBL_FLOCK_SZ  TBL_ALIGN(RECON_FLOCK_MAX * sizeof(FlockEntry))
#define TBL_WIFI_SZ   TBL_ALIGN(RECON_WIFI_MAX * sizeof(WifiAp))
#define TBL_BLE_SZ    TBL_ALIGN(RECON_BLE_MAX * sizeof(BleDevice))
#define TBL_SURVEY_SZ TBL_ALIGN(RECON_SURVEY_MAX * sizeof(SurveyEntry))
#define TBL_TOTAL_SZ  (TBL_FLOCK_SZ + TBL_WIFI_SZ + TBL_BLE_SZ + TBL_SURVEY_SZ)

/** Free whichever shape the tables were allocated in. Caller holds the mutex,
 *  or is the teardown path where nothing else is running. */
static void recon_tables_free_storage(ReconApp* app) {
    if(app->tables_split) {
        free(app->flock);
        free(app->wifi);
        free(app->ble);
        free(app->survey);
        app->tables_split = false;
    } else {
        free(app->tables_block);
    }
    app->tables_block = NULL;
}

void recon_tables_release(ReconApp* app) {
    // Persist before dropping, or a screen that merely wants memory becomes data
    // loss. A no-op when Save Hits is off, same contract as everywhere else.
    recon_hits_save(app);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    recon_tables_free_storage(app);
    app->flock = NULL;
    app->wifi = NULL;
    app->ble = NULL;
    app->survey = NULL;
    // Counts must go with the storage. A stale non-zero count over a NULL table
    // is the shape of every use-after-free this could produce.
    app->flock_count = 0;
    app->wifi_count = 0;
    app->ble_count = 0;
    app->survey_count = 0;
    furi_mutex_release(app->mutex);
}

void recon_tables_acquire(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(!app->tables_block && !app->flock) {
        // malloc on this firmware does not return NULL: it halts the device on
        // out-of-memory. So ASK FIRST. If one contiguous block is not available
        // -- which is what a fragmented heap looks like after the flasher
        // plugin, its thread stack and the file browser have come and gone --
        // take four smaller ones instead. That costs the fragmentation the
        // single block was introduced to avoid, and it is still strictly better
        // than rebooting the Flipper on the way out of a successful flash.
        if(memmgr_heap_get_max_free_block() < TBL_TOTAL_SZ + 512u) {
            app->flock = calloc(1, TBL_FLOCK_SZ);
            app->wifi = calloc(1, TBL_WIFI_SZ);
            app->ble = calloc(1, TBL_BLE_SZ);
            app->survey = calloc(1, TBL_SURVEY_SZ);
            app->tables_split = true;
        }
    }
    if(!app->tables_block && !app->flock) {
        // ONE BLOCK, carved. Four separate allocations left the heap a little
        // worse after every release/acquire round trip, because the plugin that
        // borrows the space in between is one big block and the four that come
        // back afterwards do not refill the same hole. Measured: largest
        // contiguous block 32,448 -> 25,776 in one firmware-screen visit,
        // cumulative, until the file browser could no longer allocate at all.
        uint8_t* p = calloc(1, TBL_TOTAL_SZ);
        if(p) {
            app->tables_block = p;
            app->flock = (FlockEntry*)p;
            app->wifi = (WifiAp*)(p + TBL_FLOCK_SZ);
            app->ble = (BleDevice*)(p + TBL_FLOCK_SZ + TBL_WIFI_SZ);
            app->survey = (SurveyEntry*)(p + TBL_FLOCK_SZ + TBL_WIFI_SZ + TBL_BLE_SZ);
        }
    }
    app->flock_count = 0;
    app->wifi_count = 0;
    app->ble_count = 0;
    app->survey_count = 0;
    furi_mutex_release(app->mutex);
}

static void recon_app_free(ReconApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewFlock);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewFlockDetail);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewFlockMap);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewDeflockQr);
    view_dispatcher_remove_view(app->view_dispatcher, ReconViewLocator);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    popup_free(app->popup);
    text_input_free(app->text_input);
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
    // One block backing all four tables (see recon_tables_acquire). Freed before
    // the mutex, since release takes it.
    recon_tables_free_storage(app);

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

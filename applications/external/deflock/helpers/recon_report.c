// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "recon_report.h"
#include "../recon_app_i.h"
#include "report_escape.h" // csv/json/md/xml field escapers (pure, host-tested)
#include "report_fmt.h" // fmt_mac / fmt_coord field emitters (pure, host-tested)

#include <math.h>
#include <string.h>
#include <stdarg.h>

// Reports are *streamed* a row at a time straight to the SD card rather than
// assembled in RAM. The old approach built the entire report in several growing
// FuriStrings at once (CSV + GeoJSON + WiGLE/KML), which on a large scan used
// tens of KB of heap on top of the FAP's already tight share of the ~256 KB the
// Flipper shares between firmware and app -- enough to exhaust it and crash
// ("out of memory"). Streaming keeps peak usage to one ~1 KB line buffer per
// file regardless of how many detections there are.
#define REPORT_LINE_MAX 1024

typedef struct {
    File* file;
    bool ok;
} RFile;

static void rfile_open(RFile* r, Storage* storage, const char* path) {
    r->file = storage_file_alloc(storage);
    r->ok = storage_file_open(r->file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
}

static void rfile_raw(RFile* r, const char* data, size_t len) {
    if(r->ok && storage_file_write(r->file, data, len) != len) r->ok = false;
}

static void rfile_puts(RFile* r, const char* s) {
    rfile_raw(r, s, strlen(s));
}

// Format one line into the caller's shared scratch buffer (REPORT_LINE_MAX) and
// stream it to the file. Long lines are truncated rather than overflowed.
static void rfile_printf(RFile* r, char* scratch, const char* fmt, ...) {
    if(!r->ok) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(scratch, REPORT_LINE_MAX, fmt, ap);
    va_end(ap);
    if(n < 0) {
        r->ok = false;
        return;
    }
    size_t w = ((size_t)n < REPORT_LINE_MAX) ? (size_t)n : (size_t)(REPORT_LINE_MAX - 1);
    rfile_raw(r, scratch, w);
}

// Close + free the file handle; returns whether every write to it succeeded.
static bool rfile_close(RFile* r) {
    bool ok = r->ok;
    if(r->file) {
        storage_file_close(r->file);
        storage_file_free(r->file);
        r->file = NULL;
    }
    return ok;
}

static void recon_report_timestamp(char* buf, size_t len) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(
        buf,
        len,
        "%04u%02u%02u_%02u%02u%02u",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);
}

void recon_report_ensure_dirs(void* _app) {
    ReconApp* app = _app;
    storage_common_mkdir(app->storage, EXT_PATH("apps_data"));
    storage_common_mkdir(app->storage, RECON_APP_FOLDER);
    storage_common_mkdir(app->storage, RECON_REPORT_FOLDER);
}

bool recon_report_save_flock(void* _app, char* out_path_md, size_t out_len) {
    ReconApp* app = _app;

    // Pre-count marked entries: if nothing is marked there's nothing to save,
    // and we avoid creating empty report files.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    int marked_total = 0;
    for(size_t i = 0; i < app->flock_count; i++) {
        if(app->flock[i].marked) marked_total++;
    }
    furi_mutex_release(app->mutex);
    if(marked_total == 0) return false;

    recon_report_ensure_dirs(app);

    char ts[24];
    recon_report_timestamp(ts, sizeof(ts));

    char path_md[128];
    char path_geo[128];
    char path_kml[128];
    snprintf(path_md, sizeof(path_md), "%s/flock_%s.md", RECON_REPORT_FOLDER, ts);
    snprintf(path_geo, sizeof(path_geo), "%s/flock_%s.geojson", RECON_REPORT_FOLDER, ts);
    snprintf(path_kml, sizeof(path_kml), "%s/flock_%s.kml", RECON_REPORT_FOLDER, ts);

    char* line = malloc(REPORT_LINE_MAX);
    if(!line) return false; // heap critically low; fail cleanly rather than crash
    RFile md, geo, kml;
    rfile_open(&md, app->storage, path_md);
    rfile_open(&geo, app->storage, path_geo);
    rfile_open(&kml, app->storage, path_kml);

    rfile_printf(
        &md,
        line,
        "# FlipDeFlock - Flock/ALPR Report\n\n"
        "Generated: %s (device RTC)\n\n"
        "Detection by OUI + probe behaviour + SSID naming. 'Possible' = OUI only\n"
        "(generic vendor prefix); treat as a lead, verify visually.\n\n"
        // Vendor sits next to Class because the two answer different questions
        // ("who made it" vs "what is it") and the pair is only honest together:
        // Class alone says ALPR for hardware from five different companies.
        "| # | Conf | Vendor | Class | MAC | SSID | RSSI | Ch | Seen | Lat | Lon |\n"
        "|---|------|--------|-------|-----|------|------|----|------|-----|-----|\n",
        ts);

    rfile_puts(
        &kml,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<kml xmlns=\"http://www.opengis.net/kml/2.2\"><Document>\n"
        "<name>FlipDeFlock Flock/ALPR</name>\n");

    rfile_puts(&geo, "{\n  \"type\": \"FeatureCollection\",\n  \"features\": [\n");

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    int marked = 0;
    bool first_feature = true;
    for(size_t i = 0; i < app->flock_count; i++) {
        FlockEntry* e = &app->flock[i];
        if(!e->marked) continue;
        marked++;

        char lat_s[16];
        char lon_s[16];
        bool has_coords = !isnan(e->lat) && !isnan(e->lon);
        // Both-or-nothing: a partial fix shows "-" for both cells.
        fmt_coord(lat_s, sizeof(lat_s), has_coords ? e->lat : NAN, "-");
        fmt_coord(lon_s, sizeof(lon_s), has_coords ? e->lon : NAN, "-");

        char mac_s[18];
        fmt_mac(mac_s, sizeof(mac_s), e->mac);

        char ssid_md[80];
        // Distinguish "no name recorded" from "the AP beacons and withholds it" --
        // the second is an observation about the device, the first is just a gap
        // in what we saw.
        md_escape(
            e->ssid[0] ? e->ssid :
            e->hidden  ? "(SSID withheld)" :
                         "(none seen)",
            ssid_md,
            sizeof(ssid_md));

        rfile_printf(
            &md,
            line,
            "| %d | %s | %s | %s | %s | %s | %d | %u | %lu | %s | %s |\n",
            marked,
            flock_confidence_str(e->confidence),
            flock_vendor_str(flock_vendor_of(e->mac, e->ssid)),
            flock_class_str((FlockDevClass)e->dev_class),
            mac_s,
            ssid_md,
            e->rssi,
            e->channel,
            (unsigned long)e->count,
            lat_s,
            lon_s);

        if(has_coords) {
            char head_s[16];
            if(!isnan(e->heading)) {
                snprintf(head_s, sizeof(head_s), "%.1f", (double)e->heading);
            } else {
                snprintf(head_s, sizeof(head_s), "null");
            }
            // An SSID is up to 32 bytes of arbitrary data -- escape per output
            // format so a stray " \ & or < can't produce malformed GeoJSON/KML.
            char ssid_json[128];
            char ssid_xml[128];
            json_escape(e->ssid[0] ? e->ssid : "", ssid_json, sizeof(ssid_json));
            xml_escape(e->ssid[0] ? e->ssid : "", ssid_xml, sizeof(ssid_xml));
            if(!first_feature) rfile_puts(&geo, ",\n");
            first_feature = false;
            rfile_printf(
                &geo,
                line,
                "    {\n"
                "      \"type\": \"Feature\",\n"
                "      \"geometry\": { \"type\": \"Point\", \"coordinates\": [%s, %s] },\n"
                // OSM/DeFlock tagging so the points are importable to OSM (which
                // deflock.org sources). Our extras are namespaced flipdeflock:*.
                "      \"properties\": {\n"
                "        \"man_made\": \"surveillance\",\n"
                "        \"surveillance:type\": \"ALPR\",\n"
                "        \"manufacturer\": \"Flock Safety\",\n"
                "        \"flipdeflock:confidence\": \"%s\",\n"
                "        \"flipdeflock:heading\": %s,\n"
                "        \"flipdeflock:mac\": \"%s\",\n"
                "        \"flipdeflock:ssid\": \"%s\"\n"
                "      }\n"
                "    }",
                lon_s,
                lat_s,
                flock_confidence_str(e->confidence),
                head_s,
                mac_s,
                ssid_json);

            rfile_printf(
                &kml,
                line,
                "<Placemark><name>Flock %s</name>"
                "<description>%s %s</description>"
                "<Point><coordinates>%s,%s,0</coordinates></Point></Placemark>\n",
                flock_confidence_str(e->confidence),
                mac_s,
                ssid_xml,
                lon_s,
                lat_s);
        }
    }

    furi_mutex_release(app->mutex);

    rfile_printf(&md, line, "\nTotal marked: %d\n", marked);
    rfile_puts(&geo, "\n  ]\n}\n");
    rfile_puts(&kml, "</Document></kml>\n");

    bool ok_md = rfile_close(&md);
    bool ok_geo = rfile_close(&geo);
    bool ok_kml = rfile_close(&kml);
    free(line);

    bool ok = ok_md && ok_geo && ok_kml;
    if(!ok) {
        // Don't leave partial/half-written report files behind on a failed save.
        storage_simply_remove(app->storage, path_md);
        storage_simply_remove(app->storage, path_geo);
        storage_simply_remove(app->storage, path_kml);
    } else if(out_path_md) {
        snprintf(out_path_md, out_len, "%s", path_md);
    }
    return ok;
}

bool recon_report_save_fp(void* _app, char* out_path_md, size_t out_len) {
    ReconApp* app = _app;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    size_t total = app->flock_count;
    furi_mutex_release(app->mutex);
    if(total == 0) return false;

    recon_report_ensure_dirs(app);

    char ts[24];
    recon_report_timestamp(ts, sizeof(ts));

    char path_md[128];
    snprintf(path_md, sizeof(path_md), "%s/falsepos_%s.md", RECON_REPORT_FOLDER, ts);

    char* line = malloc(REPORT_LINE_MAX);
    if(!line) return false;
    RFile md;
    rfile_open(&md, app->storage, path_md);

    // The header states what was removed, in the file itself. Someone about to
    // attach this to a public issue should not have to take our word for it from
    // a menu label they saw once, and whoever receives it should be able to tell
    // that a missing column is redaction rather than a bug.
    rfile_printf(
        &md,
        line,
        "# FlipDeFlock - False Positive Report\n\n"
        "App: %s   Generated: %s (device RTC)\n\n"
        "**Redacted for sharing.** No GPS coordinates, no heading, no timestamps,\n"
        "and only the OUI of each MAC. SSIDs are shown as a SHAPE\n"
        "(`A`=upper `a`=lower `d`=digit) unless the name itself matched a Flock\n"
        "naming rule, in which case it is the camera's own name and is shown\n"
        "as-is. Nothing here says where you were.\n\n"
        "`Method` is the indicator that actually fired. A row reading `OUI` was\n"
        "flagged on a shared silicon-vendor prefix alone, which is the most\n"
        "common source of a false positive.\n\n"
        "| # | Conf | Method | Vendor | Class | OUI | SSID | Fr | Ch | RSSI | Seen | Hid | IE fp |\n"
        "|---|------|--------|--------|-------|-----|------|----|----|------|------|-----|-------|\n",
        RECON_VERSION,
        ts);

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    int n = 0;
    for(size_t i = 0; i < app->flock_count; i++) {
        FlockEntry* e = &app->flock[i];
        n++;

        char oui_s[20];
        fmt_mac_oui(oui_s, sizeof(oui_s), e->mac);

        // Keep the literal name ONLY when the name is why this matched. A Flock
        // pattern name belongs to the camera; anything else belongs to whoever
        // owns the network, and this file is meant to be shareable.
        // ssid_out holds ssid_raw plus the two backticks the shape branch adds,
        // so it must be wider than ssid_raw or the wrap silently truncates the
        // closing one. Explicit rather than "80 looks fine": this file is built
        // with -Wformat-truncation on newer SDKs and it is an error there.
        char ssid_raw[80];
        char ssid_out[sizeof(ssid_raw) + 2];
        if(e->ssid[0] && flock_ssid_confidence(e->ssid) != FlockConfidenceNone) {
            md_escape(e->ssid, ssid_raw, sizeof(ssid_raw));
            snprintf(ssid_out, sizeof(ssid_out), "%s", ssid_raw);
        } else {
            char shape[RECON_SSID_LEN + 8];
            fmt_ssid_shape(shape, sizeof(shape), e->ssid);
            // Escaped too: the shape can contain '_', which is Markdown emphasis.
            md_escape(shape, ssid_raw, sizeof(ssid_raw));
            snprintf(ssid_out, sizeof(ssid_out), "`%s`", ssid_raw);
        }

        FlockMethod method = flock_method_of(e->mac, e->ssid, e->ftype, e->ie_fp);

        rfile_printf(
            &md,
            line,
            "| %d | %s | %s | %s | %s | %s | %s | %c | %u | %d | %lu | %s | %08lX |\n",
            n,
            flock_confidence_str(e->confidence),
            flock_method_str(method),
            flock_vendor_str(flock_vendor_of(e->mac, e->ssid)),
            flock_class_str((FlockDevClass)e->dev_class),
            oui_s,
            ssid_out,
            fmt_frame_char(e->ftype),
            e->channel,
            e->rssi,
            (unsigned long)e->count,
            e->hidden ? "y" : "-",
            (unsigned long)e->ie_fp);
    }

    furi_mutex_release(app->mutex);

    rfile_printf(&md, line, "\nTotal detections: %d\n", n);

    bool ok = rfile_close(&md);
    free(line);

    if(!ok) {
        storage_simply_remove(app->storage, path_md);
    } else if(out_path_md) {
        snprintf(out_path_md, out_len, "%s", path_md);
    }
    return ok;
}

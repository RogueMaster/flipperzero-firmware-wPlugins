// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "recon_report.h"
#include "../recon_app_i.h"
#include "wifi_audit.h"
#include "report_escape.h" // csv/json/md/xml field escapers (pure, host-tested)
#include "report_fmt.h" // fmt_mac / fmt_coord field emitters (pure, host-tested)

#include <math.h>
#include <string.h>
#include <stdarg.h>

// Shared WigleWifi-1.4 pre-header + column header (WiFi and BLE writers).
#define WIGLE_HEADER                                                    \
    "WigleWifi-1.4,appRelease=FlipDeFlock,model=FlipperZero,release=0," \
    "device=FlipDeFlock,display=,board=ESP32,brand=Flipper\n"           \
    "MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,"         \
    "CurrentLongitude,AltitudeMeters,AccuracyMeters,Type\n"

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
        "| # | Conf | MAC | SSID | RSSI | Ch | Seen | Lat | Lon |\n"
        "|---|------|-----|------|------|----|------|-----|-----|\n",
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
        md_escape(e->ssid[0] ? e->ssid : "(hidden)", ssid_md, sizeof(ssid_md));

        rfile_printf(
            &md,
            line,
            "| %d | %s | %s | %s | %d | %u | %lu | %s | %s |\n",
            marked,
            flock_confidence_str(e->confidence),
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

static const char* ble_cat_name(uint8_t cat) {
    switch(cat) {
    case 1:
        return "Flock/Raven";
    case 2:
        return "AirTag";
    case 3:
        return "Tile";
    case 4:
        return "SmartTag";
    case 5:
        return "FindMyDevice";
    default:
        return "BLE";
    }
}

// Short, machine-friendly Flock model token for report cells. Raven is GATT-
// backed (0x3100-0x3500) and confident -> no "?". Falcon keeps its "?" because
// it is never asserted; a plain battery with no Raven GATT stays FlockExtBattery.
static const char* ble_model_token(uint8_t model) {
    switch(model) {
    case FlockBleModelFalcon:
        return "FlockFalcon?";
    case FlockBleModelRaven:
        return "FlockRaven";
    case FlockBleModelGeneric:
        return "FlockExtBattery";
    default:
        return "";
    }
}

bool recon_report_save_ble(void* _app, char* out_path_md, size_t out_len) {
    ReconApp* app = _app;

    // Nothing scanned -> nothing to save (and don't create empty files).
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    size_t n = app->ble_count;
    furi_mutex_release(app->mutex);
    if(n == 0) return false;

    recon_report_ensure_dirs(app);
    char ts[24];
    recon_report_timestamp(ts, sizeof(ts));

    DateTime bdt;
    furi_hal_rtc_get_datetime(&bdt);
    char ble_seen[32];
    snprintf(
        ble_seen,
        sizeof(ble_seen),
        "%04u-%02u-%02u %02u:%02u:%02u",
        bdt.year,
        bdt.month,
        bdt.day,
        bdt.hour,
        bdt.minute,
        bdt.second);

    char path_csv[128];
    char path_geo[128];
    char path_wigle[128];
    snprintf(path_csv, sizeof(path_csv), "%s/ble_%s.csv", RECON_REPORT_FOLDER, ts);
    snprintf(path_geo, sizeof(path_geo), "%s/ble_%s.geojson", RECON_REPORT_FOLDER, ts);
    snprintf(path_wigle, sizeof(path_wigle), "%s/ble_%s.wigle.csv", RECON_REPORT_FOLDER, ts);

    char* line = malloc(REPORT_LINE_MAX);
    if(!line) return false; // heap critically low; fail cleanly rather than crash
    RFile csv, geo, wigle;
    rfile_open(&csv, app->storage, path_csv);
    rfile_open(&geo, app->storage, path_geo);
    rfile_open(&wigle, app->storage, path_wigle);

    rfile_puts(
        &csv,
        "addr,name,category,model,serial,company,rssi,count,following,tagged,first_lat,first_lon,last_lat,last_lon\n");
    rfile_puts(&geo, "{\n  \"type\": \"FeatureCollection\",\n  \"features\": [\n");
    rfile_puts(&wigle, WIGLE_HEADER);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    n = app->ble_count;
    bool first_feature = true;
    for(size_t i = 0; i < n; i++) {
        BleDevice* d = &app->ble[i];
        char fl[16], fo[16], ll[16], lo[16];
        fmt_coord(fl, sizeof(fl), d->first_lat, "");
        fmt_coord(fo, sizeof(fo), d->first_lon, "");
        fmt_coord(ll, sizeof(ll), d->last_lat, "");
        fmt_coord(lo, sizeof(lo), d->last_lon, "");

        char addr_s[18];
        fmt_mac(addr_s, sizeof(addr_s), d->addr);

        char name_esc[72];
        csv_field_escape(d->name[0] ? d->name : "", name_esc, sizeof(name_esc));
        // Serial is a persistent ID of a police asset -> omit from saved reports
        // unless the user opts in via the "Log Flock serials" privacy toggle.
        char serial_esc[40];
        csv_field_escape(
            (app->settings.log_serials && d->serial[0]) ? d->serial : "",
            serial_esc,
            sizeof(serial_esc));
        rfile_printf(
            &csv,
            line,
            "%s,%s,%s,%s,%s,0x%04X,%d,%lu,%s,%s,%s,%s,%s,%s\n",
            addr_s,
            name_esc,
            ble_cat_name(d->cat),
            ble_model_token(d->model),
            serial_esc,
            (unsigned)d->company,
            d->rssi,
            (unsigned long)d->count,
            d->following ? "yes" : "no",
            d->marked ? "yes" : "no",
            fl,
            fo,
            ll,
            lo);

        if((d->following || d->cat) && !isnan(d->last_lat)) {
            if(!first_feature) rfile_puts(&geo, ",\n");
            first_feature = false;
            const char* geo_serial = (app->settings.log_serials && d->serial[0]) ? d->serial : "";
            // A BLE tracker name is user-settable -- escape it (and the serial) so
            // a " or \ in the name can't produce invalid GeoJSON.
            char name_json[128];
            char serial_json[48];
            json_escape(d->name[0] ? d->name : "", name_json, sizeof(name_json));
            json_escape(geo_serial, serial_json, sizeof(serial_json));
            rfile_printf(
                &geo,
                line,
                "    {\n"
                "      \"type\": \"Feature\",\n"
                "      \"geometry\": { \"type\": \"Point\", \"coordinates\": [%s, %s] },\n"
                "      \"properties\": { \"type\": \"%s\", \"model\": \"%s\", \"serial\": \"%s\", "
                "\"following\": %s, "
                "\"addr\": \"%s\", \"name\": \"%s\" }\n"
                "    }",
                lo,
                ll,
                ble_cat_name(d->cat),
                ble_model_token(d->model),
                serial_json,
                d->following ? "true" : "false",
                addr_s,
                name_json);
        }

        // WiGLE row only for geotagged devices (omit no-fix -> no Null Island).
        if(!isnan(d->last_lat) && !isnan(d->last_lon)) {
            char wname[72];
            csv_field_escape(d->name[0] ? d->name : "", wname, sizeof(wname));
            rfile_printf(
                &wigle,
                line,
                "%s,%s,%s,%s,0,%d,%.6f,%.6f,0,0,BLE\n",
                addr_s,
                wname,
                d->cat != BleCatUnknown ? "[BLE][Tracker]" : "[BLE]",
                ble_seen,
                d->rssi,
                (double)d->last_lat,
                (double)d->last_lon);
        }
    }
    furi_mutex_release(app->mutex);

    rfile_puts(&geo, "\n  ]\n}\n");

    bool ok_csv = rfile_close(&csv);
    bool ok_geo = rfile_close(&geo);
    bool ok_wigle = rfile_close(&wigle);
    free(line);

    bool ok = ok_csv && ok_geo && ok_wigle;
    if(!ok) {
        storage_simply_remove(app->storage, path_csv);
        storage_simply_remove(app->storage, path_geo);
        storage_simply_remove(app->storage, path_wigle);
    } else if(out_path_md) {
        snprintf(out_path_md, out_len, "%s", path_csv);
    }
    return ok;
}

bool recon_report_append_nfc(void* _app, const char* line) {
    ReconApp* app = _app;
    recon_report_ensure_dirs(app);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    char path[128];
    snprintf(
        path,
        sizeof(path),
        "%s/nfc_audit_%04u%02u%02u.csv",
        RECON_REPORT_FOLDER,
        dt.year,
        dt.month,
        dt.day);

    File* file = storage_file_alloc(app->storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        FuriString* row = furi_string_alloc();
        // Write a column header the first time the daily file is created. If the
        // header write fails, don't write (or claim success for) a headerless row.
        bool hdr_ok = true;
        if(storage_file_size(file) == 0) {
            const char* header = "time,protocol,grade,uid,sectors_default,lat,lon\n";
            size_t hlen = strlen(header);
            hdr_ok = storage_file_write(file, header, hlen) == hlen;
        }
        if(hdr_ok) {
            furi_string_printf(row, "%02u:%02u:%02u,%s\n", dt.hour, dt.minute, dt.second, line);
            size_t len = furi_string_size(row);
            ok = storage_file_write(file, furi_string_get_cstr(row), len) == len;
        }
        furi_string_free(row);
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

/** Build a WiGLE-style capability string from the ESP auth mode + WPS flag. */
static void wigle_auth(uint8_t authmode, bool wps, char* buf, size_t len) {
    const char* base;
    switch(authmode) {
    case 0:
        base = "[ESS]";
        break;
    case 1:
        base = "[WEP][ESS]";
        break;
    case 2:
        base = "[WPA-PSK-TKIP][ESS]";
        break;
    case 3:
        base = "[WPA2-PSK-CCMP][ESS]";
        break;
    case 4:
        base = "[WPA-PSK-TKIP][WPA2-PSK-CCMP][ESS]";
        break;
    case 5:
        base = "[WPA2-EAP-CCMP][ESS]";
        break;
    case 6:
        base = "[WPA3-SAE-CCMP][ESS]";
        break;
    case 7:
        base = "[WPA2-PSK-CCMP][WPA3-SAE-CCMP][ESS]";
        break;
    case 9:
        base = "[OWE][ESS]";
        break;
    default:
        base = "[ESS]";
        break;
    }
    snprintf(buf, len, "%s%s", wps ? "[WPS]" : "", base);
}

bool recon_report_save_wifi(void* _app, char* out_path_md, size_t out_len) {
    ReconApp* app = _app;

    // Nothing scanned -> nothing to save (and don't create empty files).
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    size_t n = app->wifi_count;
    furi_mutex_release(app->mutex);
    if(n == 0) return false;

    recon_report_ensure_dirs(app);

    char ts[24];
    recon_report_timestamp(ts, sizeof(ts));

    char path_md[128];
    char path_csv[128];
    char path_wigle[128];
    snprintf(path_md, sizeof(path_md), "%s/wifi_%s.md", RECON_REPORT_FOLDER, ts);
    snprintf(path_csv, sizeof(path_csv), "%s/wifi_%s.csv", RECON_REPORT_FOLDER, ts);
    snprintf(path_wigle, sizeof(path_wigle), "%s/wifi_%s.wigle.csv", RECON_REPORT_FOLDER, ts);

    DateTime wdt;
    furi_hal_rtc_get_datetime(&wdt);
    char first_seen[32];
    snprintf(
        first_seen,
        sizeof(first_seen),
        "%04u-%02u-%02u %02u:%02u:%02u",
        wdt.year,
        wdt.month,
        wdt.day,
        wdt.hour,
        wdt.minute,
        wdt.second);

    char* line = malloc(REPORT_LINE_MAX);
    if(!line) return false; // heap critically low; fail cleanly rather than crash
    // `reasons` is built per-AP and reset each iteration -- small and reused, so
    // it doesn't accumulate like the old whole-report strings did.
    FuriString* reasons = furi_string_alloc();
    RFile md, csv, wigle;
    rfile_open(&md, app->storage, path_md);
    rfile_open(&csv, app->storage, path_csv);
    rfile_open(&wigle, app->storage, path_wigle);

    rfile_printf(
        &md,
        line,
        "# FlipDeFlock - WiFi Security Audit\n\n"
        "Generated: %s (device RTC)\n\n"
        "| # | Grade | SSID | BSSID | Auth | Ch | RSSI | WPS | Issues |\n"
        "|---|-------|------|-------|------|----|------|-----|--------|\n",
        ts);
    rfile_puts(&csv, "grade,ssid,bssid,auth,channel,rssi,wps,issues\n");
    rfile_puts(&wigle, WIGLE_HEADER);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool gfix = app->gps_valid;
    float glat = app->gps_lat;
    float glon = app->gps_lon;
    n = app->wifi_count;
    for(size_t i = 0; i < n; i++) {
        WifiAp* a = &app->wifi[i];
        furi_string_reset(reasons);
        if(a->marked) furi_string_cat(reasons, "[TAGGED]\n");
        WifiGrade g = wifi_audit_grade(a->authmode, a->pairwise, a->wps, a->ssid, reasons);
        if(a->rogue)
            furi_string_cat(reasons, "EVIL-TWIN: dup SSID mismatched security\n");
        else if(a->dup)
            furi_string_cat(reasons, "dup SSID (mesh?)\n");
        // Flatten reasons (newline -> "; ") for a single table/CSV cell.
        furi_string_replace_all(reasons, "\n", "; ");

        char ssid_md[80];
        md_escape(a->ssid[0] ? a->ssid : "(hidden)", ssid_md, sizeof(ssid_md));

        char bssid_s[18];
        fmt_mac(bssid_s, sizeof(bssid_s), a->bssid);

        rfile_printf(
            &md,
            line,
            "| %u | %s | %s | %s | %s | %u | %d | %s | %s |\n",
            (unsigned)(i + 1),
            wifi_grade_str(g),
            ssid_md,
            bssid_s,
            wifi_auth_str(a->authmode),
            a->channel,
            a->rssi,
            a->wps ? "yes" : "no",
            furi_string_get_cstr(reasons));

        char ssid_esc[72];
        csv_field_escape(a->ssid[0] ? a->ssid : "(hidden)", ssid_esc, sizeof(ssid_esc));
        // The flattened reasons string contains literal commas (e.g. "WPA1:
        // deprecated, weak") -- escape it too or it splits the `issues` column.
        char reasons_esc[320];
        csv_field_escape(furi_string_get_cstr(reasons), reasons_esc, sizeof(reasons_esc));
        rfile_printf(
            &csv,
            line,
            "%s,%s,%s,%s,%u,%d,%s,%s\n",
            wifi_grade_str(g),
            ssid_esc,
            bssid_s,
            wifi_auth_str(a->authmode),
            a->channel,
            a->rssi,
            a->wps ? "yes" : "no",
            reasons_esc);

        // WiGLE: omit rows with no GPS fix (0,0 would plant the AP at Null
        // Island); escape the SSID so a comma/quote can't break the columns.
        if(gfix) {
            char authw[48];
            wigle_auth(a->authmode, a->wps, authw, sizeof(authw));
            char wssid[72];
            csv_field_escape(a->ssid, wssid, sizeof(wssid));
            rfile_printf(
                &wigle,
                line,
                "%s,%s,%s,%s,%u,%d,%.6f,%.6f,0,0,WIFI\n",
                bssid_s,
                wssid,
                authw,
                first_seen,
                a->channel,
                a->rssi,
                (double)glat,
                (double)glon);
        }
    }
    furi_mutex_release(app->mutex);

    rfile_printf(&md, line, "\nTotal APs: %u\n", (unsigned)n);

    bool ok_md = rfile_close(&md);
    bool ok_csv = rfile_close(&csv);
    bool ok_wigle = rfile_close(&wigle);
    furi_string_free(reasons);
    free(line);

    bool ok = ok_md && ok_csv && ok_wigle;
    if(!ok) {
        storage_simply_remove(app->storage, path_md);
        storage_simply_remove(app->storage, path_csv);
        storage_simply_remove(app->storage, path_wigle);
    } else if(out_path_md) {
        snprintf(out_path_md, out_len, "%s", path_md);
    }
    return ok;
}

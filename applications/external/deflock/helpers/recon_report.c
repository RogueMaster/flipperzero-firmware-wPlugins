// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "recon_report.h"
#include "../recon_app_i.h"
#include "report_escape.h" // csv/json/md/xml field escapers (pure, host-tested)
#include "report_fmt.h" // fmt_mac / fmt_coord field emitters (pure, host-tested)
#include "survey_rank.h" // survey_mac_is_local
#include "preflight.h" // recon_companion_build_matches
#include "open_drone_id.h" // odid_ua_type_str, for the aircraft section

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

bool recon_report_save_flock(void* _app, char* out_path_md, size_t out_len, uint8_t flags) {
    ReconApp* app = _app;
    const bool redact = (flags & ReconExportRedact) != 0;
    const bool all = (flags & ReconExportAll) != 0;

    // Pre-count what will actually be written: if there is nothing, there is
    // nothing to save, and we avoid creating empty report files.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    int selected_total = 0;
    for(size_t i = 0; i < app->flock_count; i++) {
        if(all || app->flock[i].marked) selected_total++;
    }
    furi_mutex_release(app->mutex);
    if(selected_total == 0) return false;

    recon_report_ensure_dirs(app);

    char ts[24];
    recon_report_timestamp(ts, sizeof(ts));

    // The FILENAME carries the warning, not just the menu label that produced it.
    // A redacted and an unredacted export of the same drive are otherwise
    // indistinguishable once they are off the device and sitting in a folder, and
    // the one that must never be attached to a public issue is the one that looks
    // identical to the one that is safe. "_RAW" travels with the file.
    const char* suffix = redact ? "" : "_RAW";

    char path_md[128];
    char path_geo[128];
    char path_kml[128];
    snprintf(path_md, sizeof(path_md), "%s/flock_%s%s.md", RECON_REPORT_FOLDER, ts, suffix);
    snprintf(path_geo, sizeof(path_geo), "%s/flock_%s%s.geojson", RECON_REPORT_FOLDER, ts, suffix);
    snprintf(path_kml, sizeof(path_kml), "%s/flock_%s%s.kml", RECON_REPORT_FOLDER, ts, suffix);

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
        "App: %s   Generated: %s (device RTC)   Scope: %s\n\n"
        "Detection by OUI + probe behaviour + SSID naming. 'Possible' = OUI only\n"
        "(generic vendor prefix); treat as a lead, verify visually.\n\n",
        RECON_VERSION,
        ts,
        all ? "every stored detection" : "marked detections only");

    // State the privacy posture IN THE FILE. Whoever opens this months later, or
    // receives it from someone else, must be able to tell redacted from raw
    // without knowing which menu item produced it.
    if(redact) {
        rfile_puts(
            &md,
            "**Redacted for sharing.** Each MAC is reduced to its OUI, the sighting\n"
            "time and your heading are omitted, your own labels are left out, and any\n"
            "SSID that did not itself match a Flock naming rule is shown as a SHAPE\n"
            "(`A`=upper `a`=lower `d`=digit) rather than a name.\n\n"
            "CAMERA COORDINATES ARE KEPT -- they are the point of the report. What is\n"
            "removed is the detail that describes YOU rather than the camera: when you\n"
            "passed it, which way you were pointing, and the names of the household\n"
            "networks that merely happened to be in range.\n\n");
    } else {
        rfile_puts(
            &md,
            "**UNREDACTED - KEEP THIS ONE.** Full MACs, verbatim SSIDs (including\n"
            "networks that merely happened to be in range, which are often household\n"
            "names and are independently geolocatable through public wardriving\n"
            "databases), your own labels, sighting times and heading. This is the\n"
            "working copy, for you. Do not attach it to a public issue, a map\n"
            "submission, or a chat -- export the redacted version for that.\n\n");
    }

    // Vendor sits next to Class because the two answer different questions
    // ("who made it" vs "what is it") and the pair is only honest together:
    // Class alone says ALPR for hardware from five different companies.
    if(redact) {
        rfile_puts(
            &md,
            "| # | Conf | Vendor | Class | OUI | SSID | RSSI | Ch | Seen | Lat | Lon |\n"
            "|---|------|--------|-------|-----|------|------|----|------|-----|-----|\n");
    } else {
        rfile_puts(
            &md,
            "| # | Conf | Vendor | Class | MAC | SSID | Label | RSSI | Ch | Seen | Epoch | Head | Lat | Lon |\n"
            "|---|------|--------|-------|-----|------|-------|------|----|------|-------|------|-----|-----|\n");
    }

    rfile_puts(
        &kml,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<kml xmlns=\"http://www.opengis.net/kml/2.2\"><Document>\n"
        "<name>FlipDeFlock Flock/ALPR</name>\n");

    rfile_puts(&geo, "{\n  \"type\": \"FeatureCollection\",\n  \"features\": [\n");

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    int n = 0;
    bool first_feature = true;
    for(size_t i = 0; i < app->flock_count; i++) {
        FlockEntry* e = &app->flock[i];
        if(!all && !e->marked) continue;
        n++;

        char lat_s[16];
        char lon_s[16];
        bool has_coords = !isnan(e->lat) && !isnan(e->lon);
        // Both-or-nothing: a partial fix shows "-" for both cells.
        fmt_coord(lat_s, sizeof(lat_s), has_coords ? e->lat : NAN, "-");
        fmt_coord(lon_s, sizeof(lon_s), has_coords ? e->lon : NAN, "-");

        char mac_s[20];
        if(redact) {
            fmt_mac_oui(mac_s, sizeof(mac_s), e->mac);
        } else {
            fmt_mac(mac_s, sizeof(mac_s), e->mac);
        }

        // SSID. The redacted export keeps the literal name ONLY when the name is
        // why this matched -- a Flock pattern name belongs to the camera, anything
        // else belongs to whoever owns the network. Same rule as the false-positive
        // export, and for the same reason: this file is meant to be shareable.
        char ssid_raw[80];
        char ssid_md[sizeof(ssid_raw) + 2];
        bool name_is_evidence = e->ssid[0] &&
                                flock_ssid_confidence(e->ssid) != FlockConfidenceNone;
        if(!redact || name_is_evidence) {
            // Distinguish "no name recorded" from "the AP beacons and withholds
            // it" -- the second is an observation about the device, the first is
            // just a gap in what we saw.
            md_escape(
                e->ssid[0] ? e->ssid :
                e->hidden  ? "(SSID withheld)" :
                             "(none seen)",
                ssid_raw,
                sizeof(ssid_raw));
            snprintf(ssid_md, sizeof(ssid_md), "%s", ssid_raw);
        } else {
            char shape[RECON_SSID_LEN + 8];
            fmt_ssid_shape(shape, sizeof(shape), e->ssid);
            // Escaped too: the shape can contain '_', which is Markdown emphasis.
            md_escape(shape, ssid_raw, sizeof(ssid_raw));
            snprintf(ssid_md, sizeof(ssid_md), "`%s`", ssid_raw);
        }

        if(redact) {
            rfile_printf(
                &md,
                line,
                "| %d | %s | %s | %s | %s | %s | %d | %u | %lu | %s | %s |\n",
                n,
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
        } else {
            char label_md[64];
            md_escape(e->label[0] ? e->label : "-", label_md, sizeof(label_md));
            char head_md[16];
            if(isnan(e->heading)) {
                snprintf(head_md, sizeof(head_md), "-");
            } else {
                snprintf(head_md, sizeof(head_md), "%.1f", (double)e->heading);
            }
            rfile_printf(
                &md,
                line,
                "| %d | %s | %s | %s | %s | %s | %s | %d | %u | %lu | %lu | %s | %s | %s |\n",
                n,
                flock_confidence_str(e->confidence),
                flock_vendor_str(flock_vendor_of(e->mac, e->ssid)),
                flock_class_str((FlockDevClass)e->dev_class),
                mac_s,
                ssid_md,
                label_md,
                e->rssi,
                e->channel,
                (unsigned long)e->count,
                (unsigned long)e->seen_epoch,
                head_md,
                lat_s,
                lon_s);
        }

        if(has_coords) {
            // Heading is the OBSERVER's course, not the camera's -- it describes
            // which way you were travelling, so the redacted export drops it.
            char head_s[16];
            if(!redact && !isnan(e->heading)) {
                snprintf(head_s, sizeof(head_s), "%.1f", (double)e->heading);
            } else {
                snprintf(head_s, sizeof(head_s), "null");
            }
            // An SSID is up to 32 bytes of arbitrary data -- escape per output
            // format so a stray " \ & or < can't produce malformed GeoJSON/KML.
            //
            // Feed the SAME redaction decision the Markdown table made. The three
            // files are written from one loop and are attached together; a name
            // withheld in the table and re-published in the GeoJSON would be worse
            // than not redacting at all, because the operator would believe it was
            // handled.
            const char* ssid_src = (!redact || name_is_evidence) ? e->ssid : "";
            char ssid_json[128];
            char ssid_xml[128];
            json_escape(ssid_src, ssid_json, sizeof(ssid_json));
            xml_escape(ssid_src, ssid_xml, sizeof(ssid_xml));
            if(!first_feature) rfile_puts(&geo, ",\n");
            first_feature = false;

            FlockVendor gv = flock_vendor_of(e->mac, e->ssid);
            FlockDevClass gc = (FlockDevClass)e->dev_class;

            rfile_printf(
                &geo,
                line,
                "    {\n"
                "      \"type\": \"Feature\",\n"
                "      \"geometry\": { \"type\": \"Point\", \"coordinates\": [%s, %s] },\n"
                "      \"properties\": {\n",
                lon_s,
                lat_s);

            // OSM/DeFlock tagging, so the points are importable to OSM (which
            // deflock.org sources) -- but ONLY for things that actually are fixed
            // surveillance installations.
            //
            // THIS USED TO BE THREE HARDCODED LINES claiming man_made=surveillance,
            // surveillance:type=ALPR and manufacturer="Flock Safety" on EVERY
            // exported row. That is the same over-claim FlockVendor was introduced
            // to kill, surviving in the export layer after the UI was fixed: an
            // Axon pole, a Ubicquia streetlight, a SoundThinking acoustic sensor
            // and a detection on a MAC in no table at all were every one of them
            // written out as a Flock ALPR camera, into the file that gets uploaded
            // to a public map.
            if(gc == FlockClassDrone) {
                // A DRONE GETS NO OSM TAGS AT ALL. man_made=surveillance means a
                // permanent installation at a fixed location; an aircraft is a
                // transient observation that has already moved. Tagging it would
                // invite importing a passing drone into OSM as a camera that is
                // not there and never was.
                rfile_puts(&geo, "        \"flipdeflock:class\": \"drone\",\n");
            } else {
                rfile_printf(
                    &geo,
                    line,
                    "        \"man_made\": \"surveillance\",\n"
                    "        \"surveillance:type\": \"%s\",\n",
                    (gc == FlockClassAcoustic) ? "acoustic" :
                    (gc == FlockClassBodycam)  ? "camera" :
                    (gc == FlockClassGear)     ? "unknown" :
                                                 "ALPR");
                // Named ONLY when a vendor table actually matched. Attributing
                // hardware to a company on no evidence is worse than leaving the
                // field out, and OSM consumers treat manufacturer as a fact.
                if(gv != FlockVendorUnknown) {
                    rfile_printf(
                        &geo, line, "        \"manufacturer\": \"%s\",\n", flock_vendor_str(gv));
                }
            }

            rfile_printf(
                &geo,
                line,
                "        \"flipdeflock:confidence\": \"%s\",\n"
                "        \"flipdeflock:heading\": %s,\n"
                "        \"flipdeflock:mac\": \"%s\",\n"
                "        \"flipdeflock:ssid\": \"%s\"\n"
                "      }\n"
                "    }",
                flock_confidence_str(e->confidence),
                head_s,
                mac_s,
                ssid_json);

            // THE OPERATOR, AS ITS OWN FEATURE. A separate point rather than a
            // property, because it is a different place -- typically the better
            // part of a kilometre from the aircraft -- and the whole reason to
            // plot it is to see where it is relative to you.
            //
            // It is NOT the operator of this device in the FlipDeFlock sense; it
            // is the drone's pilot, whose position the aircraft is broadcasting
            // because federal law requires it to. Explicitly roled so nothing
            // downstream can mistake it for the aircraft.
            if(gc == FlockClassDrone && !isnan(e->op_lat) && !isnan(e->op_lon)) {
                char op_lat_s[16];
                char op_lon_s[16];
                fmt_coord(op_lat_s, sizeof(op_lat_s), e->op_lat, "-");
                fmt_coord(op_lon_s, sizeof(op_lon_s), e->op_lon, "-");
                rfile_puts(&geo, ",\n");
                rfile_printf(
                    &geo,
                    line,
                    "    {\n"
                    "      \"type\": \"Feature\",\n"
                    "      \"geometry\": { \"type\": \"Point\", \"coordinates\": [%s, %s] },\n"
                    "      \"properties\": {\n"
                    "        \"flipdeflock:class\": \"drone_operator\",\n"
                    "        \"flipdeflock:role\": \"operator\",\n"
                    "        \"flipdeflock:uas_id\": \"%s\",\n"
                    "        \"flipdeflock:mac\": \"%s\"\n"
                    "      }\n"
                    "    }",
                    op_lon_s,
                    op_lat_s,
                    ssid_json,
                    mac_s);
            }

            rfile_printf(
                &kml,
                line,
                "<Placemark><name>%s %s</name>"
                "<description>%s %s</description>"
                "<Point><coordinates>%s,%s,0</coordinates></Point></Placemark>\n",
                // Was hardcoded "Flock", on every row, for the same reason the
                // GeoJSON was.
                flock_class_str(gc),
                flock_confidence_str(e->confidence),
                mac_s,
                ssid_xml,
                lon_s,
                lat_s);
        }
    }

    furi_mutex_release(app->mutex);

    rfile_printf(&md, line, "\nTotal exported: %d\n", n);

    // AIRCRAFT GET THEIR OWN SECTION rather than more columns on the table above.
    // Remote ID carries fields nothing else here has -- an aircraft type, a
    // self-declared serial, and the OPERATOR's position -- and widening the main
    // table with three columns that are empty for every camera would make the
    // common case harder to read in order to serve the rare one.
    //
    // THE OPERATOR POSITION IS NOT REDACTED, and that is not an oversight.
    // Redaction here strips what describes the person HOLDING the Flipper. This
    // describes the pilot of an aircraft overhead, broadcast in the clear because
    // 14 CFR Part 89 requires it to be. It is the payload of the detection, not a
    // leak from it.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    int drones = 0;
    for(size_t i = 0; i < app->flock_count; i++) {
        FlockEntry* e = &app->flock[i];
        if(!all && !e->marked) continue;
        if(e->dev_class != (uint8_t)FlockClassDrone) continue;
        if(drones == 0) {
            rfile_puts(
                &md,
                "\n## Unmanned aircraft (ASTM F3411 Remote ID)\n\n"
                "Broadcast by the aircraft itself, in the clear, as US federal law\n"
                "requires. `Pilot lat`/`Pilot lon` is the OPERATOR's position, not\n"
                "the aircraft's -- the two are usually several hundred metres apart.\n\n"
                "| # | UAS ID | Type | Aircraft lat | Aircraft lon | Pilot lat | Pilot lon |\n"
                "|---|--------|------|--------------|--------------|-----------|-----------|\n");
        }
        drones++;
        char id_md[64];
        md_escape(e->ssid[0] ? e->ssid : "(not yet seen)", id_md, sizeof(id_md));
        char la[16], lo[16], ola[16], olo[16];
        fmt_coord(la, sizeof(la), e->lat, "-");
        fmt_coord(lo, sizeof(lo), e->lon, "-");
        fmt_coord(ola, sizeof(ola), e->op_lat, "-");
        fmt_coord(olo, sizeof(olo), e->op_lon, "-");
        rfile_printf(
            &md,
            line,
            "| %d | %s | %s | %s | %s | %s | %s |\n",
            drones,
            id_md,
            odid_ua_type_str(e->ua_type),
            la,
            lo,
            ola,
            olo);
    }
    furi_mutex_release(app->mutex);
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

bool recon_report_save_missed(void* _app, char* out_path_md, size_t out_len) {
    ReconApp* app = _app;

    recon_report_ensure_dirs(app);

    char ts[24];
    recon_report_timestamp(ts, sizeof(ts));

    char path_md[128];
    snprintf(path_md, sizeof(path_md), "%s/missed_%s.md", RECON_REPORT_FOLDER, ts);

    char* line = malloc(REPORT_LINE_MAX);
    if(!line) return false;
    RFile md;
    rfile_open(&md, app->storage, path_md);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool have_fix = app->gps_valid;
    float lat = app->gps_lat;
    float lon = app->gps_lon;
    uint32_t msgs = app->diag_flock_msgs;
    uint32_t acc = app->diag_accepted;
    uint32_t rej_c = app->diag_rej_conf;
    uint32_t rej_f = app->diag_rej_full;
    uint32_t rej_i = app->diag_rej_ignored;
    uint32_t frames = app->esp_frames;
    uint32_t ehits = app->esp_hits;
    uint32_t elines = app->esp_lines;
    uint16_t band_ch = app->esp_band_channels;
    uint8_t band_act = app->esp_band_actual;
    uint8_t proto = app->esp_proto_version;
    bool sig_bad = app->esp_sig_mismatch;
    size_t hits = app->flock_count;
    size_t surveyed = app->survey_count;
    char esp_build[12];
    snprintf(esp_build, sizeof(esp_build), "%s", app->esp_build[0] ? app->esp_build : "-");
    furi_mutex_release(app->mutex);

    bool paired = recon_companion_build_matches(esp_build, RECON_VERSION);

    char lat_s[16], lon_s[16];
    fmt_coord(lat_s, sizeof(lat_s), lat, "-");
    fmt_coord(lon_s, sizeof(lon_s), lon, "-");

    // THE LABEL IS THE WHOLE POINT OF THIS FILE. Every field report this project
    // has received arrived as an unlabelled table, and the one question that
    // could not be answered from it was "which row was the camera you could
    // physically see" -- asked verbatim, more than once, and unanswerable after
    // the fact. A miss is only evidence if the ground truth is captured while
    // the operator is still standing in front of the thing.
    rfile_printf(
        &md,
        line,
        "# FlipDeFlock - Missed Camera Report\n\n"
        "App: %s   Companion: %s%s   Generated: %s (device RTC)\n\n"
        "**The operator says there is an ALPR camera at this location and the\n"
        "detector did not flag it.** That claim is the data; everything below is\n"
        "the air picture and the detector's own state at the moment it was made.\n\n"
        "Coordinates are KEPT, like the DeFlock export and unlike the false\n"
        "positive report: this file is a claim about a camera's position, so\n"
        "stripping the position would leave nothing to check it against.\n\n"
        "## The claim\n\n"
        "| | |\n|---|---|\n"
        "| Camera position | %s, %s |\n"
        "| GPS | %s |\n"
        "| Detections in the table | %u |\n"
        "| Transmitters in the survey | %u |\n\n",
        RECON_VERSION,
        esp_build,
        paired ? "" : " (NOT the paired build)",
        ts,
        lat_s,
        lon_s,
        have_fix ? "fix" : "NO FIX - the position above is unusable",
        (unsigned)hits,
        (unsigned)surveyed);

    // WHY IT DID NOT FIRE, as counters rather than prose. "Nothing showed up"
    // has at least four distinct causes that look identical from the outside,
    // and this is the only place they separate: a deaf radio (frames flat), a
    // companion that heard plenty and binned it (hits climbing, accepted flat),
    // a full table, or the operator's own exclusion list quietly eating it.
    rfile_printf(
        &md,
        line,
        "## Why nothing fired\n\n"
        "| Stage | Count |\n|---|---|\n"
        "| Companion Wi-Fi frames | %lu |\n"
        "| Companion hits | %lu |\n"
        "| Companion lines to the app | %lu |\n"
        "| Reports handed to the app | %lu |\n"
        "| Accepted into the table | %lu |\n"
        "| Dropped, scored nothing | %lu |\n"
        "| Dropped, table full | %lu |\n"
        "| Dropped, operator excluded | %lu |\n\n"
        "Frames at 0 means the radio heard nothing at all and no signature could\n"
        "have helped. Frames climbing with reports at 0 means the companion heard\n"
        "the street and recognised none of it, which is the case a new signature\n"
        "fixes. A non-zero exclusion count means this card is suppressing\n"
        "something on purpose: check Reports, Forget Ignored before concluding\n"
        "anything from this file.\n\n"
        "Band in force: %s (%u channels).  Protocol v%u.  Signature table: %s.\n\n",
        (unsigned long)frames,
        (unsigned long)ehits,
        (unsigned long)elines,
        (unsigned long)msgs,
        (unsigned long)acc,
        (unsigned long)rej_c,
        (unsigned long)rej_f,
        (unsigned long)rej_i,
        band_act == ReconEspBand5 ? "5 GHz" : (band_act == ReconEspBandAll ? "both" : "2.4 GHz"),
        (unsigned)band_ch,
        (unsigned)proto,
        sig_bad ? "STALE, reflash the companion" : "matches this app");

    // A 2.4-only board parked under a camera uplinking on 5 GHz is a miss that
    // no table can explain, so it is called out rather than left implicit.
    if(band_ch > 0 && band_act == ReconEspBand24) {
        rfile_puts(
            &md,
            "> This board swept 2.4 GHz only. A camera whose uplink is on 5 GHz\n"
            "> cannot appear below, and that is a hardware limit rather than a\n"
            "> missing signature.\n\n");
    }

    // The survey is the half that can still carry the camera. A randomising
    // transmitter never becomes a detection, so for a modern camera this table
    // is the only place it is recorded at all.
    rfile_puts(
        &md,
        "## Everything probing at this spot\n\n"
        "`~` marks a randomised (locally administered) address, which modern\n"
        "cameras and modern phones both use. `IE sig` is the readable element\n"
        "list -- that, not the hash, is what another capture can be compared\n"
        "against by eye.\n\n"
        "| # | Addr | ~ | Ch | RSSI | Probes | fp | fp2 | IE sig |\n"
        "|---|------|---|----|------|--------|----|-----|--------|\n");

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    int n = 0;
    for(size_t i = 0; i < app->survey_count; i++) {
        SurveyEntry* e = &app->survey[i];
        n++;
        bool local = survey_mac_is_local(e->mac);

        // ADDRESS POLICY, and it differs from every other report here on purpose.
        //
        // A randomised address is invented by the device: no manufacturer stands
        // behind it, it is in no registry, and it cannot be looked up as
        // somebody's home network. It is also the ONE case where the full
        // address is itself the signature -- a camera that randomises once at
        // boot and then keeps that address is pinnable by address alone, which
        // is exactly what Air Survey's Flag MAC does. Reducing it to an OUI
        // would delete the only usable identifier for the device this file
        // exists to find.
        //
        // A globally unique address is the opposite: it names a real
        // manufacturer's unit and belongs to somebody, nearly always a resident
        // rather than a camera, so it drops to its OUI like everywhere else.
        char addr[20];
        if(local) {
            fmt_mac(addr, sizeof(addr), e->mac);
        } else {
            fmt_mac_oui(addr, sizeof(addr), e->mac);
        }

        char sig_raw[RECON_SURVEY_SIG_LEN + 8];
        md_escape(e->sig[0] ? e->sig : "-", sig_raw, sizeof(sig_raw));

        rfile_printf(
            &md,
            line,
            "| %d | %s | %s | %u | %d | %u | %08lX | %08lX | `%s` |\n",
            n,
            addr,
            local ? "~" : "-",
            e->channel,
            e->rssi,
            (unsigned)e->count,
            (unsigned long)e->fp,
            (unsigned long)e->fp2,
            sig_raw);
    }
    furi_mutex_release(app->mutex);

    if(n == 0) {
        rfile_puts(
            &md,
            "| - | (nothing) | - | - | - | - | - | - | - |\n\n"
            "An EMPTY survey next to a camera the operator can see is the\n"
            "strongest result in this file: the board heard no wildcard probes at\n"
            "all, so the camera is either silent, on a band this board cannot\n"
            "sweep, or the receive path is broken. Read the frame count above\n"
            "before hunting for a signature.\n");
    } else {
        rfile_printf(&md, line, "\nTransmitters: %d\n", n);
    }

    bool ok = rfile_close(&md);
    free(line);

    if(!ok) {
        storage_simply_remove(app->storage, path_md);
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

// storage.c — JSONL + CSV (SDK >1.3.x): append via FSOM_OPEN_APPEND
#include <furi.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static const char* DIR = "/ext/apps_data/osm_logger";

static bool ensure_dir(Storage* s) {
    FileInfo fi;
    if(storage_common_stat(s, DIR, &fi) == FSE_OK) return true;
    FS_Error err = storage_common_mkdir(s, DIR);
    if(err == FSE_OK || err == FSE_EXIST) return true;
    FURI_LOG_E("OSM", "mkdir %s failed: %d", DIR, (int)err);
    return false;
}

static bool append_line(Storage* s, const char* path, const char* line) {
    File* f = storage_file_alloc(s);
    if(!f) return false;

    if(!storage_file_open(f, path, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_free(f);
        return false;
    }

    storage_file_write(f, line, strlen(line));
    storage_file_write(f, "\n", 1);

    storage_file_close(f);
    storage_file_free(f);
    return true;
}

static void sanitize_quotes(char* s) {
    if(!s) return;
    for(char* p = s; *p; ++p) if(*p == '"') *p = '\'';
}

// Stratégie GPX / GeoJSON : garder un fichier valide entre deux saves.
// On écrit header + body + footer. Aux saves suivants, on seek juste avant
// le footer, on truncate, puis on écrit (prefix) + body + footer.
static void write_append_framed(
    Storage* s,
    const char* path,
    const char* header,              // utilisé seulement à la création du fichier
    const char* body,                // contenu principal à ajouter
    const char* body_prefix,         // écrit AVANT body si le fichier existait déjà (ex. "," pour séparer)
    const char* footer,
    size_t footer_len) {
    FileInfo fi;
    bool exists = (storage_common_stat(s, path, &fi) == FSE_OK) && (fi.size >= footer_len);

    File* f = storage_file_alloc(s);
    if(!f) return;

    if(!exists) {
        if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            if(header) storage_file_write(f, header, strlen(header));
            storage_file_write(f, body, strlen(body));
            if(footer) storage_file_write(f, footer, footer_len);
            storage_file_close(f);
        }
    } else {
        if(storage_file_open(f, path, FSAM_READ_WRITE, FSOM_OPEN_EXISTING)) {
            uint64_t size = storage_file_size(f);
            if(footer && size >= footer_len) {
                storage_file_seek(f, (uint32_t)(size - footer_len), true);
                storage_file_truncate(f);
            }
            if(body_prefix) storage_file_write(f, body_prefix, strlen(body_prefix));
            storage_file_write(f, body, strlen(body));
            if(footer) storage_file_write(f, footer, footer_len);
            storage_file_close(f);
        }
    }

    storage_file_free(f);
}

// GPX header avec namespace OsmAnd pour les extensions Favorites.
// Les apps qui ne connaissent pas osmand (JOSM, iD, QGIS, ...) ignorent
// silencieusement les extensions, c'est compatible standard GPX 1.1.
static const char GPX_HEADER[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<gpx version=\"1.1\" creator=\"Flipper Zero OSM Logger\" "
    "xmlns=\"http://www.topografix.com/GPX/1/1\" "
    "xmlns:osmand=\"https://osmand.net\">\n";
static const char GPX_FOOTER[] = "</gpx>\n";

// Format OSM XML API 0.6 — chargeable comme data layer dans JOSM.
// `upload="false"` : JOSM considère le layer comme une review avant upload,
// les IDs négatifs sont le protocole standard pour les nouveaux nodes.
static const char OSM_XML_HEADER[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<osm version=\"0.6\" generator=\"Flipper Zero OSM Logger\" upload=\"false\">\n";
static const char OSM_XML_FOOTER[] = "</osm>\n";

// Échappement XML minimal (& < > " ') sur out_size octets max.
static size_t xml_escape(char* dst, size_t dst_size, const char* src) {
    if(!dst || dst_size == 0) return 0;
    size_t o = 0;
    while(src && *src && o + 6 < dst_size) {
        if(*src == '&') { memcpy(&dst[o], "&amp;", 5); o += 5; }
        else if(*src == '<') { memcpy(&dst[o], "&lt;", 4); o += 4; }
        else if(*src == '>') { memcpy(&dst[o], "&gt;", 4); o += 4; }
        else if(*src == '"') { memcpy(&dst[o], "&quot;", 6); o += 6; }
        else if(*src == '\'') { memcpy(&dst[o], "&apos;", 6); o += 6; }
        else dst[o++] = *src;
        src++;
    }
    dst[o] = '\0';
    return o;
}

static const char GEOJSON_HEADER[] = "{\"type\":\"FeatureCollection\",\"features\":[\n";
static const char GEOJSON_FOOTER[] = "\n]}\n";

void storage_write_all_formats(
    float lat,
    float lon,
    float altitude,
    float hdop,
    uint8_t sats,
    const char* tag,
    const char* note,
    const char* display_name,
    const char* category_label,
    const char* osmand_icon,
    const char* osmand_color,
    uint32_t node_seq) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    if(!ensure_dir(s)) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char iso[32];
    snprintf(
        iso,
        sizeof(iso),
        "%04u-%02u-%02uT%02u:%02u:%02uZ",
        dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

    const char* tag_s = tag ? tag : "";

    // JSONL
    {
        char buf[320];
        snprintf(
            buf,
            sizeof(buf),
            "{\"time\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,\"alt\":%.1f,"
            "\"hdop\":%.1f,\"sats\":%u,\"tag\":\"%s\",\"note\":\"%s\"}",
            iso,
            (double)lat,
            (double)lon,
            (double)altitude,
            (double)hdop,
            (unsigned)sats,
            tag_s,
            note ? note : "");
        FURI_LOG_D("OSM", "write: jsonl start");
        append_line(s, "/ext/apps_data/osm_logger/points.jsonl", buf);
        FURI_LOG_D("OSM", "write: jsonl done");
    }

    // CSV : time,lat,lon,alt,hdop,sats,tag,note
    {
        char note_copy[96] = {0};
        if(note) {
            strncpy(note_copy, note, sizeof(note_copy) - 1);
            note_copy[sizeof(note_copy) - 1] = '\0';
            sanitize_quotes(note_copy);
        }
        char buf[320];
        snprintf(
            buf,
            sizeof(buf),
            "\"%s\",%.6f,%.6f,%.1f,%.1f,%u,\"%s\",\"%s\"",
            iso,
            (double)lat,
            (double)lon,
            (double)altitude,
            (double)hdop,
            (unsigned)sats,
            tag_s,
            note_copy);
        FURI_LOG_D("OSM", "write: csv start");
        append_line(s, "/ext/apps_data/osm_logger/notes.csv", buf);
        FURI_LOG_D("OSM", "write: csv done");
    }

    // GPX (waypoint enrichi avec extensions OsmAnd Favorites)
    // - <name>       : label humain lisible (ex. "Bench")
    // - <desc>       : infos techniques (tag OSM + HDOP + sats)
    // - <type>       : catégorie (ex. "Street furniture")
    // - <extensions> : icon + couleur + forme pour OsmAnd Favorites
    {
        const char* name = (display_name && display_name[0]) ? display_name : tag_s;
        const char* cat = (category_label && category_label[0]) ? category_label : "Other";
        const char* icon = (osmand_icon && osmand_icon[0]) ? osmand_icon : "special_marker";
        const char* color = (osmand_color && osmand_color[0]) ? osmand_color : "#c0c0c0";

        char wpt[512];
        snprintf(
            wpt,
            sizeof(wpt),
            "  <wpt lat=\"%.6f\" lon=\"%.6f\">\n"
            "    <ele>%.1f</ele>\n"
            "    <time>%s</time>\n"
            "    <name>%s</name>\n"
            "    <desc>%s  HDOP=%.1f sats=%u</desc>\n"
            "    <type>%s</type>\n"
            "    <extensions>\n"
            "      <osmand:icon>%s</osmand:icon>\n"
            "      <osmand:background>circle</osmand:background>\n"
            "      <osmand:color>%s</osmand:color>\n"
            "    </extensions>\n"
            "  </wpt>\n",
            (double)lat,
            (double)lon,
            (double)altitude,
            iso,
            name,
            tag_s,
            (double)hdop,
            (unsigned)sats,
            cat,
            icon,
            color);
        FURI_LOG_D("OSM", "write: gpx start");
        write_append_framed(
            s,
            "/ext/apps_data/osm_logger/points.gpx",
            GPX_HEADER,
            wpt,
            NULL,
            GPX_FOOTER,
            sizeof(GPX_FOOTER) - 1);
        FURI_LOG_D("OSM", "write: gpx done");
    }

    // GeoJSON (FeatureCollection)
    {
        char feat[320];
        snprintf(
            feat,
            sizeof(feat),
            "  {\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\","
            "\"coordinates\":[%.6f,%.6f,%.1f]},"
            "\"properties\":{\"time\":\"%s\",\"tag\":\"%s\","
            "\"hdop\":%.1f,\"sats\":%u}}",
            (double)lon, // GeoJSON convention: [lon, lat, alt]
            (double)lat,
            (double)altitude,
            iso,
            tag_s,
            (double)hdop,
            (unsigned)sats);
        FURI_LOG_D("OSM", "write: geojson start");
        write_append_framed(
            s,
            "/ext/apps_data/osm_logger/points.geojson",
            GEOJSON_HEADER,
            feat,
            ",\n", // séparateur entre Features si le fichier existait déjà
            GEOJSON_FOOTER,
            sizeof(GEOJSON_FOOTER) - 1);
        FURI_LOG_D("OSM", "write: geojson done");
    }

    // OSM XML (API 0.6) — data layer chargeable directement dans JOSM.
    // Chaque point devient un <node> avec ID négatif (convention OSM pour
    // nouveaux nodes pas encore uploadés). Les tags multi-clés (séparés par
    // ';' dans `tag_s`) sont éclatés en plusieurs <tag k="..." v="..."/>.
    {
        char node[640];
        int w = 0;
        w += snprintf(
            node + w,
            sizeof(node) - w,
            "  <node id=\"-%lu\" lat=\"%.6f\" lon=\"%.6f\" visible=\"true\">\n",
            (unsigned long)node_seq,
            (double)lat,
            (double)lon);

        // Parse tag_s "k1=v1;k2=v2;..." et émet un <tag k v/> par paire
        if(tag_s && tag_s[0]) {
            char tag_copy[160];
            strncpy(tag_copy, tag_s, sizeof(tag_copy) - 1);
            tag_copy[sizeof(tag_copy) - 1] = '\0';
            char* token = tag_copy;
            while(token && *token && w < (int)sizeof(node) - 80) {
                char* semi = strchr(token, ';');
                if(semi) *semi = '\0';
                char* eq = strchr(token, '=');
                if(eq) {
                    *eq = '\0';
                    const char* k = token;
                    const char* v = eq + 1;
                    char v_esc[64];
                    xml_escape(v_esc, sizeof(v_esc), v);
                    w += snprintf(
                        node + w, sizeof(node) - w,
                        "    <tag k=\"%s\" v=\"%s\"/>\n", k, v_esc);
                }
                if(!semi) break;
                token = semi + 1;
            }
        }

        // Altitude en tag ele= (convention OSM)
        if(altitude != 0.0f && w < (int)sizeof(node) - 60) {
            w += snprintf(
                node + w, sizeof(node) - w,
                "    <tag k=\"ele\" v=\"%.1f\"/>\n", (double)altitude);
        }

        // Note utilisateur en tag note= (convention OSM)
        if(note && note[0] && w < (int)sizeof(node) - 80) {
            char note_esc[128];
            xml_escape(note_esc, sizeof(note_esc), note);
            w += snprintf(
                node + w, sizeof(node) - w,
                "    <tag k=\"note\" v=\"%s\"/>\n", note_esc);
        }

        // Métadonnées FlipperOSM (pour retrouver nos points dans JOSM)
        if(w < (int)sizeof(node) - 80) {
            w += snprintf(
                node + w, sizeof(node) - w,
                "    <tag k=\"flipper:hdop\" v=\"%.1f\"/>\n"
                "    <tag k=\"flipper:sats\" v=\"%u\"/>\n"
                "    <tag k=\"flipper:time\" v=\"%s\"/>\n",
                (double)hdop, (unsigned)sats, iso);
        }

        w += snprintf(node + w, sizeof(node) - w, "  </node>\n");

        FURI_LOG_D("OSM", "write: osm start");
        write_append_framed(
            s,
            "/ext/apps_data/osm_logger/points.osm",
            OSM_XML_HEADER,
            node,
            NULL,
            OSM_XML_FOOTER,
            sizeof(OSM_XML_FOOTER) - 1);
        FURI_LOG_D("OSM", "write: osm done");
    }

    furi_record_close(RECORD_STORAGE);
}

uint32_t storage_count_saved_points(void) {
    const char* path = "/ext/apps_data/osm_logger/points.jsonl";
    Storage* s = furi_record_open(RECORD_STORAGE);
    uint32_t count = 0;

    File* f = storage_file_alloc(s);
    if(f && storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char chunk[256];
        uint16_t read;
        do {
            read = storage_file_read(f, chunk, sizeof(chunk));
            for(uint16_t i = 0; i < read; i++) {
                if(chunk[i] == '\n') count++;
            }
        } while(read > 0);
        storage_file_close(f);
    }
    if(f) storage_file_free(f);

    furi_record_close(RECORD_STORAGE);
    return count;
}

static const char TRACK_HEADER[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<gpx version=\"1.1\" creator=\"Flipper Zero OSM Logger\" "
    "xmlns=\"http://www.topografix.com/GPX/1/1\">\n"
    "<trk><name>Track</name>\n"
    "<trkseg>\n";
static const char TRACK_FOOTER[] = "</trkseg>\n</trk>\n</gpx>\n";

// --- Helpers lecture/troncature (pour Browse + Undo) ---

// Lit tout le fichier en mémoire (max size_max octets). Retourne NULL si
// le fichier n'existe pas, est trop gros, ou alloc échouée. Le buffer retourné
// doit être free() par l'appelant et peut être muté en place.
static char* read_whole_file(Storage* s, const char* path, uint32_t* out_size, uint32_t size_max) {
    FileInfo fi;
    if(storage_common_stat(s, path, &fi) != FSE_OK) return NULL;
    if(fi.size == 0 || fi.size > size_max) return NULL;

    File* f = storage_file_alloc(s);
    if(!f) return NULL;
    char* buf = NULL;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        buf = malloc((size_t)fi.size + 1);
        if(buf) {
            uint16_t read = storage_file_read(f, buf, (uint16_t)fi.size);
            buf[read] = '\0';
            if(out_size) *out_size = read;
        }
        storage_file_close(f);
    }
    storage_file_free(f);
    return buf;
}

// Écrit un buffer en remplacement du contenu d'un fichier (CREATE_ALWAYS).
static void rewrite_file(Storage* s, const char* path, const char* buf, size_t size) {
    File* f = storage_file_alloc(s);
    if(!f) return;
    if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        if(size > 0) storage_file_write(f, buf, size);
        storage_file_close(f);
    }
    storage_file_free(f);
}

// JSONL / CSV : supprime la dernière ligne (truncate avant le '\n' précédent).
static bool trim_last_line(Storage* s, const char* path) {
    uint32_t size = 0;
    char* buf = read_whole_file(s, path, &size, 65536);
    if(!buf) return false;
    if(size == 0) { free(buf); return false; }

    // trim trailing \n
    size_t end = size;
    if(end > 0 && buf[end - 1] == '\n') end--;
    if(end > 0 && buf[end - 1] == '\r') end--;
    // find previous \n
    size_t prev = end;
    while(prev > 0 && buf[prev - 1] != '\n') prev--;

    rewrite_file(s, path, buf, prev);
    free(buf);
    return true;
}

// GPX : supprime le dernier <wpt>..</wpt> et réécrit le footer.
static bool trim_last_gpx_wpt(Storage* s) {
    const char* path = "/ext/apps_data/osm_logger/points.gpx";
    uint32_t size = 0;
    char* buf = read_whole_file(s, path, &size, 65536);
    if(!buf) return false;

    // Trouve la dernière occurrence de "  <wpt "
    char* last = NULL;
    for(char* p = buf; p + 7 < buf + size; p++) {
        if(!memcmp(p, "  <wpt ", 7)) last = p;
    }
    if(!last) { free(buf); return false; }

    size_t new_size = (size_t)(last - buf);
    // Réécrit jusqu'à last, puis rajoute le footer
    File* f = storage_file_alloc(s);
    if(f && storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, buf, new_size);
        storage_file_write(f, GPX_FOOTER, sizeof(GPX_FOOTER) - 1);
        storage_file_close(f);
    }
    if(f) storage_file_free(f);
    free(buf);
    return true;
}

// OSM XML : supprime le dernier <node>...</node> et réécrit le footer.
static bool trim_last_osm_node(Storage* s) {
    const char* path = "/ext/apps_data/osm_logger/points.osm";
    uint32_t size = 0;
    char* buf = read_whole_file(s, path, &size, 131072); // 128K pour .osm (gros fichier)
    if(!buf) return false;

    // Trouve la dernière occurrence de "  <node "
    char* last = NULL;
    for(char* p = buf; p + 8 < buf + size; p++) {
        if(!memcmp(p, "  <node ", 8)) last = p;
    }
    if(!last) {
        free(buf);
        return false;
    }

    size_t new_size = (size_t)(last - buf);
    File* f = storage_file_alloc(s);
    if(f && storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, buf, new_size);
        storage_file_write(f, OSM_XML_FOOTER, sizeof(OSM_XML_FOOTER) - 1);
        storage_file_close(f);
    }
    if(f) storage_file_free(f);
    free(buf);
    return true;
}

// GeoJSON : supprime la dernière Feature et réécrit le footer.
static bool trim_last_geojson_feature(Storage* s) {
    const char* path = "/ext/apps_data/osm_logger/points.geojson";
    uint32_t size = 0;
    char* buf = read_whole_file(s, path, &size, 65536);
    if(!buf) return false;

    // Cherche la dernière occurrence de "  {\"type\":\"Feature\""
    const char* marker = "  {\"type\":\"Feature\"";
    size_t mlen = strlen(marker);
    char* last = NULL;
    for(char* p = buf; p + mlen < buf + size; p++) {
        if(!memcmp(p, marker, mlen)) last = p;
    }
    if(!last) { free(buf); return false; }

    // On veut tronquer juste avant la virgule+\n qui précède (ou juste après le header).
    // Si last est précédé de ",\n" on tronque avant.
    size_t cut = (size_t)(last - buf);
    if(cut >= 2 && buf[cut - 1] == '\n' && buf[cut - 2] == ',') {
        cut -= 2;
    } else if(cut >= 1 && buf[cut - 1] == '\n') {
        cut -= 1; // premier Feature, pas de virgule avant
    }

    File* f = storage_file_alloc(s);
    if(f && storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, buf, cut);
        storage_file_write(f, GEOJSON_FOOTER, sizeof(GEOJSON_FOOTER) - 1);
        storage_file_close(f);
    }
    if(f) storage_file_free(f);
    free(buf);
    return true;
}

bool storage_delete_last_point(void) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    bool any = false;
    if(trim_last_line(s, "/ext/apps_data/osm_logger/points.jsonl")) any = true;
    if(trim_last_line(s, "/ext/apps_data/osm_logger/notes.csv")) any = true;
    trim_last_gpx_wpt(s);         // best-effort
    trim_last_geojson_feature(s); // best-effort
    trim_last_osm_node(s);        // best-effort
    furi_record_close(RECORD_STORAGE);
    return any;
}

void storage_clear_all_points(void) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    storage_common_remove(s, "/ext/apps_data/osm_logger/points.jsonl");
    storage_common_remove(s, "/ext/apps_data/osm_logger/notes.csv");
    storage_common_remove(s, "/ext/apps_data/osm_logger/points.gpx");
    storage_common_remove(s, "/ext/apps_data/osm_logger/points.geojson");
    storage_common_remove(s, "/ext/apps_data/osm_logger/points.osm");
    furi_record_close(RECORD_STORAGE);
}

// Extrait le contenu d'un champ JSON simple "key":"value" à partir de line.
// Copie jusqu'à max_len - 1 caractères dans out. Retourne true si trouvé.
static bool json_extract_string(
    const char* line,
    size_t line_len,
    const char* key_quoted,
    char* out,
    size_t max_len) {
    size_t klen = strlen(key_quoted);
    if(klen >= line_len) return false;
    for(size_t i = 0; i + klen < line_len; i++) {
        if(!memcmp(&line[i], key_quoted, klen)) {
            size_t j = i + klen;
            if(j >= line_len || line[j] != ':') continue;
            j++;
            if(j >= line_len || line[j] != '"') continue;
            j++;
            // Copie jusqu'au " suivant
            size_t o = 0;
            while(j < line_len && line[j] != '"' && o < max_len - 1) {
                out[o++] = line[j++];
            }
            out[o] = '\0';
            return true;
        }
    }
    return false;
}

uint8_t storage_read_last_points(char* out_lines, uint8_t max_lines, size_t line_size) {
    if(!out_lines || max_lines == 0) return 0;
    Storage* s = furi_record_open(RECORD_STORAGE);
    uint32_t size = 0;
    char* buf = read_whole_file(s, "/ext/apps_data/osm_logger/points.jsonl", &size, 65536);
    furi_record_close(RECORD_STORAGE);
    if(!buf || size == 0) {
        if(buf) free(buf);
        return 0;
    }

    // Trouve les offsets des N dernières lignes (séparateur \n).
    uint32_t line_starts[32];
    if(max_lines > 32) max_lines = 32;
    uint8_t n_lines = 0;
    size_t end = size;
    if(end > 0 && buf[end - 1] == '\n') end--; // ignore trailing \n
    size_t cursor = end;
    while(n_lines < max_lines) {
        // Cherche le \n précédent depuis cursor
        size_t start = cursor;
        while(start > 0 && buf[start - 1] != '\n') start--;
        line_starts[n_lines++] = start;
        if(start == 0) break;
        cursor = start - 1; // saute le \n
    }

    // Reformate chaque ligne (de la plus récente à la plus ancienne)
    for(uint8_t i = 0; i < n_lines; i++) {
        size_t s0 = line_starts[i];
        size_t s1 = (i == 0) ? end : line_starts[i - 1] - 1;
        if(s1 > end) s1 = end;
        size_t linelen = (s1 > s0) ? (s1 - s0) : 0;
        const char* line = &buf[s0];

        char time[32] = "?";
        char tag[48] = "?";
        json_extract_string(line, linelen, "\"time\"", time, sizeof(time));
        json_extract_string(line, linelen, "\"tag\"", tag, sizeof(tag));

        // Time est ISO "2026-04-19T10:42:18Z" -> on garde juste "10:42"
        char short_time[6] = "--:--";
        size_t tlen = strlen(time);
        if(tlen >= 16) {
            short_time[0] = time[11];
            short_time[1] = time[12];
            short_time[2] = ':';
            short_time[3] = time[14];
            short_time[4] = time[15];
            short_time[5] = '\0';
        }

        char* dest = out_lines + (size_t)i * line_size;
        snprintf(dest, line_size, "%s  %s", short_time, tag);
    }

    free(buf);
    return n_lines;
}

bool storage_get_point_raw(uint8_t idx_from_end, char* out, size_t out_size) {
    if(!out || out_size == 0) return false;
    Storage* s = furi_record_open(RECORD_STORAGE);
    uint32_t size = 0;
    char* buf = read_whole_file(s, "/ext/apps_data/osm_logger/points.jsonl", &size, 65536);
    furi_record_close(RECORD_STORAGE);
    if(!buf || size == 0) {
        if(buf) free(buf);
        out[0] = '\0';
        return false;
    }

    size_t end = size;
    if(end > 0 && buf[end - 1] == '\n') end--;

    // Parcourt les lignes depuis la fin
    size_t cursor = end;
    for(uint8_t i = 0; ; i++) {
        size_t start = cursor;
        while(start > 0 && buf[start - 1] != '\n') start--;
        if(i == idx_from_end) {
            size_t linelen = cursor - start;
            if(linelen >= out_size) linelen = out_size - 1;
            memcpy(out, &buf[start], linelen);
            out[linelen] = '\0';
            free(buf);
            return true;
        }
        if(start == 0) break;
        cursor = start - 1;
    }

    free(buf);
    out[0] = '\0';
    return false;
}

bool storage_archive_session(char* err, size_t err_size) {
    if(err && err_size > 0) err[0] = '\0';

    Storage* s = furi_record_open(RECORD_STORAGE);

    // Timestamp pour le nom de dossier
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    char folder[96];
    snprintf(
        folder,
        sizeof(folder),
        "/ext/apps_data/osm_logger/session_%04u%02u%02uT%02u%02u%02u",
        dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

    FS_Error r = storage_common_mkdir(s, folder);
    if(r != FSE_OK && r != FSE_EXIST) {
        if(err) snprintf(err, err_size, "mkdir failed");
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Renomme les fichiers (silencieux si absent)
    static const char* const files[] = {
        "points.jsonl", "notes.csv", "points.gpx", "points.geojson",
        "points.osm", "track.gpx",
    };
    bool any = false;
    for(size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        char src[96], dst[128];
        snprintf(src, sizeof(src), "/ext/apps_data/osm_logger/%s", files[i]);
        snprintf(dst, sizeof(dst), "%s/%s", folder, files[i]);
        FileInfo fi;
        if(storage_common_stat(s, src, &fi) == FSE_OK) {
            if(storage_common_rename(s, src, dst) == FSE_OK) {
                any = true;
            }
        }
    }

    if(!any && err) snprintf(err, err_size, "no files to archive");

    furi_record_close(RECORD_STORAGE);
    return any;
}

bool storage_pre_save_check(char* err, size_t err_size) {
    if(err && err_size > 0) err[0] = '\0';

    Storage* s = furi_record_open(RECORD_STORAGE);
    bool ok = true;

    // API canonique pour vérifier la SD (plus fiable que storage_common_stat("/ext"))
    FS_Error sd = storage_sd_status(s);
    if(sd != FSE_OK) {
        if(err) snprintf(err, err_size, "SD card: %s", storage_error_get_desc(sd));
        ok = false;
        goto end;
    }

    // Test définitif de l'accès disque : tente mkdir (FSE_EXIST = OK aussi).
    // Si ça échoue avec autre chose, on a un vrai problème SD.
    FS_Error r = storage_common_mkdir(s, "/ext/apps_data/osm_logger");
    if(r != FSE_OK && r != FSE_EXIST) {
        if(err) snprintf(err, err_size, "SD error: %s", storage_error_get_desc(r));
        ok = false;
        goto end;
    }

    // Espace libre (non-bloquant : si l'API échoue, on laisse passer)
    uint64_t total = 0, freeb = 0;
    if(storage_common_fs_info(s, "/ext", &total, &freeb) == FSE_OK) {
        if(freeb < 10240u) {
            if(err) snprintf(err, err_size, "Disk nearly full");
            ok = false;
            goto end;
        }
    }

end:
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static float haversine_m(float lat1, float lon1, float lat2, float lon2) {
    const float DEG_TO_RAD = 0.01745329f;
    const float M_PER_DEG_LAT = 110574.0f;
    float avg_lat_rad = (lat1 + lat2) * 0.5f * DEG_TO_RAD;
    float m_per_deg_lon = 111320.0f * cosf(avg_lat_rad);
    float dlat = (lat2 - lat1) * M_PER_DEG_LAT;
    float dlon = (lon2 - lon1) * m_per_deg_lon;
    return sqrtf(dlat * dlat + dlon * dlon);
}

// Parser atof minimaliste (atof est désactivé dans l'API Flipper).
// Gère signe optionnel, chiffres, puis point décimal.
static float simple_atof(const char* s) {
    if(!s || !*s) return 0.0f;
    float sign = 1.0f;
    if(*s == '-') { sign = -1.0f; s++; }
    else if(*s == '+') { s++; }

    float val = 0.0f;
    while(*s >= '0' && *s <= '9') {
        val = val * 10.0f + (float)(*s - '0');
        s++;
    }
    if(*s == '.') {
        s++;
        float frac = 0.1f;
        while(*s >= '0' && *s <= '9') {
            val += (float)(*s - '0') * frac;
            frac *= 0.1f;
            s++;
        }
    }
    return sign * val;
}

// Parse une ligne JSONL null-terminée pour en extraire lat, lon et tag.
static bool parse_jsonl_line(const char* line, float* out_lat, float* out_lon, char* out_tag, size_t tag_size) {
    if(!line) return false;

    const char* q = strstr(line, "\"lat\":");
    if(!q) return false;
    *out_lat = simple_atof(q + 6);

    q = strstr(line, "\"lon\":");
    if(!q) return false;
    *out_lon = simple_atof(q + 6);

    q = strstr(line, "\"tag\":\"");
    if(!q) return false;
    const char* tag_start = q + 7;
    size_t o = 0;
    while(*tag_start && *tag_start != '"' && o < tag_size - 1) {
        out_tag[o++] = *tag_start++;
    }
    out_tag[o] = '\0';
    return true;
}

bool storage_find_duplicate_nearby(
    float lat,
    float lon,
    const char* tag,
    uint8_t max_dist_m,
    float* out_dist_m) {
    if(!tag || !*tag || max_dist_m == 0) return false;
    Storage* s = furi_record_open(RECORD_STORAGE);
    uint32_t size = 0;
    char* buf = read_whole_file(s, "/ext/apps_data/osm_logger/points.jsonl", &size, 65536);
    furi_record_close(RECORD_STORAGE);
    if(!buf || size == 0) {
        if(buf) free(buf);
        return false;
    }

    bool found = false;
    float nearest = (float)max_dist_m;

    char* p = buf;
    char* end = buf + size;
    while(p < end) {
        char* eol = memchr(p, '\n', (size_t)(end - p));
        char saved = 0;
        if(eol) {
            saved = *eol;
            *eol = '\0';
        }

        float llat = 0, llon = 0;
        char ltag[64];
        if(parse_jsonl_line(p, &llat, &llon, ltag, sizeof(ltag))) {
            if(!strcmp(ltag, tag)) {
                float d = haversine_m(lat, lon, llat, llon);
                if(d < nearest) {
                    nearest = d;
                    found = true;
                }
            }
        }

        if(!eol) break;
        *eol = saved;
        p = eol + 1;
    }

    free(buf);
    if(found && out_dist_m) *out_dist_m = nearest;
    return found;
}

void storage_append_trkpt(float lat, float lon, float altitude, bool new_segment) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    if(!ensure_dir(s)) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    char iso[32];
    snprintf(
        iso,
        sizeof(iso),
        "%04u-%02u-%02uT%02u:%02u:%02uZ",
        dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

    char pt[192];
    snprintf(
        pt,
        sizeof(pt),
        "  <trkpt lat=\"%.6f\" lon=\"%.6f\"><ele>%.1f</ele><time>%s</time></trkpt>\n",
        (double)lat, (double)lon, (double)altitude, iso);

    write_append_framed(
        s,
        "/ext/apps_data/osm_logger/track.gpx",
        TRACK_HEADER,
        pt,
        new_segment ? "</trkseg>\n<trkseg>\n" : NULL,
        TRACK_FOOTER,
        sizeof(TRACK_FOOTER) - 1);

    furi_record_close(RECORD_STORAGE);
}

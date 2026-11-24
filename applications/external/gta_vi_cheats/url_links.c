#include "url_links.h"
#include "gta_catalog_io.h"
#include "gta_nfc_plugin.h"
#include <furi.h>
#include <stdio.h>
#include <string.h>

static GtaRecord url_record, category_record, extra_records[2];
static GtaUrlLinks resolved;

void gta_url_links_release(void) {
    memset(&resolved, 0, sizeof(resolved));
    gta_record_clear(&url_record);
    gta_record_clear(&category_record);
    for(size_t i = 0; i < 2; ++i)
        gta_record_clear(&extra_records[i]);
}

static const char* host_start(const char* url) {
    if(!url) return NULL;
    if(strncmp(url, "https://", 8) == 0) return url + 8;
    if(strncmp(url, "http://", 7) == 0) return url + 7;
    return NULL;
}

static bool host_is(const char* url, const char* domain) {
    const char* host = host_start(url);
    if(!host) return false;
    if(strncmp(host, "www.", 4) == 0) host += 4;
    size_t n = strlen(domain);
    return strncmp(host, domain, n) == 0 &&
           (host[n] == '/' || host[n] == '?' || host[n] == '#' || host[n] == '\0');
}

static bool is_gps(const char* url) {
    return host_is(url, "gta6gps.com");
}

static bool is_gps_pin(const char* url) {
    return is_gps(url) && (strstr(url, "?p=") || strstr(url, "&p="));
}

static bool is_image(const char* url) {
    const char* end = url + strcspn(url, "?#");
    const char* const extensions[] = {".jpg", ".jpeg", ".png", ".webp", ".gif"};
    for(size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); ++i) {
        size_t n = strlen(extensions[i]);
        if((size_t)(end - url) >= n && !strncmp(end - n, extensions[i], n)) return true;
    }
    return strstr(url, "/_next/image?") != NULL;
}

static bool is_map(const char* url) {
    return is_gps(url) || strstr(url, "/map/") || strstr(url, "/map?") ||
           strstr(url, "/interactive-map") || strstr(url, "/gta-6-map/") ||
           strstr(url, "index-exploration.html?pin=") || host_is(url, "gta6-maps.net") ||
           host_is(url, "gta6mapwiki.com") || host_is(url, "gta6direct.com") ||
           host_is(url, "maps.gamecraves.com");
}

static unsigned priority(const GtaUrlChoice* choice) {
    if(is_gps_pin(choice->url)) return 0;
    if(is_gps(choice->url)) return 1;
    if(choice->kind == GtaUrlMapImage) return 4;
    if(is_map(choice->url)) return choice->map_index ? 3 : 2;
    return choice->map_index ? 6 : 5;
}

void gta_url_links_add(GtaUrlLinks* links, GtaUrlChoice choice) {
    if(!links || !host_start(choice.url) || strlen(choice.url) >= GTA_URL_MAX) return;
    /* Some legacy map-image columns actually contain an interactive GPS pin. */
    if(is_gps(choice.url)) {
        choice.kind = GtaUrlSource;
        choice.map_index = !is_gps_pin(choice.url);
    } else if(is_image(choice.url)) {
        choice.kind = GtaUrlMapImage;
        choice.map_index = false;
    } else if(choice.kind == GtaUrlMapImage) {
        choice.kind = GtaUrlSource2;
    }
    for(size_t i = 0; i < links->count; ++i) {
        if(!strcmp(links->choices[i].url, choice.url)) {
            if(choice.label && *choice.label) links->choices[i].label = choice.label;
            return;
        }
    }
    size_t pos = 0;
    while(pos < links->count && priority(&links->choices[pos]) <= priority(&choice))
        ++pos;
    if(pos == GTA_URL_CHOICES_MAX) return;
    if(links->count < GTA_URL_CHOICES_MAX) ++links->count;
    for(size_t i = links->count - 1; i > pos; --i)
        links->choices[i] = links->choices[i - 1];
    links->choices[pos] = choice;
}

static void add_record(GtaUrlLinks* links, GtaRecord* record, bool maps_only) {
    char** f = record->field;
    if(!f[0]) return;
    const GtaUrlChoice choices[] = {
        {f[1], NULL, GtaUrlSource, *f[4] == '1'},
        {f[2], NULL, GtaUrlSource2, *f[5] == '1'},
        {f[3], NULL, GtaUrlMapImage, false},
    };
    for(size_t i = 0; i < sizeof(choices) / sizeof(choices[0]); ++i) {
        if(!maps_only ||
           (host_start(choices[i].url) && (is_map(choices[i].url) || is_image(choices[i].url))))
            gta_url_links_add(links, choices[i]);
    }
}

GtaUrlLinks* gta_url_links_resolve(uint16_t id, const char* asset, const char* title) {
    gta_url_links_release();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(id < GtaProgressBytes * 8u) {
        char file[24], key[8];
        snprintf(key, sizeof(key), "%u", (unsigned)id);
        snprintf(file, sizeof(file), "links_%02u.tsv", (unsigned)(id / 256u));
        if(gta_record_find(storage, file, key, 6, &url_record))
            add_record(&resolved, &url_record, false);
        /* Up to two optional rows: id and id:2, each with four label/URL pairs. */
        snprintf(file, sizeof(file), "links_extra_%02u.tsv", (unsigned)(id / 256u));
        for(size_t row = 0; row < 2; ++row) {
            if(row) snprintf(key, sizeof(key), "%u:2", (unsigned)id);
            GtaRecord* extra = &extra_records[row];
            if(!gta_record_find(storage, file, key, 9, extra)) break;
            for(size_t i = 1; i < 9; i += 2) {
                const char* url = extra->field[i + 1];
                const char* label = extra->field[i];
                gta_url_links_add(
                    &resolved, (GtaUrlChoice){url, *label ? label : NULL, GtaUrlSource2, false});
            }
            if(!*extra->field[8]) break;
        }
    }
    /* Keep existing one-link behavior when possible. If the item has no GPS
     * reference, expose the shared GTA6GPS map ahead of third-party links. */
    bool has_gps = resolved.count && is_gps(resolved.choices[0].url);
    if(!has_gps) {
        const char* key = asset ? asset : title;
        bool found = key &&
                     gta_record_find(storage, "link_categories.tsv", key, 6, &category_record);
        if(!found && asset && title && strcmp(asset, title))
            found = gta_record_find(storage, "link_categories.tsv", title, 6, &category_record);
        if(found) add_record(&resolved, &category_record, resolved.count != 0);
    }
    furi_record_close(RECORD_STORAGE);
    return &resolved;
}

static void append_text(char* out, size_t size, const char* text, size_t length) {
    if(!size) return;
    size_t used = strlen(out);
    if(used >= size - 1u) return;
    size_t available = size - used - 1u;
    if(length > available) length = available;
    memcpy(out + used, text, length);
    out[used + length] = '\0';
}

static void provider_name(const char* url, char* out, size_t size) {
    static const struct {
        const char* domain;
        const char* name;
    } providers[] = {
        {"gta6gps.com", "GTA6GPS"},
        {"gtadb.net", "GTADB"},
        {"gta6map.io", "GTA6Map.io"},
        {"gta-walkthrough.com", "GTA Walkthrough"},
        {"gtabase.com", "GTABase"},
        {"game-checklists.com", "Game Checklists"},
        {"gta6-life.com", "GTA6 Life"},
        {"pcgamer.com", "PC Gamer"},
        {"cdn.mos.cms.futurecdn.net", "Future CDN"},
        {"powerpyx.com", "PowerPyx"},
        {"thegameswiki.com", "The Games Wiki"},
        {"gta6wiki.eu", "GTA6Wiki"},
        {"nexzyapp.com", "Nexzy"},
        {"metaforge.app", "MetaForge"},
        {"pushsquare.com", "Push Square"},
        {"destructoid.com", "Destructoid"},
        {"linkleung310.github.io", "Leonida Atlas"},
        {"gta6direct.com", "GTA6Direct"},
        {"maps.gamecraves.com", "Game Craves"},
    };
    for(size_t i = 0; i < sizeof(providers) / sizeof(providers[0]); ++i) {
        if(host_is(url, providers[i].domain)) {
            snprintf(out, size, "%s", providers[i].name);
            return;
        }
    }
    const char* host = host_start(url);
    if(!host) {
        snprintf(out, size, "Web");
        return;
    }
    if(!strncmp(host, "www.", 4)) host += 4;
    if(!size) return;
    *out = '\0';
    append_text(out, size, host, strcspn(host, "/?#"));
}

static void base_label(const GtaUrlChoice* choice, char* out, size_t size) {
    char provider[32];
    provider_name(choice->url, provider, sizeof(provider));
    if(!size) return;
    *out = '\0';
    append_text(out, size, provider, strlen(provider));
    append_text(out, size, ": ", 2);
    if(choice->label && *choice->label) {
        append_text(out, size, choice->label, strlen(choice->label));
        return;
    }
    const char* type;
    if(is_gps_pin(choice->url)) {
        const char* pin = strstr(choice->url, "?p=pin-");
        if(pin) {
            append_text(out, size, "Pin ", 4);
            append_text(out, size, pin + 7, strcspn(pin + 7, "&#"));
            return;
        }
        const char* named = strstr(choice->url, "?p=");
        if(named) {
            append_text(out, size, named + 3, strcspn(named + 3, "&#"));
            return;
        }
        type = "Location pin";
    } else if(is_gps(choice->url)) {
        type = "Category map";
    } else if(choice->kind == GtaUrlMapImage) {
        type = "Map image";
    } else if(strstr(choice->url, "#:~:text=")) {
        type = "Item reference";
    } else if(is_map(choice->url)) {
        type = choice->map_index ? "Shared map" : "Location map";
    } else {
        type = choice->map_index ? "Shared guide" : "Item guide";
    }
    append_text(out, size, type, strlen(type));
}

void gta_url_choice_label(const GtaUrlLinks* links, size_t index, char* out, size_t size) {
    if(!size) return;
    *out = '\0';
    if(!links || index >= links->count) return;
    char label[GTA_URL_LABEL_MAX], other[GTA_URL_LABEL_MAX];
    base_label(&links->choices[index], label, sizeof(label));
    unsigned ordinal = 0, total = 0;
    for(size_t i = 0; i < links->count; ++i) {
        base_label(&links->choices[i], other, sizeof(other));
        if(strcmp(label, other)) continue;
        ++total;
        if(i <= index) ++ordinal;
    }
    if(total > 1) {
        char suffix[16];
        snprintf(suffix, sizeof(suffix), " (%u)", ordinal);
        size_t suffix_len = strlen(suffix);
        size_t limit = size > suffix_len ? size - suffix_len : size;
        append_text(out, limit, label, strlen(label));
        append_text(out, size, suffix, suffix_len);
    } else {
        append_text(out, size, label, strlen(label));
    }
}

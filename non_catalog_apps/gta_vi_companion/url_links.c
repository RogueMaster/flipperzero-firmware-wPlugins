#include "url_links.h"
#include <stddef.h>
GtaUrlChoice gta_url_links_choose(const GtaUrlLinks* links, bool right) {
    if(!links) return (GtaUrlChoice){0};
    if(!right) {
        if(links->source)
            return (GtaUrlChoice){links->source, "Source", GtaUrlSource, links->source_index};
    } else if(links->map_image) {
        return (GtaUrlChoice){links->map_image, "Map", GtaUrlMapImage, false};
    } else if(links->source2) {
        return (GtaUrlChoice){links->source2, "Source 2", GtaUrlSource2, links->source2_index};
    }
    return (GtaUrlChoice){0};
}

#include "gta_catalog_io.h"
#include <furi.h>
#include <stdio.h>
#include <string.h>
static GtaRecord url_record;
static GtaUrlLinks resolved;
void gta_url_links_release(void) {
    gta_record_clear(&url_record);
}
static const GtaUrlLinks* lookup(const char* asset, uint16_t id, const char* key) {
    gta_url_links_release();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    char numeric_key[6];
    if(!key) {
        snprintf(numeric_key, sizeof(numeric_key), "%u", (unsigned)id);
        key = numeric_key;
    }
    const GtaUrlLinks* result = NULL;
    if(gta_record_find(storage, asset, key, 6, &url_record)) {
        char** f = url_record.field;
        resolved = (GtaUrlLinks){
            *f[1] ? f[1] : NULL,
            *f[2] ? f[2] : NULL,
            *f[3] ? f[3] : NULL,
            *f[4] == '1',
            *f[5] == '1'};
        result = &resolved;
    }
    furi_record_close(RECORD_STORAGE);
    if(!result) gta_url_links_release();
    return result;
}
const GtaUrlLinks* gta_url_links_find(uint16_t id) {
    return lookup("links.tsv", id, NULL);
}
const GtaUrlLinks* gta_url_links_resolve(uint16_t id, const char* asset, const char* title) {
    const GtaUrlLinks* specific = gta_url_links_find(id);
    if(specific) return specific;
    const char* key = asset ? asset : title;
    return key ? lookup("link_categories.tsv", 0, key) : NULL;
}

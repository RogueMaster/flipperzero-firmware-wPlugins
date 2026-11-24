#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    GtaUrlSource = 0,
    GtaUrlMapImage = 1,
    GtaUrlSource2 = 2,
} GtaUrlKind;

typedef struct {
    const char* source;
    const char* source2;
    const char* map_image; // Direct image URL only, not an interactive map or guide page.
    bool source_index;
    bool source2_index;
} GtaUrlLinks;

typedef struct {
    const char* url;
    const char* label;
    GtaUrlKind kind;
    bool map_index;
} GtaUrlChoice;

const GtaUrlLinks* gta_url_links_find(uint16_t id);
const GtaUrlLinks* gta_url_links_resolve(uint16_t id, const char* asset, const char* title);
GtaUrlChoice gta_url_links_choose(const GtaUrlLinks* links, bool right);

#include "carto/style.h"
#include <string.h>

void carto_style_default(carto_style *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));

    const carto_rgb water     = {95, 105, 120};
    const carto_rgb park      = {60, 80, 60};
    const carto_rgb building  = {75, 75, 80};
    const carto_rgb white     = {255, 255, 255};
    const carto_rgb black     = {0, 0, 0};

    out->bg         = (carto_rgb){15, 15, 20};
    out->water      = water;
    out->park       = park;
    out->building   = building;
    out->road_color = white;

    static const uint8_t widths[CARTO_ROAD_PRIO_MAX + 1] =
        { 0, 4, 4, 4, 4, 5, 5, 6, 7, 8, 9 };
    memcpy(out->road_width, widths, sizeof(widths));

    static const carto_rgb ramp[CARTO_ROAD_PRIO_MAX + 1] = {
        {0, 0, 0},
        {110, 110, 110},
        {120, 120, 120},
        {140, 140, 140},
        {160, 160, 160},
        {175, 175, 175},
        {195, 195, 195},
        {210, 210, 210},
        {230, 230, 230},
        {245, 245, 245},
        {255, 255, 255},
    };
    memcpy(out->road_color_by_prio, ramp, sizeof(ramp));

    out->label_color = white;
    out->halo_color  = black;

    out->aircraft_color           = (carto_rgb){255, 200, 60};
    out->aircraft_selected_color  = white;
    out->aircraft_emergency_color = (carto_rgb){255, 80, 80};
    out->aircraft_label_color     = (carto_rgb){255, 220, 120};
    out->aircraft_halo_color      = black;

    out->draw_labels = false;
}

int carto_road_priority(const char *cls) {
    if (!cls) return 1;
    static const struct { const char *name; int prio; } table[] = {
        {"highway", 10}, {"motorway", 10}, {"trunk", 9}, {"primary", 8},
        {"secondary", 7}, {"tertiary", 6}, {"minor_road", 5},
        {"residential", 4}, {"street", 4}, {"service", 3},
        {"path", 2}, {"footway", 2}, {"cycleway", 2}, {"track", 2},
        {"other", 1},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
        if (strcmp(cls, table[i].name) == 0) return table[i].prio;
    }
    return 1;
}

carto_layer_kind carto_classify_layer(const char *layer_name) {
    if (!layer_name) return CARTO_LAYER_COUNT;
    static const struct { const char *name; carto_layer_kind kind; } table[] = {
        {"water",           CARTO_LAYER_WATER},
        {"ocean",           CARTO_LAYER_WATER},
        {"rivers",          CARTO_LAYER_WATER},
        {"lakes",           CARTO_LAYER_WATER},
        {"water_polygons",  CARTO_LAYER_WATER},
        {"water_lines",     CARTO_LAYER_WATER},
        {"waterway",        CARTO_LAYER_WATER},
        {"landuse",         CARTO_LAYER_LANDUSE},
        {"landcover",       CARTO_LAYER_LANDUSE},
        {"natural",         CARTO_LAYER_LANDUSE},
        {"land",            CARTO_LAYER_LANDUSE},
        {"sites",           CARTO_LAYER_LANDUSE},
        {"buildings",       CARTO_LAYER_BUILDING},
        {"building",        CARTO_LAYER_BUILDING},
        {"roads",           CARTO_LAYER_ROAD},
        {"transportation",  CARTO_LAYER_ROAD},
        {"streets",         CARTO_LAYER_ROAD},
        {"street_polygons", CARTO_LAYER_ROAD},
        {"bridges",         CARTO_LAYER_ROAD},
        {"places",          CARTO_LAYER_PLACE},
        {"place",           CARTO_LAYER_PLACE},
        {"place_labels",    CARTO_LAYER_PLACE},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
        if (strcmp(layer_name, table[i].name) == 0) return table[i].kind;
    }
    return CARTO_LAYER_COUNT;
}

#include "gta_handoff.h"
#include <furi.h>
#include <furi_hal_random.h>
#include <assets_icons.h>
#include <flipper_application/flipper_application.h>
#include <gui/gui.h>
#include <gui/icon_animation.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>

/* Use the Hub's existing extracted mock-loader assets, never a second asset pack. */
static const char* const dnd[] = {
    "gta",
    "gta_v2",
    "original",
    "rogue",
    "wizard",
    "ranger",
    "paladin",
    "cleric",
    "bard",
    "artificer",
    "druid",
    "monk",
    "sorcerer",
    "warlock",
    "barbarian",
};
static const char* const gta[] = {
    "lazer_vice_city",        "speeder_waterfront",
    "carbon_rs_ocean_drive",  "maverick_vice_city",
    "rhino_air_station",      "banshee_art_deco",
    "gauntlet_ambrosia",      "airgator_grassrivers",
    "ocean_drive_after_dark", "vice_beach_lifeguard",
    "leonida_keys_causeway",  "grassrivers_gator",
    "port_gellhorn_cranes",   "vice_city_cafecito",
    "downtown_vice_elevated", "rearview_pursuit",
    "flamingo_car_wash",      "lowrider_meet",
    "midnight_drawbridge",    "jet_ski_cutback",
    "scrapyard_magnet",       "seaplane_landing",
    "vice_boardwalk",         "drive_in_double_feature",
    "hurricane_watch",        "mangrove_tunnel",
    "overpass_basketball",    "mud_run",
    "leonida_fire_tower",     "rooftop_dj",
};
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static bool read_exact(Storage* s, const char* path, void* out, size_t size) {
    File* f = storage_file_alloc(s);
    if(!f) return false;
    bool ok = storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
              storage_file_size(f) == size && storage_file_read(f, out, size) == size;
    storage_file_free(f);
    return ok;
}

static uint8_t enabled_sets(Storage* s) {
    const char* paths[] = {
        EXT_PATH("apps_data/gta_vi_cheats/mock_loaders.a"),
        EXT_PATH("apps_data/gta_vi_cheats/mock_loaders.b")};
    uint8_t mask = 2, generation = 0;
    bool found = false;
    for(size_t i = 0; i < COUNT(paths); ++i) {
        uint8_t record[4];
        if(!read_exact(s, paths[i], record, sizeof(record)) || record[0] != 0xA3 ||
           record[2] > 3 || record[3] != (uint8_t)(record[0] ^ record[1] ^ record[2] ^ 0xFFu))
            continue;
        uint8_t age = (uint8_t)(record[1] - generation);
        if(!found || (age && age < 128)) {
            found = true;
            generation = record[1];
            mask = record[2];
        }
    }
    return mask;
}

typedef struct {
    Gui* gui;
    Canvas* canvas;
    FuriMutex* mutex;
    IconAnimation* animation;
    uint8_t* bitmap;
    uint32_t started;
    bool active;
} GtaLoading;

static void paint(GtaLoading* l) {
    canvas_clear(l->canvas);
    canvas_set_color(l->canvas, ColorBlack);
    if(l->bitmap)
        canvas_draw_xbm(l->canvas, 0, 0, 128, 64, l->bitmap);
    else {
        canvas_set_font(l->canvas, FontSecondary);
        canvas_draw_str(l->canvas, 24, 32, "Loading...");
    }
    canvas_set_color(l->canvas, ColorWhite);
    canvas_draw_box(l->canvas, 100, 36, 28, 28);
    canvas_set_color(l->canvas, ColorBlack);
    canvas_draw_frame(l->canvas, 100, 36, 28, 28);
    canvas_draw_icon_animation(l->canvas, 102, 38, l->animation);
    canvas_commit(l->canvas);
}
static void release(GtaLoading* l) {
    if(l->active) {
        l->active = false;
        gui_direct_draw_release(l->gui);
        l->canvas = NULL;
    }
}
static void update(IconAnimation* animation, void* ctx) {
    UNUSED(animation);
    GtaLoading* l = ctx;
    furi_check(furi_mutex_acquire(l->mutex, FuriWaitForever) == FuriStatusOk);
    if(l->active) {
        if(furi_get_tick() - l->started >= furi_ms_to_ticks(10000))
            release(l);
        else
            paint(l);
    }
    furi_check(furi_mutex_release(l->mutex) == FuriStatusOk);
}
static void end(void* ctx) {
    GtaLoading* l = ctx;
    if(l->animation) {
        icon_animation_stop(l->animation);
        icon_animation_free(l->animation);
    }
    if(l->mutex) {
        furi_check(furi_mutex_acquire(l->mutex, FuriWaitForever) == FuriStatusOk);
        release(l);
        furi_check(furi_mutex_release(l->mutex) == FuriStatusOk);
        furi_mutex_free(l->mutex);
    }
    if(l->gui) furi_record_close(RECORD_GUI);
    free(l->bitmap);
    free(l);
}
static void* begin(void) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    uint8_t mask = enabled_sets(s);
    if(!mask) {
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }
    GtaLoading* l = calloc(1, sizeof(*l));
    if(!l) {
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }
    uint32_t n = ((mask & 1) ? COUNT(dnd) : 0) + ((mask & 2) ? COUNT(gta) : 0);
    uint32_t random, limit = UINT32_MAX - UINT32_MAX % n;
    do {
        random = furi_hal_random_get();
    } while(random >= limit);
    size_t index = random % n;
    bool is_dnd = (mask & 1) && index < COUNT(dnd);
    if(!is_dnd && (mask & 1)) index -= COUNT(dnd);
    char path[128];
    snprintf(
        path,
        sizeof(path),
        is_dnd ? APP_ASSETS_PATH("mock_loaders/%s.bin") :
                 APP_ASSETS_PATH("mock_loaders/gta_vi/%s_128x64.bin"),
        is_dnd ? dnd[index] : gta[index]);
    l->bitmap = malloc(1024);
    if(l->bitmap && !read_exact(s, path, l->bitmap, 1024)) {
        free(l->bitmap);
        l->bitmap = NULL;
    }
    furi_record_close(RECORD_STORAGE);
    l->gui = furi_record_open(RECORD_GUI);
    l->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    l->animation = icon_animation_alloc(&A_Loading_24);
    if(!l->gui || !l->mutex || !l->animation) goto fail;
    icon_animation_set_update_callback(l->animation, update, l);
    furi_check(furi_mutex_acquire(l->mutex, FuriWaitForever) == FuriStatusOk);
    l->canvas = gui_direct_draw_acquire(l->gui);
    l->active = true;
    l->started = furi_get_tick();
    paint(l);
    furi_check(furi_mutex_release(l->mutex) == FuriStatusOk);
    icon_animation_start(l->animation);
    return l;
fail:
    end(l);
    return NULL;
}
static const GtaLoadingApi api = {sizeof(GtaLoadingApi), begin, end};
static const FlipperAppPluginDescriptor descriptor = {
    GTA_LOADING_ID,
    GTA_LOADING_API_VERSION,
    &api};
const FlipperAppPluginDescriptor* gta_loading_ep(void) {
    return &descriptor;
}

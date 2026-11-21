#include "mock_loaders.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <stdio.h>
#include <stdlib.h>
#include <storage/storage.h>

static const char* const gta_dnd_mock_loader_files[] = {
    "gta.bin",
    "gta_v2.bin",
    "original.bin",
    "rogue.bin",
    "wizard.bin",
    "ranger.bin",
    "paladin.bin",
    "cleric.bin",
    "bard.bin",
    "artificer.bin",
    "druid.bin",
    "monk.bin",
    "sorcerer.bin",
    "warlock.bin",
    "barbarian.bin",
};

static const char* const gta_vi_mock_loader_files[] = {
    "lazer_vice_city_128x64.bin",        "speeder_waterfront_128x64.bin",
    "carbon_rs_ocean_drive_128x64.bin",  "maverick_vice_city_128x64.bin",
    "rhino_air_station_128x64.bin",      "banshee_art_deco_128x64.bin",
    "gauntlet_ambrosia_128x64.bin",      "airgator_grassrivers_128x64.bin",
    "ocean_drive_after_dark_128x64.bin", "vice_beach_lifeguard_128x64.bin",
    "leonida_keys_causeway_128x64.bin",  "grassrivers_gator_128x64.bin",
    "port_gellhorn_cranes_128x64.bin",   "vice_city_cafecito_128x64.bin",
    "downtown_vice_elevated_128x64.bin", "rearview_pursuit_128x64.bin",
    "flamingo_car_wash_128x64.bin",      "lowrider_meet_128x64.bin",
    "midnight_drawbridge_128x64.bin",    "jet_ski_cutback_128x64.bin",
    "scrapyard_magnet_128x64.bin",       "seaplane_landing_128x64.bin",
    "vice_boardwalk_128x64.bin",         "drive_in_double_feature_128x64.bin",
    "hurricane_watch_128x64.bin",        "mangrove_tunnel_128x64.bin",
    "overpass_basketball_128x64.bin",    "mud_run_128x64.bin",
    "leonida_fire_tower_128x64.bin",     "rooftop_dj_128x64.bin",
};

typedef enum {
    GtaMockLoaderDnd,
    GtaMockLoaderGtaVi,
    GtaMockLoaderSetCount,
} GtaMockLoaderSetId;

typedef struct {
    const char* name;
    const char* directory;
    const char* const* files;
    size_t count;
} GtaMockLoaderSet;

static const GtaMockLoaderSet gta_mock_loader_sets[GtaMockLoaderSetCount] = {
    [GtaMockLoaderDnd] =
        {
            "Dungeons & Dolphins",
            "mock_loaders",
            gta_dnd_mock_loader_files,
            sizeof(gta_dnd_mock_loader_files) / sizeof(gta_dnd_mock_loader_files[0]),
        },
    [GtaMockLoaderGtaVi] =
        {
            "GTA VI",
            "mock_loaders/gta_vi",
            gta_vi_mock_loader_files,
            sizeof(gta_vi_mock_loader_files) / sizeof(gta_vi_mock_loader_files[0]),
        },
};

enum {
    GtaMockLoaderWidth = 128,
    GtaMockLoaderHeight = 64,
    GtaMockLoaderBytes = GtaMockLoaderWidth * GtaMockLoaderHeight / 8,
    GtaMockLoaderAllSets = (1u << GtaMockLoaderSetCount) - 1u,
    GtaMockLoaderSaveMagic = 0xA3,
    GtaMockLoaderSaveSlots = 2,
};

#define GTA_MOCK_LOADER_DATA_DIRECTORY EXT_PATH("apps_data/gta_vi_cheats")

static const char* const gta_mock_loader_save_paths[GtaMockLoaderSaveSlots] = {
    GTA_MOCK_LOADER_DATA_DIRECTORY "/mock_loaders.a",
    GTA_MOCK_LOADER_DATA_DIRECTORY "/mock_loaders.b",
};

typedef struct {
    uint8_t magic;
    uint8_t generation;
    uint8_t enabled_sets;
    uint8_t checksum;
} GtaMockLoaderSave;

typedef struct {
    uint8_t* bitmap;
    GtaMockLoaderSetId set;
    size_t selected;
    bool has_enabled_sets;
    bool available;
} GtaMockLoaderModel;

typedef struct {
    GtaMockLoaderSetId selected;
    uint8_t enabled_sets;
    bool save_error;
} GtaMockLoaderSettingsModel;

struct GtaMockLoaders {
    ViewDispatcher* dispatcher;
    View* viewer;
    View* settings;
    Storage* storage;
    uint32_t viewer_id;
    uint32_t settings_id;
    GtaMockLoaderSave save;
    size_t save_slot;
    bool showing_startup;
};

static uint8_t gta_mock_loader_save_checksum(const GtaMockLoaderSave* save) {
    return (uint8_t)(save->magic ^ save->generation ^ save->enabled_sets ^ 0xFFu);
}

static void gta_mock_loader_load_settings(GtaMockLoaders* loaders) {
    loaders->save.magic = GtaMockLoaderSaveMagic;
    loaders->save.enabled_sets = 1u << GtaMockLoaderGtaVi;
    loaders->save_slot = GtaMockLoaderSaveSlots;

    for(size_t slot = 0; slot < GtaMockLoaderSaveSlots; ++slot) {
        GtaMockLoaderSave candidate;
        File* file = storage_file_alloc(loaders->storage);
        if(!file) continue;
        const bool valid =
            storage_file_open(
                file, gta_mock_loader_save_paths[slot], FSAM_READ, FSOM_OPEN_EXISTING) &&
            storage_file_size(file) == sizeof(candidate) &&
            storage_file_read(file, &candidate, sizeof(candidate)) == sizeof(candidate) &&
            candidate.magic == GtaMockLoaderSaveMagic &&
            candidate.enabled_sets <= GtaMockLoaderAllSets &&
            candidate.checksum == gta_mock_loader_save_checksum(&candidate);
        storage_file_free(file);
        if(!valid) continue;

        const uint8_t age = (uint8_t)(candidate.generation - loaders->save.generation);
        if(loaders->save_slot == GtaMockLoaderSaveSlots || (age && age < 128u)) {
            loaders->save = candidate;
            loaders->save_slot = slot;
        }
    }
}

static bool gta_mock_loader_toggle_set(GtaMockLoaders* loaders, GtaMockLoaderSetId set) {
    Storage* storage = loaders->storage;
    if(!storage_simply_mkdir(storage, EXT_PATH("apps_data")) ||
       !storage_simply_mkdir(storage, GTA_MOCK_LOADER_DATA_DIRECTORY)) {
        return false;
    }

    GtaMockLoaderSave next = loaders->save;
    next.enabled_sets ^= (uint8_t)(1u << set);
    ++next.generation;
    next.checksum = gta_mock_loader_save_checksum(&next);

    // Keep the last complete settings available while writing the next slot.
    const size_t slot = loaders->save_slot == 0 ? 1 : 0;
    const char* path = gta_mock_loader_save_paths[slot];
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        return false;
    }

    const bool written = storage_file_write(file, &next, sizeof(next)) == sizeof(next) &&
                         storage_file_sync(file);
    const bool closed = storage_file_close(file);
    storage_file_free(file);
    if(!written || !closed) {
        storage_simply_remove(storage, path);
        return false;
    }

    loaders->save = next;
    loaders->save_slot = slot;
    return true;
}

static void gta_mock_loader_read(
    GtaMockLoaders* loaders,
    GtaMockLoaderModel* model,
    GtaMockLoaderSetId set_id,
    size_t index) {
    model->available = false;
    if(!model->bitmap) model->bitmap = malloc(GtaMockLoaderBytes);
    if(!model->bitmap) return;

    const GtaMockLoaderSet* set = &gta_mock_loader_sets[set_id];
    char path[128];
    snprintf(path, sizeof(path), APP_ASSETS_PATH("%s/%s"), set->directory, set->files[index]);

    File* file = storage_file_alloc(loaders->storage);
    if(!file) return;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        model->available = storage_file_size(file) == GtaMockLoaderBytes &&
                           storage_file_read(file, model->bitmap, GtaMockLoaderBytes) ==
                               GtaMockLoaderBytes;
        storage_file_close(file);
    }
    storage_file_free(file);
}

static void gta_mock_loader_draw(Canvas* canvas, void* context) {
    const GtaMockLoaderModel* model = context;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(model->available) {
        canvas_draw_xbm(canvas, 0, 0, GtaMockLoaderWidth, GtaMockLoaderHeight, model->bitmap);
    } else {
        canvas_set_font(canvas, FontSecondary);
        if(model->has_enabled_sets) {
            canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Image unavailable");
        } else {
            canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "No sets enabled");
            canvas_draw_str_aligned(
                canvas, 64, 39, AlignCenter, AlignCenter, "Use Mock Loader Settings");
        }
    }
}

static void gta_mock_loader_move(GtaMockLoaderModel* model, uint8_t enabled_sets, bool forward) {
    const size_t count = gta_mock_loader_sets[model->set].count;
    if(forward && model->selected + 1u < count) {
        ++model->selected;
    } else if(!forward && model->selected > 0) {
        --model->selected;
    } else {
        const GtaMockLoaderSetId other = model->set == GtaMockLoaderDnd ? GtaMockLoaderGtaVi :
                                                                          GtaMockLoaderDnd;
        if(enabled_sets & (1u << other)) model->set = other;
        model->selected = forward ? 0 : gta_mock_loader_sets[model->set].count - 1u;
    }
}

static bool gta_mock_loader_input(InputEvent* event, void* context) {
    GtaMockLoaders* loaders = context;
    if(loaders->showing_startup) return true;

    if(event->type != InputTypeShort ||
       (event->key != InputKeyLeft && event->key != InputKeyRight)) {
        return false;
    }

    const uint8_t enabled_sets = loaders->save.enabled_sets;
    if(enabled_sets) {
        with_view_model(
            loaders->viewer,
            GtaMockLoaderModel * model,
            {
                gta_mock_loader_move(model, enabled_sets, event->key == InputKeyRight);
                gta_mock_loader_read(loaders, model, model->set, model->selected);
            },
            true);
    }
    return true;
}

static void gta_mock_loader_enter(void* context) {
    GtaMockLoaders* loaders = context;
    if(loaders->showing_startup) return;
    const uint8_t enabled_sets = loaders->save.enabled_sets;
    with_view_model(
        loaders->viewer,
        GtaMockLoaderModel * model,
        {
            model->has_enabled_sets = enabled_sets != 0;
            if(enabled_sets) {
                if(!(enabled_sets & (1u << model->set))) {
                    model->set = (enabled_sets & (1u << GtaMockLoaderDnd)) ? GtaMockLoaderDnd :
                                                                             GtaMockLoaderGtaVi;
                    model->selected = 0;
                }
                gta_mock_loader_read(loaders, model, model->set, model->selected);
            }
        },
        true);
}

static void gta_mock_loader_exit(void* context) {
    GtaMockLoaders* loaders = context;
    loaders->showing_startup = false;
    with_view_model(
        loaders->viewer,
        GtaMockLoaderModel * model,
        {
            free(model->bitmap);
            model->bitmap = NULL;
            model->available = false;
        },
        false);
}

bool gta_mock_loaders_show_startup(GtaMockLoaders* loaders) {
    const uint8_t enabled_sets = loaders->save.enabled_sets;
    uint32_t count = 0;
    for(size_t set = 0; set < GtaMockLoaderSetCount; ++set) {
        if(enabled_sets & (1u << set)) count += gta_mock_loader_sets[set].count;
    }
    if(!count) return false;

    const uint32_t limit = UINT32_MAX - UINT32_MAX % count;
    uint32_t random;
    do {
        random = furi_hal_random_get();
    } while(random >= limit);

    size_t index = random % count;
    for(size_t set = 0; set < GtaMockLoaderSetCount; ++set) {
        if(!(enabled_sets & (1u << set))) continue;
        const size_t set_count = gta_mock_loader_sets[set].count;
        if(index >= set_count) {
            index -= set_count;
            continue;
        }

        bool available = false;
        with_view_model(
            loaders->viewer,
            GtaMockLoaderModel * model,
            {
                gta_mock_loader_read(loaders, model, (GtaMockLoaderSetId)set, index);
                available = model->available;
            },
            false);
        if(!available) {
            gta_mock_loader_exit(loaders);
            return false;
        }

        loaders->showing_startup = true;
        view_dispatcher_switch_to_view(loaders->dispatcher, loaders->viewer_id);
        return true;
    }
    return false;
}

static void gta_mock_loader_settings_draw(Canvas* canvas, void* context) {
    const GtaMockLoaderSettingsModel* model = context;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Mock Loader Settings");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    for(size_t set = 0; set < GtaMockLoaderSetCount; ++set) {
        const int32_t y = 25 + 13 * set;
        if(set == model->selected) {
            canvas_draw_box(canvas, 0, y - 8, 128, 11);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_frame(canvas, 3, y - 6, 7, 7);
        if(model->enabled_sets & (1u << set)) {
            canvas_draw_line(canvas, 4, y - 3, 6, y - 1);
            canvas_draw_line(canvas, 6, y - 1, 8, y - 5);
        }

        const GtaMockLoaderSet* item = &gta_mock_loader_sets[set];
        char count[12];
        snprintf(count, sizeof(count), "%u", (unsigned)item->count);
        canvas_draw_str(canvas, 14, y, item->name);
        canvas_draw_str(canvas, 125 - canvas_string_width(canvas, count), y, count);
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_draw_line(canvas, 0, 51, 127, 51);
    canvas_draw_str(canvas, 2, 61, model->save_error ? "SD save failed" : "OK Toggle   Back");
}

static bool gta_mock_loader_settings_input(InputEvent* event, void* context) {
    if(event->type != InputTypeShort ||
       (event->key != InputKeyUp && event->key != InputKeyDown && event->key != InputKeyOk)) {
        return false;
    }

    GtaMockLoaders* loaders = context;
    with_view_model(
        loaders->settings,
        GtaMockLoaderSettingsModel * model,
        {
            if(event->key == InputKeyOk) {
                model->save_error = !gta_mock_loader_toggle_set(loaders, model->selected);
                model->enabled_sets = loaders->save.enabled_sets;
            } else {
                model->selected = model->selected == GtaMockLoaderDnd ? GtaMockLoaderGtaVi :
                                                                        GtaMockLoaderDnd;
            }
        },
        true);
    return true;
}

GtaMockLoaders*
    gta_mock_loaders_alloc(ViewDispatcher* dispatcher, uint32_t viewer_id, uint32_t settings_id) {
    GtaMockLoaders* loaders = calloc(1, sizeof(GtaMockLoaders));
    furi_check(loaders);
    loaders->dispatcher = dispatcher;
    loaders->viewer_id = viewer_id;
    loaders->settings_id = settings_id;
    loaders->storage = furi_record_open(RECORD_STORAGE);
    gta_mock_loader_load_settings(loaders);

    loaders->viewer = view_alloc();
    view_allocate_model(loaders->viewer, ViewModelTypeLocking, sizeof(GtaMockLoaderModel));
    with_view_model(
        loaders->viewer,
        GtaMockLoaderModel * model,
        { *model = (GtaMockLoaderModel){.set = GtaMockLoaderGtaVi}; },
        false);
    view_set_context(loaders->viewer, loaders);
    view_set_draw_callback(loaders->viewer, gta_mock_loader_draw);
    view_set_input_callback(loaders->viewer, gta_mock_loader_input);
    view_set_enter_callback(loaders->viewer, gta_mock_loader_enter);
    view_set_exit_callback(loaders->viewer, gta_mock_loader_exit);
    view_dispatcher_add_view(dispatcher, viewer_id, loaders->viewer);

    loaders->settings = view_alloc();
    view_allocate_model(
        loaders->settings, ViewModelTypeLocking, sizeof(GtaMockLoaderSettingsModel));
    with_view_model(
        loaders->settings,
        GtaMockLoaderSettingsModel * model,
        {
            model->selected = GtaMockLoaderDnd;
            model->enabled_sets = loaders->save.enabled_sets;
            model->save_error = false;
        },
        false);
    view_set_context(loaders->settings, loaders);
    view_set_draw_callback(loaders->settings, gta_mock_loader_settings_draw);
    view_set_input_callback(loaders->settings, gta_mock_loader_settings_input);
    view_dispatcher_add_view(dispatcher, settings_id, loaders->settings);
    return loaders;
}

void gta_mock_loaders_free(GtaMockLoaders* loaders) {
    view_dispatcher_remove_view(loaders->dispatcher, loaders->settings_id);
    view_dispatcher_remove_view(loaders->dispatcher, loaders->viewer_id);
    view_free(loaders->settings);
    view_free(loaders->viewer);
    furi_record_close(RECORD_STORAGE);
    free(loaders);
}

#include "companion.h"

#include <furi.h>
#include <gui/canvas.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    GtaSaveMagic = 0x33415447, // GTA3: extended 4096-bit save
    GtaLegacySaveMagic = 0x32415447, // GTA2: original 2048-bit save
    GtaLegacyProgressBytes = 256,
    GtaSaveSlots = 2,
    GtaListRows = 4,
    GtaDetailToggle = 0x400,
    GtaDispatchNext,
    GtaExploreMaxAssetSize = 16 * 1024,
    GtaExploreMaxRows = 128,
    GtaAholFirstTypeId = 1376,
};

#define GTA_DATA_DIRECTORY EXT_PATH("apps_data/gta_vi_cheats")

// Use separate paths so the older 2048-bit saves remain available for recovery.
static const char* const gta_save_paths[GtaSaveSlots] = {
    GTA_DATA_DIRECTORY "/progress2.a",
    GTA_DATA_DIRECTORY "/progress2.b",
};
static const char* const gta_legacy_save_paths[GtaSaveSlots] = {
    GTA_DATA_DIRECTORY "/progress.a",
    GTA_DATA_DIRECTORY "/progress.b",
};

// Ranges use the saved IDs from ahol.tsv and ahol_pieces.tsv in type order.
static const struct {
    uint16_t first;
    uint8_t count;
} gta_ahol_ranges[] = {
    {1920, 10}, // Mattresses
    {1930, 8}, // Tires
    {1938, 9}, // Wheels
    {1947, 9}, // Fridges
    {1956, 7}, // Armchairs
    {1963, 8}, // TVs
    {1971, 15}, // Containers
    {1986, 13}, // Cones
    {1999, 11}, // Buckets
    {2010, 13}, // Pallets
    {2023, 9}, // Chairs
};

#define GTA_AHOL_TYPE_COUNT (sizeof(gta_ahol_ranges) / sizeof(gta_ahol_ranges[0]))

typedef struct {
    uint32_t magic;
    uint32_t generation;
    uint8_t checked[GtaProgressBytes];
    uint32_t checksum;
} GtaSave;

// The original on-disk structure is intentionally retained for read-only migration.
typedef struct {
    uint32_t magic;
    uint32_t generation;
    uint8_t checked[GtaLegacyProgressBytes];
    uint32_t checksum;
} GtaLegacySave;
_Static_assert(sizeof(GtaLegacySave) == 268, "Legacy GTA2 save layout changed");
_Static_assert(sizeof(GtaSave) == 524, "GTA3 progress save layout changed");

typedef struct {
    const GtaCollection* collection;
    size_t selected;
    size_t checked_count;
    uint8_t checked[GtaProgressBytes];
    bool save_error;
    const char* list_status;
} GtaListModel;

struct GtaCompanion {
    ViewDispatcher* dispatcher;
    View* list_view;
    Widget* detail;
    FuriString* text;
    Storage* storage;
    uint32_t list_view_id;
    uint32_t detail_view_id;
    GtaSave save;
    size_t save_slot;
    size_t selections[GtaCollectionCount];
    const GtaCollection* list;
    size_t* selection;
    const GtaExploreSection* explore_section;
    GtaCollection explore_list;
    size_t explore_selection;
    char* explore_data;
    GtaCollectionEntry* explore_rows;
    const char* list_status;
    const GtaCollectionEntry* dispatch_entry;
    const GtaCollection* dispatch_collection;
    size_t dispatch_cursor;
    GtaDispatchRegion dispatch_region;
    bool dispatch_unavailable;
    bool detail_visible;
    bool dispatch;
    bool save_error;
    bool progress_load_failed;
};

static uint8_t gta_entry_mask(uint16_t id) {
    return (uint8_t)(1u << (id % 8u));
}

static bool gta_is_checked(const uint8_t* checked, uint16_t id) {
    return (checked[id / 8u] & gta_entry_mask(id)) != 0;
}

static void gta_set_checked(uint8_t* checked, uint16_t id, bool value) {
    const uint8_t mask = gta_entry_mask(id);
    if(value) {
        checked[id / 8u] |= mask;
    } else {
        checked[id / 8u] &= (uint8_t)~mask;
    }
}

static void gta_set_ahol_pieces(uint8_t* checked, size_t type, bool value) {
    const uint16_t first = gta_ahol_ranges[type].first;
    for(uint16_t id = first; id < first + gta_ahol_ranges[type].count; ++id) {
        gta_set_checked(checked, id, value);
    }
}

static void gta_toggle_checkmark(uint8_t* checked, uint16_t id) {
    const bool value = !gta_is_checked(checked, id);
    gta_set_checked(checked, id, value);
    for(size_t type = 0; type < GTA_AHOL_TYPE_COUNT; ++type) {
        const uint16_t type_id = GtaAholFirstTypeId + type;
        if(id == type_id) {
            gta_set_ahol_pieces(checked, type, value);
            return;
        }
        const uint16_t first = gta_ahol_ranges[type].first;
        const uint16_t end = first + gta_ahol_ranges[type].count;
        if(id < first || id >= end) continue;
        bool complete = true;
        for(uint16_t piece = first; piece < end; ++piece) {
            if(!gta_is_checked(checked, piece)) {
                complete = false;
                break;
            }
        }
        gta_set_checked(checked, type_id, complete);
        return;
    }
}

static uint32_t gta_save_checksum(const void* record, size_t length) {
    const uint8_t* bytes = record;
    uint32_t checksum = UINT32_C(2166136261);
    for(size_t i = 0; i < length; ++i) {
        checksum ^= bytes[i];
        checksum *= UINT32_C(16777619);
    }
    return checksum;
}

static bool gta_read_save(Storage* storage, const char* path, GtaSave* save) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    const bool valid = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                       storage_file_size(file) == sizeof(*save) &&
                       storage_file_read(file, save, sizeof(*save)) == sizeof(*save) &&
                       save->magic == GtaSaveMagic &&
                       save->checksum == gta_save_checksum(save, offsetof(GtaSave, checksum));
    storage_file_free(file);
    return valid;
}

static bool gta_read_legacy_save(Storage* storage, const char* path, GtaLegacySave* save) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    const bool valid = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                       storage_file_size(file) == sizeof(*save) &&
                       storage_file_read(file, save, sizeof(*save)) == sizeof(*save) &&
                       save->magic == GtaLegacySaveMagic &&
                       save->checksum ==
                           gta_save_checksum(save, offsetof(GtaLegacySave, checksum));
    storage_file_free(file);
    return valid;
}

static bool gta_generation_is_newer(uint32_t incoming, uint32_t current) {
    const uint32_t age = incoming - current;
    return age != 0 && age < UINT32_C(0x80000000);
}

static void gta_load_progress(GtaCompanion* companion) {
    memset(&companion->save, 0, sizeof(companion->save));
    companion->save.magic = GtaSaveMagic;
    companion->save_slot = GtaSaveSlots;

    // Prefer any valid extended save, even if a legacy file has a higher generation.
    GtaSave* candidate = malloc(sizeof(*candidate));
    if(!candidate) {
        companion->save_error = true;
        companion->progress_load_failed = true;
        return;
    }
    for(size_t slot = 0; slot < GtaSaveSlots; ++slot) {
        if(!gta_read_save(companion->storage, gta_save_paths[slot], candidate)) continue;
        if(companion->save_slot == GtaSaveSlots ||
           gta_generation_is_newer(candidate->generation, companion->save.generation)) {
            companion->save = *candidate;
            companion->save_slot = slot;
        }
    }
    free(candidate);

    // First run after upgrade: import valid GTA2 saves but never overwrite them.
    if(companion->save_slot == GtaSaveSlots) {
        GtaLegacySave* legacy = malloc(sizeof(*legacy));
        if(!legacy) {
            companion->save_error = true;
            companion->progress_load_failed = true;
            return;
        }
        bool have_legacy = false;
        uint32_t legacy_generation = 0;
        for(size_t slot = 0; slot < GtaSaveSlots; ++slot) {
            if(!gta_read_legacy_save(companion->storage, gta_legacy_save_paths[slot], legacy))
                continue;
            if(!have_legacy || gta_generation_is_newer(legacy->generation, legacy_generation)) {
                memcpy(companion->save.checked, legacy->checked, GtaLegacyProgressBytes);
                legacy_generation = legacy->generation;
                have_legacy = true;
            }
        }
        if(have_legacy) companion->save.generation = legacy_generation;
        free(legacy);
    }

    // A checked type in an existing save means all of its pieces were scanned.
    for(size_t type = 0; type < GTA_AHOL_TYPE_COUNT; ++type) {
        if(gta_is_checked(companion->save.checked, GtaAholFirstTypeId + type)) {
            gta_set_ahol_pieces(companion->save.checked, type, true);
        }
    }
}

static bool gta_save_checkmark(GtaCompanion* companion, uint16_t id) {
    if(id >= GtaProgressBytes * 8u || companion->progress_load_failed) return false;
    Storage* storage = companion->storage;
    if(!storage_simply_mkdir(storage, EXT_PATH("apps_data")) ||
       !storage_simply_mkdir(storage, GTA_DATA_DIRECTORY)) {
        return false;
    }

    // Keep the 524-byte record off the Flipper app's small (2 KiB) task stack.
    GtaSave* next = malloc(sizeof(*next));
    if(!next) return false;
    *next = companion->save;
    gta_toggle_checkmark(next->checked, id);
    ++next->generation;
    next->checksum = gta_save_checksum(next, offsetof(GtaSave, checksum));

    // Preserve the last valid GTA3 slot until a full replacement has synced.
    const size_t slot = companion->save_slot == 0 ? 1 : 0;
    const char* path = gta_save_paths[slot];
    File* file = storage_file_alloc(storage);
    if(!file) {
        free(next);
        return false;
    }
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        free(next);
        return false;
    }

    const bool written = storage_file_write(file, next, sizeof(*next)) == sizeof(*next) &&
                         storage_file_sync(file);
    const bool closed = storage_file_close(file);
    storage_file_free(file);
    if(!written || !closed) {
        storage_simply_remove(storage, path);
        free(next);
        return false;
    }

    companion->save = *next;
    companion->save_slot = slot;
    free(next);
    return true;
}

static void gta_draw_label(Canvas* canvas, uint8_t x, uint8_t y, uint8_t width, const char* text) {
    if(canvas_string_width(canvas, text) <= width) {
        canvas_draw_str(canvas, x, y, text);
        return;
    }

    char label[64];
    size_t length = strlen(text);
    if(length > sizeof(label) - 4) length = sizeof(label) - 4;
    memcpy(label, text, length);
    for(;;) {
        memcpy(label + length, "...", 4);
        if(canvas_string_width(canvas, label) <= width || length == 0) break;
        --length;
    }
    canvas_draw_str(canvas, x, y, label);
}

static void gta_draw_list(Canvas* canvas, void* context) {
    const GtaListModel* model = context;
    const GtaCollection* collection = model->collection;
    canvas_clear(canvas);
    if(!collection) return;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    char count[12];
    snprintf(
        count, sizeof(count), "%u/%u", (unsigned)model->checked_count, (unsigned)collection->count);
    const uint8_t count_x = 126 - canvas_string_width(canvas, count);
    gta_draw_label(canvas, 2, 9, count_x - 5, collection->title);
    canvas_draw_str(canvas, count_x, 9, count);
    canvas_draw_line(canvas, 0, 11, 127, 11);

    if(collection->count == 0) {
        gta_draw_label(canvas, 2, 29, 124, model->list_status ? model->list_status : "No entries");
        canvas_draw_str(canvas, 2, 62, "Back");
        return;
    }

    const size_t first = model->selected / GtaListRows * GtaListRows;
    for(size_t row = 0; row < GtaListRows && first + row < collection->count; ++row) {
        const size_t index = first + row;
        const GtaCollectionEntry* entry = &collection->entries[index];
        const uint8_t y = 20 + row * 10;
        if(index == model->selected) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_frame(canvas, 3, y - 6, 7, 7);
        if(gta_is_checked(model->checked, entry->id)) {
            canvas_draw_line(canvas, 4, y - 3, 6, y - 1);
            canvas_draw_line(canvas, 6, y - 1, 8, y - 5);
        }
        gta_draw_label(canvas, 14, y, 111, entry->name);
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_line(canvas, 0, 53, 127, 53);
    canvas_draw_str(canvas, 2, 62, model->save_error ? "SD save failed" : "OK Info   > Check");
}

static const GtaCollectionEntry* gta_selected_entry(const GtaCompanion* companion) {
    if(companion->dispatch) return companion->dispatch_entry;
    if(companion->list->count == 0) return NULL;
    return &companion->list->entries[*companion->selection];
}

static void gta_refresh_list(GtaCompanion* companion) {
    const GtaCollection* collection = companion->list;
    size_t checked_count = 0;
    for(size_t i = 0; i < collection->count; ++i) {
        if(gta_is_checked(companion->save.checked, collection->entries[i].id)) ++checked_count;
    }

    with_view_model(
        companion->list_view,
        GtaListModel * model,
        {
            model->collection = collection;
            model->selected = *companion->selection;
            model->checked_count = checked_count;
            memcpy(model->checked, companion->save.checked, sizeof(model->checked));
            model->save_error = companion->save_error;
            model->list_status = companion->list_status;
        },
        true);
}

static void gta_detail_button(GuiButtonType button, InputType type, void* context) {
    GtaCompanion* companion = context;
    if(type != InputTypeShort) return;
    if(button == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(companion->dispatcher, GtaDetailToggle);
    } else if(button == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(companion->dispatcher, GtaDispatchNext);
    }
}

static void gta_render_detail(GtaCompanion* companion) {
    Widget* detail = companion->detail;
    FuriString* text = companion->text;
    const GtaCollectionEntry* entry = gta_selected_entry(companion);
    widget_reset(detail);
    furi_string_reset(text);

    if(companion->dispatch) {
        furi_string_cat_printf(
            text, "Dispatch / %s\n", gta_dispatch_regions[companion->dispatch_region]);
    }
    if(entry) {
        const GtaCollection* collection = companion->dispatch ? companion->dispatch_collection :
                                                                companion->list;
        const bool checked = gta_is_checked(companion->save.checked, entry->id);
        furi_string_cat_printf(
            text, "%s\n%s\n", entry->name, checked ? collection->checked_label : "Unchecked");
        if(companion->save_error) furi_string_cat_printf(text, "SD save failed. Try again.\n");
        furi_string_cat_printf(
            text, "\n%s", entry->location ? entry->location : "Location unconfirmed.");
        if(entry->notes) furi_string_cat_printf(text, "\n\n%s", entry->notes);
        if(companion->dispatch) {
            furi_string_cat_printf(text, "\n\n%s", collection->title);
        } else if(
            companion->explore_section &&
            companion->explore_section->game_total > collection->count) {
            furi_string_cat_printf(
                text,
                "\n\nCatalog coverage: %u/%u",
                (unsigned)collection->count,
                (unsigned)companion->explore_section->game_total);
        }
        widget_add_button_element(
            detail, GuiButtonTypeCenter, checked ? "Undo" : "Check", gta_detail_button, companion);
        if(companion->dispatch) {
            widget_add_button_element(
                detail, GuiButtonTypeRight, "Next", gta_detail_button, companion);
        }
    } else {
        furi_string_cat_printf(
            text,
            "\n%s",
            companion->dispatch_unavailable ? "Some location data is unavailable." :
                                              "No unchecked stops with a location clue.");
    }
    widget_add_text_scroll_element(detail, 0, 0, 128, entry ? 51 : 64, furi_string_get_cstr(text));
}

static void gta_show_detail(GtaCompanion* companion) {
    gta_render_detail(companion);
    companion->detail_visible = true;
    view_dispatcher_switch_to_view(companion->dispatcher, companion->detail_view_id);
}

static bool gta_input_list(InputEvent* event, void* context) {
    GtaCompanion* companion = context;
    if(!companion->list || !companion->list->count ||
       (event->type != InputTypeShort && event->type != InputTypeRepeat)) {
        return false;
    }

    size_t* selected = companion->selection;
    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        if(event->key == InputKeyUp && *selected > 0) --*selected;
        if(event->key == InputKeyDown && *selected + 1 < companion->list->count) {
            ++*selected;
        }
        with_view_model(
            companion->list_view, GtaListModel * model, { model->selected = *selected; }, true);
        return true;
    }
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyRight) {
        companion->save_error = !gta_save_checkmark(companion, gta_selected_entry(companion)->id);
        gta_refresh_list(companion);
        return true;
    }
    if(event->key == InputKeyOk) {
        gta_show_detail(companion);
        return true;
    }
    return false;
}

static char* gta_take_field(char** line, char delimiter) {
    if(!*line) return NULL;
    char* field = *line;
    char* end = strchr(field, delimiter);
    if(!end) return NULL;
    *end = '\0';
    *line = end + 1;
    return field;
}

static bool gta_parse_explore_asset(GtaCompanion* companion, char* data, size_t size) {
    size_t rows = 0;
    for(size_t i = 0; i < size; ++i) {
        if(data[i] == '\n') ++rows;
    }
    if(!rows || rows > GtaExploreMaxRows || data[size - 1] != '\n') return false;
    GtaCollectionEntry* entries = calloc(rows, sizeof(*entries));
    if(!entries) return false;

    char* cursor = data;
    for(size_t i = 0; i < rows; ++i) {
        char* line = gta_take_field(&cursor, '\n');
        const char* id = gta_take_field(&line, '\t');
        const char* name = gta_take_field(&line, '\t');
        const char* location = gta_take_field(&line, '\t');
        char* end = NULL;
        const unsigned long value = id ? strtoul(id, &end, 10) : 0;
        if(!id || end == id || *end || value < GtaExploreFirstId ||
           value >= GtaProgressBytes * 8 || !name || !*name || !location || strchr(line, '\t')) {
            free(entries);
            return false;
        }
        entries[i] = (GtaCollectionEntry){
            .id = (uint16_t)value,
            .name = name,
            .location = *location ? location : NULL,
            .notes = *line ? line : NULL,
        };
    }
    companion->explore_rows = entries;
    companion->explore_data = data;
    companion->explore_list.entries = entries;
    companion->explore_list.count = rows;
    return true;
}

static bool gta_load_explore_asset(GtaCompanion* companion, const char* asset) {
    char path[88];
    const int length = snprintf(path, sizeof(path), APP_ASSETS_PATH("collections/%s"), asset);
    if(length < 0 || (size_t)length >= sizeof(path)) return false;

    File* file = storage_file_alloc(companion->storage);
    char* data = NULL;
    size_t size = 0;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        const uint64_t file_size = storage_file_size(file);
        if(file_size && file_size <= GtaExploreMaxAssetSize) {
            size = (size_t)file_size;
            data = malloc(size + 1);
            if(data && storage_file_read(file, data, size) != size) {
                free(data);
                data = NULL;
            }
        }
    }
    storage_file_free(file);
    if(!data) return false;

    data[size] = '\0';
    if(gta_parse_explore_asset(companion, data, size)) return true;
    free(data);
    return false;
}

static void gta_release_exploration(GtaCompanion* companion) {
    if(!companion->explore_section) return;
    // Draw callbacks hold the model lock while reading the asset-backed rows.
    with_view_model(
        companion->list_view, GtaListModel * model, { model->collection = NULL; }, false);
    companion->explore_section = NULL;
    free(companion->explore_rows);
    free(companion->explore_data);
    companion->explore_rows = NULL;
    companion->explore_data = NULL;
    companion->explore_list.entries = NULL;
    companion->explore_list.count = 0;
}

static bool gta_prepare_exploration(GtaCompanion* companion, const GtaExploreSection* section) {
    if(companion->explore_section == section) return companion->explore_list.count != 0;
    gta_release_exploration(companion);
    companion->explore_section = section;
    companion->explore_list = (GtaCollection){
        .title = section->title,
        .checked_label = section->checked_label,
    };
    return section->asset && gta_load_explore_asset(companion, section->asset);
}

static const GtaExploreSection*
    gta_find_explore_asset(const GtaExploreSection* sections, size_t count, const char* asset) {
    for(size_t i = 0; i < count; ++i) {
        const GtaExploreSection* section = &sections[i];
        if(section->asset && strcmp(section->asset, asset) == 0) return section;
        if(section->child_count) {
            const GtaExploreSection* found =
                gta_find_explore_asset(section->children, section->child_count, asset);
            if(found) return found;
        }
    }
    return NULL;
}

static const GtaCollection* gta_dispatch_load_source(GtaCompanion* companion, uint8_t source) {
    if(source < GtaDispatchFirstAsset) {
        gta_release_exploration(companion);
        return &gta_collection_catalog
            [source == GtaDispatchSourceMoses ? GtaCollectionMoses : GtaCollectionRandy];
    }
    const GtaExploreSection* section = gta_find_explore_asset(
        gta_explore_sections,
        gta_explore_section_count,
        gta_dispatch_assets[source - GtaDispatchFirstAsset]);
    if(!section || !gta_prepare_exploration(companion, section)) return NULL;
    return &companion->explore_list;
}

static void gta_dispatch_next(GtaCompanion* companion) {
    const uint8_t region = 1u << companion->dispatch_region;
    const bool all_regions = companion->dispatch_region == GtaDispatchAll;
    uint8_t loaded_source = UINT8_MAX;
    const GtaCollection* collection = NULL;

    companion->dispatch_entry = NULL;
    companion->dispatch_collection = NULL;
    companion->dispatch_unavailable = false;
    for(size_t tried = 0; tried < gta_dispatch_target_count; ++tried) {
        const GtaDispatchTarget* target = &gta_dispatch_targets[companion->dispatch_cursor];
        if(++companion->dispatch_cursor == gta_dispatch_target_count)
            companion->dispatch_cursor = 0;
        if((!all_regions && !(target->regions & region)) ||
           gta_is_checked(companion->save.checked, target->id)) {
            continue;
        }
        if(loaded_source != target->source) {
            loaded_source = target->source;
            collection = gta_dispatch_load_source(companion, loaded_source);
        }
        if(collection) {
            for(size_t i = 0; i < collection->count; ++i) {
                const GtaCollectionEntry* entry = &collection->entries[i];
                if(entry->id == target->id && entry->location) {
                    companion->dispatch_entry = entry;
                    companion->dispatch_collection = collection;
                    return;
                }
            }
        }
        companion->dispatch_unavailable = true;
    }
    gta_release_exploration(companion);
}

GtaCompanion* gta_companion_alloc(
    ViewDispatcher* dispatcher,
    uint32_t list_view_id,
    uint32_t detail_view_id) {
    GtaCompanion* companion = calloc(1, sizeof(*companion));
    furi_check(companion);
    companion->dispatcher = dispatcher;
    companion->list_view_id = list_view_id;
    companion->detail_view_id = detail_view_id;
    companion->list_view = view_alloc();
    companion->detail = widget_alloc();
    companion->text = furi_string_alloc();
    companion->storage = furi_record_open(RECORD_STORAGE);
    gta_load_progress(companion);

    view_set_context(companion->list_view, companion);
    view_allocate_model(companion->list_view, ViewModelTypeLocking, sizeof(GtaListModel));
    view_set_draw_callback(companion->list_view, gta_draw_list);
    view_set_input_callback(companion->list_view, gta_input_list);
    view_dispatcher_add_view(dispatcher, list_view_id, companion->list_view);
    view_dispatcher_add_view(dispatcher, detail_view_id, widget_get_view(companion->detail));
    return companion;
}

void gta_companion_free(GtaCompanion* companion) {
    view_dispatcher_remove_view(companion->dispatcher, companion->detail_view_id);
    view_dispatcher_remove_view(companion->dispatcher, companion->list_view_id);
    widget_free(companion->detail);
    gta_release_exploration(companion);
    view_free(companion->list_view);
    furi_string_free(companion->text);
    furi_record_close(RECORD_STORAGE);
    free(companion);
}

static void gta_open_checklist(
    GtaCompanion* companion,
    const GtaCollection* list,
    size_t* selection,
    const char* status) {
    companion->list = list;
    companion->selection = selection;
    companion->list_status = status;
    companion->dispatch = false;
    companion->detail_visible = false;
    companion->save_error = false;
    gta_refresh_list(companion);
    view_dispatcher_switch_to_view(companion->dispatcher, companion->list_view_id);
}

void gta_companion_open_list(GtaCompanion* companion, GtaCollectionId collection) {
    gta_release_exploration(companion);
    gta_open_checklist(
        companion, &gta_collection_catalog[collection], &companion->selections[collection], NULL);
}

void gta_companion_open_exploration(GtaCompanion* companion, const GtaExploreSection* section) {
    companion->explore_selection = 0;
    const bool loaded = gta_prepare_exploration(companion, section);
    const char* status = section->asset ? (loaded ? NULL : "Collection unavailable") :
                                          "Locations pending";
    gta_open_checklist(companion, &companion->explore_list, &companion->explore_selection, status);
}

void gta_companion_open_dispatch(GtaCompanion* companion, GtaDispatchRegion region) {
    gta_release_exploration(companion);
    companion->dispatch = true;
    companion->dispatch_region = region;
    companion->dispatch_cursor = 0;
    companion->save_error = false;
    gta_dispatch_next(companion);
    gta_show_detail(companion);
}

bool gta_companion_custom_event(GtaCompanion* companion, uint32_t event) {
    if(!companion->detail_visible) return false;
    if(event == GtaDetailToggle) {
        const GtaCollectionEntry* entry = gta_selected_entry(companion);
        if(entry) {
            companion->save_error = !gta_save_checkmark(companion, entry->id);
            gta_render_detail(companion);
        }
        return true;
    }
    if(event == GtaDispatchNext && companion->dispatch) {
        companion->save_error = false;
        gta_dispatch_next(companion);
        gta_render_detail(companion);
        return true;
    }
    return false;
}

bool gta_companion_back(GtaCompanion* companion) {
    if(companion->detail_visible && !companion->dispatch) {
        companion->detail_visible = false;
        gta_refresh_list(companion);
        view_dispatcher_switch_to_view(companion->dispatcher, companion->list_view_id);
        return true;
    }
    companion->detail_visible = false;
    companion->dispatch = false;
    companion->dispatch_entry = NULL;
    companion->dispatch_collection = NULL;
    gta_release_exploration(companion);
    return false;
}

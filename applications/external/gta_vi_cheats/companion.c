#include "companion.h"
#include "url_links.h"
#include "gta_catalog_io.h"
#include "gta_nfc_plugin.h"
#include "gta_handoff.h"
#include <flipper_application/flipper_application.h>
#include <loader/firmware_api/firmware_api.h>

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
    GtaDetailLinks,
    GtaLinkSource,
    GtaLinkRight,
    GtaWreckPicker,
    GtaWreckPrev,
    GtaWreckNext,
    GtaWreckEmulate,
    GtaExploreMaxAssetSize = 256 * 1024,
    GtaExploreMaxRows = 4096,
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
    struct {
        uint16_t id;
        char name[64];
        bool checked;
    } rows[GtaListRows];
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
    const char* asset;
    bool vehicle_asset;
    uint32_t* row_offsets; // Built on demand from the active TSV
    uint16_t* row_ids;
    GtaRecord selected_record;
    GtaCollectionEntry selected_entry;
    size_t selected_row;
    bool selected_valid;
    const char* list_status;
    const GtaCollectionEntry* dispatch_entry;
    const GtaCollection* dispatch_collection;
    uint32_t dispatch_cursor; // Byte offset in dispatch.tsv
    GtaDispatchRegion dispatch_region;
    bool dispatch_unavailable;
    bool detail_visible;
    bool links_visible;
    const char* guide_text;
    bool wreck_picker;
    size_t wreck_index;
    FlipperApplication* nfc_module;
    const GtaNfcPluginApi* nfc_api;
    void* nfc_session;
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

/* Unique collection IDs, from companion_catalog.c and every
 * Collections TSV. Regenerate these ranges when adding or removing entries;
 * do not count the unlisted 4,096-bit progress capacity as collectibles. */
static const struct {
    uint16_t first;
    uint16_t last;
} gta_collection_id_ranges[] = {
    {1, 7},
    {16, 28},
    {29, 34},
    {64, 68},
    {80, 94},
    {112, 115},
    {128, 233},
    {400, 510},
    {512, 1270},
    {1272, 2100},
    {2300, 2552},
    {3000, 3095},
    {3582, 3796},
    {3900, 3918},
};

bool gta_companion_progress_summary(
    const GtaCompanion* companion,
    size_t* total_items,
    size_t* completed_items) {
    if(!companion || !total_items || !completed_items || companion->progress_load_failed)
        return false;

    size_t total = 0;
    size_t completed = 0;
    for(size_t i = 0; i < sizeof(gta_collection_id_ranges) / sizeof(gta_collection_id_ranges[0]);
        ++i) {
        for(uint16_t id = gta_collection_id_ranges[i].first;
            id <= gta_collection_id_ranges[i].last;
            ++id) {
            ++total;
            if(gta_is_checked(companion->save.checked, id)) ++completed;
        }
    }
    *total_items = total;
    *completed_items = completed;
    return true;
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

        const uint8_t y = 20 + row * 10;
        if(index == model->selected) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_frame(canvas, 3, y - 6, 7, 7);
        if(model->rows[row].checked) {
            canvas_draw_line(canvas, 4, y - 3, 6, y - 1);
            canvas_draw_line(canvas, 6, y - 1, 8, y - 5);
        }
        gta_draw_label(canvas, 14, y, 111, model->rows[row].name);
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_line(canvas, 0, 53, 127, 53);
    canvas_draw_str(canvas, 2, 62, model->save_error ? "SD save failed" : "OK Info   > Check");
}

static bool gta_read_collection_row(GtaCompanion* companion, size_t row, GtaRecord* record);
static File* gta_open_tsv(GtaCompanion* companion);
static bool
    gta_read_collection_label(GtaCompanion* companion, size_t row, char* name, size_t capacity) {
    if(row >= companion->explore_list.count || !capacity) return false;
    // The TSV index was validated on open. Read only the name prefix (not its description).
    File* file = gta_open_tsv(companion);
    if(!file) return false;
    uint32_t start = companion->row_offsets[row], end = companion->row_offsets[row + 1u];
    char prefix[72];
    size_t n = end - start < sizeof(prefix) ? end - start : sizeof(prefix);
    bool ok = storage_file_seek(file, start, true) && storage_file_read(file, prefix, n) == n;
    storage_file_free(file);
    if(!ok) return false;
    const char* tab = memchr(prefix, '\t', n);
    if(!tab) return false;
    const char* from = tab + 1;
    size_t available = n - (size_t)(from - prefix);
    const char* next = memchr(from, '\t', available);
    if(next) available = (size_t)(next - from);
    if(!available) return false;
    if(available >= capacity) available = capacity - 1u;
    if(companion->vehicle_asset && memchr(from, '\\', available)) {
        GtaRecord record = {0};
        bool decoded = gta_read_collection_row(companion, row, &record);
        if(decoded) snprintf(name, capacity, "%s", record.field[1]);
        gta_record_clear(&record);
        return decoded;
    }
    memcpy(name, from, available);
    name[available] = 0;
    return true;
}
static const GtaCollectionEntry* gta_read_selected(GtaCompanion* companion, size_t row) {
    if(companion->selected_valid && companion->selected_row == row)
        return &companion->selected_entry;
    companion->selected_valid = false;
    if(!gta_read_collection_row(companion, row, &companion->selected_record)) return NULL;
    if(strtoul(companion->selected_record.field[0], NULL, 10) != companion->row_ids[row]) {
        gta_record_clear(&companion->selected_record);
        return NULL;
    }
    char** f = companion->selected_record.field;
    companion->selected_entry = (GtaCollectionEntry){
        companion->row_ids[row], f[1], *f[2] ? f[2] : NULL, *f[3] ? f[3] : NULL};
    companion->selected_row = row;
    companion->selected_valid = true;
    return &companion->selected_entry;
}
typedef struct {
    const char* title;
    const char* url;
    bool guide;
} GtaWreckMap;

#define GTA_WRECK_PIN(n) "https://gta6gps.com/?p=pin-" #n
static const GtaWreckMap gta_transgressor_maps[] = {
    {"Wreck marker 1", GTA_WRECK_PIN(918071), false},
    {"Wreck marker 2", GTA_WRECK_PIN(918070), false},
    {"Wreck marker 3", GTA_WRECK_PIN(918153), false},
};
static const GtaWreckMap gta_riata_maps[] = {
    {"Wreck marker 1", GTA_WRECK_PIN(918507), false},
    {"Wreck marker 2", GTA_WRECK_PIN(918508), false},
    {"Wreck marker 3", GTA_WRECK_PIN(918509), false},
};
static const GtaWreckMap gta_mamba_maps[] = {
    {"Collapsed bridge", GTA_WRECK_PIN(918691), false},
    {"Emerald Springs (lead)", GTA_WRECK_PIN(918473), false},
    {"All four wrecks - guide",
     "https://www.pcgamer.com/games/grand-theft-auto/gta-6-mamba-gt-locations/",
     true},
};
static const GtaWreckMap gta_kit_maps[] = {
    {"Yellow container (probable)", GTA_WRECK_PIN(918731), false},
    {"All five wrecks - guide",
     "https://www.pcgamer.com/games/grand-theft-auto/gta-6-kit-car-locations/",
     true},
};
#undef GTA_WRECK_PIN

static const GtaWreckMap* gta_wreck_maps(uint16_t id, size_t* count) {
    switch(id) {
    case 64:
        *count = sizeof(gta_transgressor_maps) / sizeof(gta_transgressor_maps[0]);
        return gta_transgressor_maps;
    case 65:
        *count = sizeof(gta_riata_maps) / sizeof(gta_riata_maps[0]);
        return gta_riata_maps;
    case 66:
        *count = sizeof(gta_mamba_maps) / sizeof(gta_mamba_maps[0]);
        return gta_mamba_maps;
    case 67:
        *count = sizeof(gta_kit_maps) / sizeof(gta_kit_maps[0]);
        return gta_kit_maps;
    default:
        *count = 0;
        return NULL;
    }
}

static const GtaCollectionEntry* gta_selected_entry(GtaCompanion* companion) {
    static const GtaCollectionEntry money_entry = {222, "Apollo Aggregates", NULL, NULL};
    if(companion->guide_text) return &money_entry;
    if(companion->dispatch) return companion->dispatch_entry;
    if(!companion->list || !companion->list->count) return NULL;
    return gta_read_selected(companion, *companion->selection);
}

static const GtaUrlLinks*
    gta_selected_links(const GtaCompanion* companion, const GtaCollectionEntry* entry) {
    if(!entry) return NULL;
    if(companion->guide_text) {
        static const GtaUrlLinks money_map = {.source = "https://gta6gps.com/?p=pin-916938"};
        return &money_map;
    }
    if(companion->wreck_picker) {
        size_t count;
        const GtaWreckMap* maps = gta_wreck_maps(entry->id, &count);
        if(!maps || companion->wreck_index >= count) return NULL;
        static GtaUrlLinks wreck_link;
        wreck_link = (GtaUrlLinks){
            .source = maps[companion->wreck_index].url,
            .source_index = maps[companion->wreck_index].guide};
        return &wreck_link;
    }
    const GtaCollection* collection = companion->dispatch ? companion->dispatch_collection :
                                                            companion->list;
    return gta_url_links_resolve(
        entry->id,
        companion->explore_section ? companion->explore_section->asset : NULL,
        collection ? collection->title : NULL);
}

static void gta_refresh_list(GtaCompanion* companion) {
    const GtaCollection* collection = companion->list;
    size_t checked_count = 0;
    for(size_t i = 0; i < collection->count; ++i) {
        if(gta_is_checked(companion->save.checked, companion->row_ids[i])) ++checked_count;
    }
    size_t first = *companion->selection / GtaListRows * GtaListRows;
    GtaListModel page = {0};
    for(size_t row = 0; row < GtaListRows && first + row < collection->count; ++row) {
        size_t i = first + row;
        page.rows[row].id = companion->row_ids[i];
        page.rows[row].checked = gta_is_checked(companion->save.checked, companion->row_ids[i]);
        if(!gta_read_collection_label(
               companion, i, page.rows[row].name, sizeof(page.rows[row].name)))
            snprintf(page.rows[row].name, sizeof(page.rows[row].name), "Data unavailable");
    }
    with_view_model(
        companion->list_view,
        GtaListModel * model,
        {
            model->collection = collection;
            model->selected = *companion->selection;
            model->checked_count = checked_count;
            memcpy(model->rows, page.rows, sizeof(model->rows));
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
        view_dispatcher_send_custom_event(companion->dispatcher, GtaDetailLinks);
    } else if(button == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(
            companion->dispatcher, companion->dispatch ? GtaDispatchNext : GtaWreckPicker);
    }
}

static void gta_wreck_button(GuiButtonType button, InputType type, void* context) {
    GtaCompanion* companion = context;
    if(type != InputTypeShort) return;
    uint32_t event = button == GuiButtonTypeCenter ? GtaWreckEmulate :
                     button == GuiButtonTypeLeft   ? GtaWreckPrev :
                                                     GtaWreckNext;
    view_dispatcher_send_custom_event(companion->dispatcher, event);
}

static void gta_render_detail(GtaCompanion* companion) {
    Widget* detail = companion->detail;
    FuriString* text = companion->text;
    const GtaCollectionEntry* entry = gta_selected_entry(companion);
    widget_reset(detail);
    furi_string_reset(text);

    if(companion->guide_text) {
        widget_add_text_scroll_element(detail, 0, 0, 128, 51, companion->guide_text);
        widget_add_button_element(detail, GuiButtonTypeRight, "Map", gta_detail_button, companion);
        return;
    }
    if(companion->wreck_picker && entry) {
        size_t count;
        const GtaWreckMap* maps = gta_wreck_maps(entry->id, &count);
        if(maps && companion->wreck_index < count) {
            const GtaWreckMap* map = &maps[companion->wreck_index];
            furi_string_cat_printf(
                text,
                "%s\nWreck links %u/%u\n\n%s\n%s",
                entry->name,
                (unsigned)(companion->wreck_index + 1),
                (unsigned)count,
                map->title,
                map->guide ? "Open component location guide." : "Open reported map location.");
            widget_add_text_scroll_element(detail, 0, 0, 128, 51, furi_string_get_cstr(text));
            widget_add_button_element(
                detail,
                GuiButtonTypeCenter,
                map->guide ? "Guide" : "Map",
                gta_wreck_button,
                companion);
            if(companion->wreck_index > 0)
                widget_add_button_element(
                    detail, GuiButtonTypeLeft, "Prev", gta_wreck_button, companion);
            if(companion->wreck_index + 1 < count)
                widget_add_button_element(
                    detail, GuiButtonTypeRight, "Next", gta_wreck_button, companion);
            return;
        }
        companion->wreck_picker = false;
    }
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
        const GtaUrlLinks* links = gta_selected_links(companion, entry);
        const bool can_emulate = gta_url_links_choose(links, false).url ||
                                 gta_url_links_choose(links, true).url;
        if(can_emulate && links->source_index && !links->source2 && !links->map_image)
            furi_string_cat_printf(text, "NFC: shared guide/index\n");
        gta_url_links_release();
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
                detail, GuiButtonTypeLeft, "Next", gta_detail_button, companion);
        } else {
            size_t count;
            if(gta_wreck_maps(entry->id, &count))
                widget_add_button_element(
                    detail, GuiButtonTypeLeft, "Wrecks", gta_detail_button, companion);
        }
        if(can_emulate) {
            widget_add_button_element(
                detail, GuiButtonTypeRight, "Emulate", gta_detail_button, companion);
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

static void gta_link_button(GuiButtonType button, InputType type, void* context) {
    GtaCompanion* companion = context;
    if(type != InputTypeShort) return;
    if(button == GuiButtonTypeLeft)
        view_dispatcher_send_custom_event(companion->dispatcher, GtaLinkSource);
    else if(button == GuiButtonTypeRight)
        view_dispatcher_send_custom_event(companion->dispatcher, GtaLinkRight);
}

static void gta_show_links(GtaCompanion* companion, const char* error) {
    const GtaCollectionEntry* entry = gta_selected_entry(companion);
    const GtaUrlLinks* links = gta_selected_links(companion, entry);
    const GtaUrlChoice left = gta_url_links_choose(links, false);
    const GtaUrlChoice right = gta_url_links_choose(links, true);
    if(!left.url && !right.url) {
        gta_url_links_release();
        companion->links_visible = false;
        gta_render_detail(companion);
        return;
    }
    companion->links_visible = true;
    widget_reset(companion->detail);
    furi_string_reset(companion->text);
    furi_string_cat_printf(companion->text, "NFC URL links\n%s\n", entry->name);
    if(error)
        furi_string_cat_printf(companion->text, "%s\n", error);
    else {
        furi_string_cat_printf(companion->text, "Choose a link.\n");
        if(left.map_index)
            furi_string_cat_printf(companion->text, "Source: shared guide/index.\n");
        if(right.map_index)
            furi_string_cat_printf(companion->text, "Source 2: shared guide/index.\n");
    }
    widget_add_text_scroll_element(
        companion->detail, 0, 0, 128, 51, furi_string_get_cstr(companion->text));
    if(left.url)
        widget_add_button_element(
            companion->detail,
            GuiButtonTypeLeft,
            companion->guide_text ? "Map" : left.label,
            gta_link_button,
            companion);
    if(right.url)
        widget_add_button_element(
            companion->detail, GuiButtonTypeRight, right.label, gta_link_button, companion);
    gta_url_links_release();
}

/* Host owns the mapped module and releases it only after its view, timer and
 * NFC listener have been stopped and detached. No plugin callback unloads itself. */
static void gta_nfc_release(GtaCompanion* companion) {
    if(companion->nfc_session) {
        companion->nfc_api->close(companion->nfc_session);
        companion->nfc_session = NULL;
    }
    companion->nfc_api = NULL;
    if(companion->nfc_module) {
        flipper_application_free(companion->nfc_module);
        companion->nfc_module = NULL;
    }
}

static void gta_emulate_link(GtaCompanion* companion, bool right) {
    const GtaCollectionEntry* entry = gta_selected_entry(companion);
    const GtaUrlChoice choice = gta_url_links_choose(gta_selected_links(companion, entry), right);
    if(!entry || !choice.url || companion->nfc_module) {
        gta_url_links_release();
        return;
    }
    if(strlen(choice.url) >= GTA_URL_MAX) {
        gta_show_links(companion, "Link unavailable.");
        return;
    }
    if(!storage_file_exists(companion->storage, GTA_NFC_PATH)) {
        FURI_LOG_E("GTA NFC", "Missing embedded plugin: %s", GTA_NFC_PATH);
        gta_show_links(companion, "NFC plugin missing. Reinstall app.");
        return;
    }
    char* url = strdup(choice.url);
    if(!url) {
        gta_show_links(companion, "Not enough memory.");
        return;
    }
    uint16_t id = entry->id;
    gta_url_links_release();
    companion->selected_valid = false;
    gta_record_clear(&companion->selected_record);
    if(companion->dispatch) companion->dispatch_entry = NULL;
    widget_reset(companion->detail);
    furi_string_free(companion->text);
    companion->text = furi_string_alloc();
    if(companion->list_view) {
        view_dispatcher_remove_view(companion->dispatcher, companion->list_view_id);
        view_free(companion->list_view);
        companion->list_view = NULL;
    }
    gta_ram_log("NFC before loading");
    GtaLoadingHandle* loading = gta_loading_show();
    const char* error = "Unable to load GTA NFC Link.";
    companion->nfc_module = flipper_application_alloc(companion->storage, firmware_api_interface);
    if(!companion->nfc_module) goto fail;
    const FlipperApplicationPreloadStatus preload =
        flipper_application_preload(companion->nfc_module, GTA_NFC_PATH);
    if(preload != FlipperApplicationPreloadStatusSuccess) {
        FURI_LOG_E("GTA NFC", "Plugin preload failed: %d", (int)preload);
        error = "NFC plugin preload failed.";
        goto fail;
    }
    if(!flipper_application_is_plugin(companion->nfc_module)) {
        error = "Invalid NFC plugin.";
        goto fail;
    }
    const FlipperApplicationLoadStatus mapped =
        flipper_application_map_to_memory(companion->nfc_module);
    if(mapped != FlipperApplicationLoadStatusSuccess) {
        FURI_LOG_E("GTA NFC", "Plugin mapping failed: %d", (int)mapped);
        error = "NFC plugin mapping failed.";
        goto fail;
    }
    const FlipperAppPluginDescriptor* descriptor =
        flipper_application_plugin_get_descriptor(companion->nfc_module);
    if(!descriptor || !descriptor->appid || strcmp(descriptor->appid, GTA_NFC_ID) ||
       descriptor->ep_api_version != GTA_NFC_API_VERSION || !descriptor->entry_point) {
        error = "GTA NFC Link version mismatch.";
        goto fail;
    }
    companion->nfc_api = descriptor->entry_point;
    if(companion->nfc_api->size < sizeof(GtaNfcPluginApi) || !companion->nfc_api->open ||
       !companion->nfc_api->stop || !companion->nfc_api->close) {
        error = "GTA NFC Link API mismatch.";
        goto fail;
    }
    companion->nfc_session = companion->nfc_api->open(companion->dispatcher, url, id, choice.kind);
    if(!companion->nfc_session) goto fail;
    gta_loading_hide(loading);
    free(url);
    gta_ram_log("NFC active");
    return;
fail:
    free(url);
    gta_nfc_release(companion);
    if(companion->dispatch)
        companion->dispatch_entry = gta_read_selected(companion, companion->selected_row);
    gta_show_links(companion, error);
    gta_loading_hide(loading);
}

static void gta_nfc_return(GtaCompanion* companion, bool exit) {
    if(!companion->nfc_session) return;
    companion->nfc_api->stop(companion->nfc_session);
    GtaLoadingHandle* loading = exit ? NULL : gta_loading_show();
    // The detail widget and selector were never destroyed. Restore them before
    // removing the plugin's view; remove_view also synchronizes with GUI drawing.
    view_dispatcher_switch_to_view(companion->dispatcher, companion->detail_view_id);
    gta_nfc_release(companion);
    if(!exit && companion->dispatch)
        companion->dispatch_entry = gta_read_selected(companion, companion->selected_row);
    if(!exit) {
        if(companion->links_visible)
            gta_show_links(companion, NULL);
        else
            gta_render_detail(companion);
    }
    gta_loading_hide(loading);
    gta_ram_log("NFC released");
    if(exit) view_dispatcher_stop(companion->dispatcher);
}

static void gta_open_links(GtaCompanion* companion) {
    const GtaCollectionEntry* entry = gta_selected_entry(companion);
    const GtaUrlLinks* links = gta_selected_links(companion, entry);
    const GtaUrlChoice left = gta_url_links_choose(links, false);
    const GtaUrlChoice right = gta_url_links_choose(links, true);
    if(left.url && right.url) {
        gta_show_links(companion, NULL);
    } else if(left.url || right.url) {
        gta_emulate_link(companion, right.url != NULL);
    } else
        gta_url_links_release();
}

static void gta_ensure_detail(GtaCompanion* companion);
static void gta_show_detail(GtaCompanion* companion) {
    gta_ensure_detail(companion);
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
        size_t old_page = *selected / GtaListRows;
        if(event->key == InputKeyUp && *selected > 0) --*selected;
        if(event->key == InputKeyDown && *selected + 1 < companion->list->count) {
            ++*selected;
        }
        if(old_page != *selected / GtaListRows)
            gta_refresh_list(companion);
        else
            with_view_model(
                companion->list_view, GtaListModel * model, { model->selected = *selected; }, true);
        return true;
    }
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyRight) {
        companion->save_error = !gta_save_checkmark(companion, companion->row_ids[*selected]);
        gta_refresh_list(companion);
        return true;
    }
    if(event->key == InputKeyOk) {
        gta_show_detail(companion);
        return true;
    }
    return false;
}

/* Build a temporary row index for the active TSV. Never hold a complete
 * description file in the heap. */
static File* gta_open_tsv(GtaCompanion* companion) {
    char path[96];
    int n = snprintf(
        path,
        sizeof(path),
        APP_ASSETS_PATH("%s/%s"),
        companion->vehicle_asset ? "catalogs" : "collections",
        companion->asset);
    if(n < 0 || (size_t)n >= sizeof(path)) return NULL;
    File* f = storage_file_alloc(companion->storage);
    if(!f) return NULL;
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return NULL;
    }
    return f;
}
static bool gta_read_collection_row(GtaCompanion* companion, size_t row, GtaRecord* r) {
    if(row >= companion->explore_list.count) return false;
    gta_record_clear(r);
    uint32_t start = companion->row_offsets[row], end = companion->row_offsets[row + 1];
    if(end <= start || end - start > GTA_RECORD_MAX) return false;
    File* f = gta_open_tsv(companion);
    if(!f) return false;
    size_t n = end - start;
    r->data = malloc(n);
    bool ok = r->data && storage_file_seek(f, start, true) &&
              storage_file_read(f, r->data, n) == n && r->data[n - 1] == '\n';
    storage_file_free(f);
    if(ok) ok = gta_record_decode(r, n, 4, companion->vehicle_asset) && *r->field[1];
    if(!ok) gta_record_clear(r);
    return ok;
}
static bool gta_load_explore_asset(GtaCompanion* companion, const char* asset) {
    companion->asset = asset;
    File* file = gta_open_tsv(companion);
    if(!file) return false;
    uint64_t size = storage_file_size(file);
    bool ok = size > 0 && size <= GtaExploreMaxAssetSize;
    size_t rows = 0, capacity = 0;
    uint32_t offset = 0, start = 0;
    unsigned id = 0, digits = 0, tabs = 0;
    bool name = false;
    uint8_t chunk[256];
    while(ok && offset < size) {
        size_t n = size - offset > sizeof(chunk) ? sizeof(chunk) : (size_t)(size - offset);
        if(storage_file_read(file, chunk, n) != n) {
            ok = false;
            break;
        }
        for(size_t j = 0; j < n && ok; ++j, ++offset) {
            uint8_t ch = chunk[j];
            if(!ch || offset - start >= GTA_RECORD_MAX) {
                ok = false;
                break;
            }
            if(ch == '\n') {
                if(tabs != 3 || !name ||
                   id < (companion->vehicle_asset ? 1u : GtaExploreFirstId) ||
                   id >= GtaProgressBytes * 8u || rows >= GtaExploreMaxRows) {
                    ok = false;
                    break;
                }
                if(rows == capacity) {
                    size_t next = capacity ? capacity * 2u : 16u;
                    uint32_t* offsets =
                        realloc(companion->row_offsets, (next + 1u) * sizeof(uint32_t));
                    if(!offsets) {
                        ok = false;
                        break;
                    }
                    companion->row_offsets = offsets;
                    uint16_t* ids = realloc(companion->row_ids, next * sizeof(uint16_t));
                    if(!ids) {
                        ok = false;
                        break;
                    }
                    companion->row_ids = ids;
                    capacity = next;
                }
                companion->row_offsets[rows] = start;
                companion->row_ids[rows++] = (uint16_t)id;
                start = offset + 1u;
                id = digits = tabs = 0;
                name = false;
            } else if(ch == '\t') {
                if(++tabs > 3 || (tabs == 1 && !digits)) ok = false;
            } else if(!tabs) {
                if(ch < '0' || ch > '9' || ++digits > 4)
                    ok = false;
                else
                    id = id * 10u + ch - '0';
            } else if(tabs == 1)
                name = true;
        }
    }
    storage_file_free(file);
    if(!ok || !rows || start != size) {
        free(companion->row_offsets);
        free(companion->row_ids);
        companion->row_offsets = NULL;
        companion->row_ids = NULL;
        return false;
    }
    companion->row_offsets[rows] = start;
    // Keep only the used offsets and IDs after the streaming scan.
    uint32_t* offsets = realloc(companion->row_offsets, (rows + 1u) * sizeof(uint32_t));
    if(offsets) companion->row_offsets = offsets;
    uint16_t* ids = realloc(companion->row_ids, rows * sizeof(uint16_t));
    if(ids) companion->row_ids = ids;
    companion->explore_list.count = rows;
    return true;
}
static void gta_release_exploration(GtaCompanion* companion) {
    companion->guide_text = NULL;
    companion->wreck_picker = false;
    companion->wreck_index = 0;
    if(companion->list_view)
        with_view_model(
            companion->list_view, GtaListModel * model, { model->collection = NULL; }, false);
    companion->explore_section = NULL;
    companion->asset = NULL;
    companion->vehicle_asset = false;
    companion->selected_valid = false;
    gta_record_clear(&companion->selected_record);
    gta_url_links_release();
    free(companion->row_offsets);
    free(companion->row_ids);
    companion->row_offsets = NULL;
    companion->row_ids = NULL;
    companion->explore_list.count = 0;
}
static bool gta_prepare_vehicle(GtaCompanion* companion, GtaCollectionId id) {
    gta_release_exploration(companion);
    companion->explore_list = gta_collection_catalog[id];
    companion->explore_list.count = 0;
    companion->asset = gta_vehicle_assets[id];
    companion->vehicle_asset = true;
    return gta_load_explore_asset(companion, companion->asset);
}
static bool gta_prepare_exploration(GtaCompanion* companion, const GtaExploreSection* section) {
    if(companion->explore_section == section) return companion->explore_list.count != 0;
    gta_release_exploration(companion);
    companion->explore_section = section;
    companion->explore_list =
        (GtaCollection){.title = section->title, .checked_label = section->checked_label};
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
        return gta_prepare_vehicle(
                   companion,
                   source == GtaDispatchSourceMoses ? GtaCollectionMoses : GtaCollectionRandy) ?
                   &companion->explore_list :
                   NULL;
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
    File* index = storage_file_alloc(companion->storage);
    if(!index ||
       !storage_file_open(
           index, APP_ASSETS_PATH("catalogs/dispatch.tsv"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(index) storage_file_free(index);
        companion->dispatch_unavailable = true;
        return;
    }
    uint64_t size = storage_file_size(index);
    if(!size || size > GtaExploreMaxAssetSize) {
        storage_file_free(index);
        companion->dispatch_unavailable = true;
        return;
    }
    if(companion->dispatch_cursor >= size) companion->dispatch_cursor = 0;
    const uint32_t first = companion->dispatch_cursor;
    do {
        GtaDispatchTarget value;
        if(!gta_dispatch_read(index, &companion->dispatch_cursor, &value)) {
            companion->dispatch_unavailable = true;
            break;
        }
        if(companion->dispatch_cursor == size) companion->dispatch_cursor = 0;
        const GtaDispatchTarget* target = &value;
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
                if(companion->row_ids[i] != target->id) continue;
                const GtaCollectionEntry* entry = gta_read_selected(companion, i);
                if(entry && entry->location) {
                    companion->dispatch_entry = entry;
                    companion->dispatch_collection = collection;
                    storage_file_free(index);
                    return;
                }
            }
        }
        companion->dispatch_unavailable = true;
    } while(companion->dispatch_cursor != first);
    storage_file_free(index);
    gta_release_exploration(companion);
}

static void gta_ensure_list(GtaCompanion* companion) {
    if(companion->list_view) return;
    companion->list_view = view_alloc();
    view_set_context(companion->list_view, companion);
    view_allocate_model(companion->list_view, ViewModelTypeLocking, sizeof(GtaListModel));
    view_set_draw_callback(companion->list_view, gta_draw_list);
    view_set_input_callback(companion->list_view, gta_input_list);
    view_dispatcher_add_view(companion->dispatcher, companion->list_view_id, companion->list_view);
}
static void gta_ensure_detail(GtaCompanion* companion) {
    if(!companion->detail) {
        companion->detail = widget_alloc();
        view_dispatcher_add_view(
            companion->dispatcher, companion->detail_view_id, widget_get_view(companion->detail));
    }
    if(!companion->text) companion->text = furi_string_alloc();
}
void gta_companion_idle(GtaCompanion* companion) {
    if(!companion || companion->nfc_session) return;
    gta_release_exploration(companion);
    if(companion->detail) {
        view_dispatcher_remove_view(companion->dispatcher, companion->detail_view_id);
        widget_free(companion->detail);
        companion->detail = NULL;
    }
    if(companion->list_view) {
        view_dispatcher_remove_view(companion->dispatcher, companion->list_view_id);
        view_free(companion->list_view);
        companion->list_view = NULL;
    }
    if(companion->text) {
        furi_string_free(companion->text);
        companion->text = NULL;
    }
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
    companion->storage = furi_record_open(RECORD_STORAGE);
    gta_load_progress(companion);
    return companion;
}
void gta_companion_free(GtaCompanion* companion) {
    if(companion->nfc_session && companion->detail)
        view_dispatcher_switch_to_view(companion->dispatcher, companion->detail_view_id);
    gta_nfc_release(companion);
    gta_companion_idle(companion);
    furi_record_close(RECORD_STORAGE);
    free(companion);
}

static void gta_open_checklist(
    GtaCompanion* companion,
    const GtaCollection* list,
    size_t* selection,
    const char* status) {
    gta_ensure_list(companion);
    companion->list = list;
    companion->selection = selection;
    if(*selection >= list->count) *selection = 0;
    companion->list_status = status;
    companion->dispatch = false;
    companion->detail_visible = false;
    companion->save_error = false;
    gta_refresh_list(companion);
    view_dispatcher_switch_to_view(companion->dispatcher, companion->list_view_id);
    gta_ram_log("collection open");
}

void gta_companion_open_list(GtaCompanion* companion, GtaCollectionId collection) {
    const bool loaded = gta_prepare_vehicle(companion, collection);
    gta_open_checklist(
        companion,
        &companion->explore_list,
        &companion->selections[collection],
        loaded ? NULL : "Collection unavailable");
}

void gta_companion_open_exploration(GtaCompanion* companion, const GtaExploreSection* section) {
    companion->explore_selection = 0;
    const bool loaded = gta_prepare_exploration(companion, section);
    const char* status = section->asset ? (loaded ? NULL : "Collection unavailable") :
                                          "Locations pending";
    gta_open_checklist(companion, &companion->explore_list, &companion->explore_selection, status);
}

void gta_companion_open_money_glitch(GtaCompanion* companion, const char* text) {
    gta_release_exploration(companion);
    companion->dispatch = false;
    companion->links_visible = false;
    companion->guide_text = text;
    gta_show_detail(companion);
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
    if(companion->nfc_session) {
        if(event == GTA_NFC_BACK_EVENT || event == GTA_NFC_EXIT_EVENT)
            gta_nfc_return(companion, event == GTA_NFC_EXIT_EVENT);
        return true;
    }
    if(event == GTA_NFC_BACK_EVENT || event == GTA_NFC_EXIT_EVENT) return true;
    if(!companion->detail_visible) return false;
    if(companion->links_visible) {
        if(event == GtaLinkSource || event == GtaLinkRight)
            gta_emulate_link(companion, event == GtaLinkRight);
        return true;
    }
    if(companion->guide_text) {
        if(event == GtaDetailLinks) gta_open_links(companion);
        return true;
    }
    if(companion->wreck_picker) {
        const GtaCollectionEntry* entry = gta_selected_entry(companion);
        size_t count = 0;
        if(entry) gta_wreck_maps(entry->id, &count);
        if(event == GtaWreckEmulate && count)
            gta_open_links(companion);
        else if(event == GtaWreckPrev && companion->wreck_index > 0) {
            --companion->wreck_index;
            gta_render_detail(companion);
        } else if(event == GtaWreckNext && companion->wreck_index + 1 < count) {
            ++companion->wreck_index;
            gta_render_detail(companion);
        }
        return true;
    }
    if(event == GtaWreckPicker && !companion->dispatch) {
        const GtaCollectionEntry* entry = gta_selected_entry(companion);
        size_t count;
        if(entry && gta_wreck_maps(entry->id, &count)) {
            companion->wreck_picker = true;
            companion->wreck_index = 0;
            gta_render_detail(companion);
        }
        return true;
    }
    if(event == GtaDetailLinks) {
        gta_open_links(companion);
        return true;
    }
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
    if(companion->nfc_session) {
        gta_nfc_return(companion, false);
        return true;
    }
    if(companion->links_visible) {
        companion->links_visible = false;
        gta_render_detail(companion);
        return true;
    }
    if(companion->guide_text) {
        companion->guide_text = NULL;
        companion->detail_visible = false;
        return false;
    }
    if(companion->wreck_picker) {
        companion->wreck_picker = false;
        gta_render_detail(companion);
        return true;
    }
    if(companion->detail_visible && !companion->dispatch) {
        companion->detail_visible = false;
        widget_reset(companion->detail);
        gta_record_clear(&companion->selected_record);
        companion->selected_valid = false;
        gta_ensure_list(companion);
        gta_refresh_list(companion);
        view_dispatcher_switch_to_view(companion->dispatcher, companion->list_view_id);
        view_dispatcher_remove_view(companion->dispatcher, companion->detail_view_id);
        widget_free(companion->detail);
        companion->detail = NULL;
        furi_string_free(companion->text);
        companion->text = NULL;
        return true;
    }
    companion->detail_visible = false;
    companion->dispatch = false;
    companion->dispatch_entry = NULL;
    companion->dispatch_collection = NULL;
    if(companion->detail) widget_reset(companion->detail);
    gta_release_exploration(companion);
    return false;
}

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
#include <gui/modules/submenu.h>
#include <gui/view.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    GtaSaveMagic = 0x34415447, // GTA4: extended 8192-bit save
    GtaPreviousSaveMagic = 0x33415447, // GTA3: 4096-bit save
    GtaPreviousProgressBytes = 512,
    GtaLegacySaveMagic = 0x32415447, // GTA2: original 2048-bit save
    GtaLegacyProgressBytes = 256,
    GtaSaveSlots = 2,
    GtaListRows = 4,
    GtaDetailToggle = 0x400,
    GtaDispatchNext,
    GtaDetailLinks,
    GtaLinkSelect = 0x420,
    GtaLinkViewId = 0x1001,
    GtaRowOffsetBits = 19,
    GtaRowOffsetMask = (1u << GtaRowOffsetBits) - 1u,
    GtaAholFirstTypeId = 1376,
};

#define GTA_DATA_DIRECTORY EXT_PATH("apps_data/gta_vi_cheats")

// Keep both older save pairs untouched for recovery and downgrade.
static const char* const gta_save_paths[GtaSaveSlots] = {
    GTA_DATA_DIRECTORY "/progress3.a",
    GTA_DATA_DIRECTORY "/progress3.b",
};
static const char* const gta_previous_save_paths[GtaSaveSlots] = {
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
typedef struct {
    uint32_t magic;
    uint32_t generation;
    uint8_t checked[GtaPreviousProgressBytes];
    uint32_t checksum;
} GtaPreviousSave;
_Static_assert(sizeof(GtaPreviousSave) == 524, "Legacy GTA3 save layout changed");
_Static_assert(sizeof(GtaSave) == 1036, "GTA4 progress save layout changed");
_Static_assert(GTA_CATALOG_MAX_SIZE <= GtaRowOffsetMask, "Catalog end offset must fit in a row");
_Static_assert(
    GtaProgressBytes * 8u <= (1u << (32 - GtaRowOffsetBits)),
    "Progress ID must fit in a row");
_Static_assert(offsetof(GtaSave, checked) == 8, "GTA4 bitmap offset changed");
_Static_assert(offsetof(GtaPreviousSave, checked) == 8, "GTA3 bitmap offset changed");
_Static_assert(offsetof(GtaLegacySave, checked) == 8, "GTA2 bitmap offset changed");
_Static_assert(offsetof(GtaSave, checksum) == 1032, "GTA4 checksum offset changed");
_Static_assert(offsetof(GtaPreviousSave, checksum) == 520, "GTA3 checksum offset changed");
_Static_assert(offsetof(GtaLegacySave, checksum) == 264, "GTA2 checksum offset changed");

typedef struct {
    const GtaCollection* collection;
    size_t selected;
    size_t checked_count;
    struct {
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
    bool standout_view;
    uint32_t* rows; // ID and byte offset per row, followed by the end offset.
    uint32_t* standout_ends; // All view: end offset and source file per row.
    size_t standout_selections[GTA_STANDOUT_CATEGORY_COUNT + 1u];
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
    bool money_guide;
    size_t money_guide_index;
    char money_guide_label[64];
    Submenu* link_menu;
    size_t link_selected;
    FlipperApplication* nfc_module;
    const GtaNfcPluginApi* nfc_api;
    void* nfc_session;
    bool dispatch;
    bool save_error;
    bool progress_load_failed;
};

static uint32_t gta_row_offset(const GtaCompanion* companion, size_t row) {
    return companion->rows[row] & GtaRowOffsetMask;
}

static uint32_t gta_row_end(const GtaCompanion* companion, size_t row) {
    return companion->standout_ends ? companion->standout_ends[row] & GtaRowOffsetMask :
                                       gta_row_offset(companion, row + 1u);
}

static uint16_t gta_row_id(const GtaCompanion* companion, size_t row) {
    return (uint16_t)(companion->rows[row] >> GtaRowOffsetBits);
}

static uint8_t gta_entry_mask(uint16_t id) {
    return (uint8_t)(1u << (id % 8u));
}

static bool gta_is_checked(const uint8_t* checked, uint16_t id) {
    return (checked[id / 8u] & gta_entry_mask(id)) != 0;
}

/* The progress ranges are derived from active catalog IDs and shipped in TSV.
 * The save format, bit numbering and checked-state logic are unchanged. */
static bool gta_progress_range_id(const char* field, uint16_t* id) {
    if(!*field) return false;
    unsigned n = 0;
    while(*field) {
        if(*field < '0' || *field > '9') return false;
        n = n * 10u + (unsigned)(*field++ - '0');
        if(n >= GtaProgressBytes * 8u) return false;
    }
    if(!n) return false;
    *id = (uint16_t)n;
    return true;
}

bool gta_companion_progress_summary(
    const GtaCompanion* companion,
    size_t* total_items,
    size_t* completed_items) {
    if(!companion || !total_items || !completed_items || companion->progress_load_failed)
        return false;

    const size_t count = gta_record_count(companion->storage, "progress_ranges.tsv");
    bool ok = count > 0;
    GtaRecord row = {0};
    uint16_t previous = 0;
    size_t total = 0;
    size_t completed = 0;
    for(size_t i = 0; ok && i < count; ++i) {
        uint16_t first = 0, last = 0;
        ok = gta_record_read(companion->storage, "progress_ranges.tsv", i, 2, &row) &&
             gta_progress_range_id(row.field[0], &first) &&
             gta_progress_range_id(row.field[1], &last) && first > previous && last >= first;
        if(ok) {
            for(uint16_t id = first; id <= last; ++id) {
                ++total;
                if(gta_is_checked(companion->save.checked, id)) ++completed;
            }
            previous = last;
        }
        gta_record_clear(&row);
    }
    gta_record_index_release();
    if(!ok) return false;
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

// All three formats share their header/bitmap offsets; checksum is the final word.
static bool gta_read_save_record(
    Storage* storage,
    const char* path,
    GtaSave* record,
    size_t length,
    uint32_t magic) {
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    const bool read = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                      storage_file_size(file) == length &&
                      storage_file_read(file, record, length) == length;
    storage_file_free(file);
    if(!read || record->magic != magic) return false;
    uint32_t stored_checksum;
    memcpy(
        &stored_checksum,
        (const uint8_t*)record + length - sizeof(stored_checksum),
        sizeof(stored_checksum));
    return stored_checksum == gta_save_checksum(record, length - sizeof(stored_checksum));
}

static bool gta_generation_is_newer(uint32_t incoming, uint32_t current) {
    const uint32_t age = incoming - current;
    return age != 0 && age < UINT32_C(0x80000000);
}

static void gta_load_progress(GtaCompanion* companion) {
    memset(&companion->save, 0, sizeof(companion->save));
    companion->save.magic = GtaSaveMagic;
    companion->save_slot = GtaSaveSlots;

    // Prefer the newest supported format with a valid slot. Within a format,
    // compare generations with wraparound. Older files are only read, never written.
    static const struct {
        const char* const* paths;
        size_t length;
        size_t bitmap_bytes;
        uint32_t magic;
    } formats[] = {
        {gta_save_paths, sizeof(GtaSave), GtaProgressBytes, GtaSaveMagic},
        {gta_previous_save_paths,
         sizeof(GtaPreviousSave),
         GtaPreviousProgressBytes,
         GtaPreviousSaveMagic},
        {gta_legacy_save_paths, sizeof(GtaLegacySave), GtaLegacyProgressBytes, GtaLegacySaveMagic},
    };
    // A single heap buffer also accommodates the smaller legacy records.
    GtaSave* candidate = gta_try_alloc(sizeof(*candidate));
    if(!candidate) {
        companion->save_error = true;
        companion->progress_load_failed = true;
        return;
    }
    for(size_t format = 0; format < sizeof(formats) / sizeof(formats[0]); ++format) {
        bool found = false;
        for(size_t slot = 0; slot < GtaSaveSlots; ++slot) {
            if(!gta_read_save_record(
                   companion->storage,
                   formats[format].paths[slot],
                   candidate,
                   formats[format].length,
                   formats[format].magic))
                continue;
            if(!found ||
               gta_generation_is_newer(candidate->generation, companion->save.generation)) {
                companion->save.generation = candidate->generation;
                memcpy(companion->save.checked, candidate->checked, formats[format].bitmap_bytes);
                // The zeroed upper bitmap is preserved on legacy imports.
                if(format == 0) companion->save_slot = slot;
                found = true;
            }
        }
        if(found) break;
    }
    free(candidate);

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

    // Keep the 1036-byte record off the Flipper app task stack.
    GtaSave* next = gta_try_alloc(sizeof(*next));
    if(!next) return false;
    *next = companion->save;
    gta_toggle_checkmark(next->checked, id);
    ++next->generation;
    next->checksum = gta_save_checksum(next, offsetof(GtaSave, checksum));

    // Preserve the last valid GTA4 slot until a full replacement has synced.
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
static File* gta_open_row(GtaCompanion* companion, size_t row);
static bool
    gta_read_collection_label(GtaCompanion* companion, size_t row, char* name, size_t capacity) {
    if(row >= companion->explore_list.count || !capacity) return false;
    // The TSV index was validated on open. Read only the name prefix (not its description).
    File* file = gta_open_row(companion, row);
    if(!file) return false;
    uint32_t start = gta_row_offset(companion, row), end = gta_row_end(companion, row);
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
    const uint16_t id = gta_row_id(companion, row);
    if(strtoul(companion->selected_record.field[0], NULL, 10) != id) {
        gta_record_clear(&companion->selected_record);
        return NULL;
    }
    char** f = companion->selected_record.field;
    companion->selected_entry =
        (GtaCollectionEntry){id, f[1], *f[2] ? f[2] : NULL, *f[3] ? f[3] : NULL};
    companion->selected_row = row;
    companion->selected_valid = true;
    return &companion->selected_entry;
}
static const GtaCollectionEntry* gta_selected_entry(GtaCompanion* companion) {
    if(companion->money_guide) {
        companion->selected_entry =
            (GtaCollectionEntry){222, companion->money_guide_label, NULL, NULL};
        return &companion->selected_entry;
    }
    if(companion->dispatch) return companion->dispatch_entry;
    if(!companion->list || !companion->list->count) return NULL;
    return gta_read_selected(companion, *companion->selection);
}

static const GtaUrlLinks*
    gta_selected_links(const GtaCompanion* companion, const GtaCollectionEntry* entry) {
    if(!entry) return NULL;
    if(companion->money_guide) {
        /* The existing catalog already stores this warehouse pin under ID 222.
         * Keep the money guide's original single-map behavior. */
        GtaUrlLinks* links = gta_url_links_resolve(entry->id, NULL, NULL);
        if(links && links->count) {
            links->count = 1;
            links->choices[0].label = entry->name;
        }
        return links;
    }
    const GtaCollection* collection = companion->dispatch ? companion->dispatch_collection :
                                                            companion->list;
    return gta_url_links_resolve(
        entry->id,
        companion->explore_section ? companion->explore_section->asset : NULL,
        companion->standout_view ? gta_collection_catalog[GtaCollectionStandout].title :
                                   (collection ? collection->title : NULL));
}

static void gta_refresh_list(GtaCompanion* companion) {
    const GtaCollection* collection = companion->list;
    size_t checked_count = 0;
    for(size_t i = 0; i < collection->count; ++i) {
        if(gta_is_checked(companion->save.checked, gta_row_id(companion, i))) ++checked_count;
    }
    size_t first = *companion->selection / GtaListRows * GtaListRows;
    GtaListModel page = {0};
    for(size_t row = 0; row < GtaListRows && first + row < collection->count; ++row) {
        size_t i = first + row;
        page.rows[row].checked = gta_is_checked(companion->save.checked, gta_row_id(companion, i));
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
    } else if(button == GuiButtonTypeLeft && companion->dispatch) {
        view_dispatcher_send_custom_event(companion->dispatcher, GtaDispatchNext);
    }
}

static void gta_render_detail(GtaCompanion* companion) {
    Widget* detail = companion->detail;
    const GtaCollectionEntry* entry = gta_selected_entry(companion);
    widget_reset(detail);

    if(companion->money_guide) {
        GtaRecord guide = {0};
        bool loaded = gta_record_read(
                          companion->storage, GTA_GLITCH_ASSET, companion->money_guide_index, 5, &guide) &&
                      !strcmp(guide.field[1], GTA_GUIDE_MONEY_MAP_MODE) && *guide.field[3];
        snprintf(
            companion->money_guide_label,
            sizeof(companion->money_guide_label),
            "%s",
            loaded ? guide.field[3] : "Map");
        widget_add_text_scroll_element(
            detail, 0, 0, 128, 51, loaded ? guide.field[2] : "Guide data unavailable.");
        /* The widget copies text, as in the normal collection renderer. */
        gta_record_clear(&guide);
        gta_record_index_release();
        widget_add_button_element(detail, GuiButtonTypeRight, "Map", gta_detail_button, companion);
        return;
    }
    FuriString* text = furi_string_alloc();
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
        const size_t link_count = links ? links->count : 0;
        if(link_count == 1 && links->choices[0].map_index)
            furi_string_cat_printf(text, "NFC: shared guide/index\n");
        else if(link_count > 1)
            furi_string_cat_printf(text, "NFC: %u links\n", (unsigned)link_count);
        gta_url_links_release();
        furi_string_cat_printf(
            text, "\n%s", entry->location ? entry->location : "Location not listed.");
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
        }
        if(link_count) {
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
    furi_string_free(text);
}

static void gta_link_button(void* context, uint32_t index) {
    GtaCompanion* companion = context;
    view_dispatcher_send_custom_event(companion->dispatcher, index);
}

/* Free only after switching away from the submenu view. */
static void gta_link_menu_release(GtaCompanion* companion) {
    if(!companion->link_menu) return;
    view_dispatcher_remove_view(companion->dispatcher, GtaLinkViewId);
    submenu_free(companion->link_menu);
    companion->link_menu = NULL;
}

static void
    gta_show_resolved_links(GtaCompanion* companion, const GtaUrlLinks* links, const char* error) {
    if(!links || !links->count) {
        gta_url_links_release();
        companion->links_visible = false;
        view_dispatcher_switch_to_view(companion->dispatcher, companion->detail_view_id);
        gta_link_menu_release(companion);
        gta_render_detail(companion);
        return;
    }
    companion->links_visible = true;
    if(!companion->link_menu) {
        companion->link_menu = submenu_alloc();
        view_dispatcher_add_view(
            companion->dispatcher, GtaLinkViewId, submenu_get_view(companion->link_menu));
    }
    submenu_reset(companion->link_menu);
    submenu_set_header(companion->link_menu, error ? error : "NFC Links - OK");
    for(size_t i = 0; i < links->count; ++i) {
        char label[GTA_URL_LABEL_MAX];
        gta_url_choice_label(links, i, label, sizeof(label));
        submenu_add_item(
            companion->link_menu, label, GtaLinkSelect + (uint32_t)i, gta_link_button, companion);
    }
    if(companion->link_selected >= links->count) companion->link_selected = 0;
    submenu_set_selected_item(
        companion->link_menu, GtaLinkSelect + (uint32_t)companion->link_selected);
    gta_url_links_release();
    view_dispatcher_switch_to_view(companion->dispatcher, GtaLinkViewId);
    widget_reset(companion->detail);
}

static void gta_show_links(GtaCompanion* companion, const char* error) {
    const GtaCollectionEntry* entry = gta_selected_entry(companion);
    gta_show_resolved_links(companion, gta_selected_links(companion, entry), error);
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

static void gta_emulate_resolved_link(
    GtaCompanion* companion,
    const GtaCollectionEntry* entry,
    const GtaUrlLinks* links,
    size_t index) {
    if(!entry || !links || index >= links->count || companion->nfc_module) {
        gta_url_links_release();
        return;
    }
    const GtaUrlChoice choice = links->choices[index];
    const bool has_alternates = links->count > 1;
    if(strlen(choice.url) >= GTA_URL_MAX) {
        gta_show_links(companion, "Link unavailable");
        return;
    }
    if(!storage_file_exists(companion->storage, GTA_NFC_PATH)) {
        FURI_LOG_E("GTA NFC", "Missing embedded plugin: %s", GTA_NFC_PATH);
        gta_show_links(companion, "NFC plugin missing. Reinstall app.");
        return;
    }
    const size_t url_size = strlen(choice.url) + 1u;
    char* url = gta_try_alloc(url_size);
    if(!url) {
        gta_show_links(companion, "Not enough memory.");
        return;
    }
    memcpy(url, choice.url, url_size);
    uint16_t id = entry->id;
    companion->link_selected = index;
    gta_url_links_release();
    if(companion->link_menu) {
        view_dispatcher_switch_to_view(companion->dispatcher, companion->detail_view_id);
        gta_link_menu_release(companion);
    }
    companion->selected_valid = false;
    gta_record_clear(&companion->selected_record);
    if(companion->dispatch) companion->dispatch_entry = NULL;
    widget_reset(companion->detail);
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
    companion->nfc_session =
        companion->nfc_api->open(companion->dispatcher, url, id, choice.kind, has_alternates);
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

static void gta_emulate_link(GtaCompanion* companion, size_t index) {
    const GtaCollectionEntry* entry = gta_selected_entry(companion);
    gta_emulate_resolved_link(companion, entry, gta_selected_links(companion, entry), index);
}

static void gta_nfc_return(GtaCompanion* companion, bool exit, bool show_links) {
    if(!companion->nfc_session) return;
    companion->nfc_api->stop(companion->nfc_session);
    GtaLoadingHandle* loading = exit ? NULL : gta_loading_show();
    // Switch away before removing the plugin view, then rebuild the previous
    // detail or selector only after the NFC module has been released.
    view_dispatcher_switch_to_view(companion->dispatcher, companion->detail_view_id);
    gta_nfc_release(companion);
    if(!exit && companion->dispatch)
        companion->dispatch_entry = gta_read_selected(companion, companion->selected_row);
    if(!exit) {
        if(show_links) {
            gta_show_links(companion, NULL);
        } else {
            companion->links_visible = false;
            // Dispatch and the money guide have no originating checklist view.
            if(companion->dispatch || companion->money_guide)
                gta_render_detail(companion);
            else
                gta_companion_back(companion);
        }
    }
    gta_loading_hide(loading);
    gta_ram_log("NFC released");
    if(exit) view_dispatcher_stop(companion->dispatcher);
}

static void gta_open_links(GtaCompanion* companion) {
    const GtaCollectionEntry* entry = gta_selected_entry(companion);
    const GtaUrlLinks* links = gta_selected_links(companion, entry);
    const size_t count = links ? links->count : 0;
    companion->link_selected = 0;
    companion->links_visible = false;
    if(count)
        gta_emulate_resolved_link(companion, entry, links, 0);
    else
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
        companion->save_error = !gta_save_checkmark(companion, gta_row_id(companion, *selected));
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
static File* gta_open_tsv_asset(GtaCompanion* companion, const char* asset) {
    if(!asset) return NULL;
    char path[96];
    int n = snprintf(
        path,
        sizeof(path),
        APP_ASSETS_PATH("%s/%s"),
        companion->vehicle_asset ? "catalogs" : "collections",
        asset);
    if(n < 0 || (size_t)n >= sizeof(path)) return NULL;
    File* f = storage_file_alloc(companion->storage);
    if(!f) return NULL;
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return NULL;
    }
    return f;
}
static File* gta_open_tsv(GtaCompanion* companion) {
    return gta_open_tsv_asset(companion, companion->asset);
}
static File* gta_open_row(GtaCompanion* companion, size_t row) {
    if(!companion->standout_ends) return gta_open_tsv(companion);
    size_t source = companion->standout_ends[row] >> GtaRowOffsetBits;
    if(source >= GTA_STANDOUT_CATEGORY_COUNT) return NULL;
    return gta_open_tsv_asset(companion, gta_standout_categories[source].asset);
}
static bool gta_read_collection_row(GtaCompanion* companion, size_t row, GtaRecord* r) {
    if(row >= companion->explore_list.count) return false;
    gta_record_clear(r);
    uint32_t start = gta_row_offset(companion, row), end = gta_row_end(companion, row);
    if(end <= start || end - start > GTA_RECORD_MAX) return false;
    File* f = gta_open_row(companion, row);
    if(!f) return false;
    size_t n = end - start;
    r->data = gta_try_alloc(n);
    bool ok = r->data && storage_file_seek(f, start, true) &&
              storage_file_read(f, r->data, n) == n && r->data[n - 1] == '\n';
    storage_file_free(f);
    if(ok) ok = gta_record_decode(r, n, 4, true) && *r->field[1];
    if(!ok) gta_record_clear(r);
    return ok;
}
static bool gta_load_explore_asset(GtaCompanion* companion, const char* asset) {
    companion->asset = asset;
    File* file = gta_open_tsv(companion);
    if(!file) return false;
    const uint64_t size = storage_file_size(file);
    const size_t count = gta_tsv_row_count(file);
    uint32_t* index = count ? gta_try_alloc((count + 1u) * sizeof(*index)) : NULL;
    bool ok = index && storage_file_size(file) == size && storage_file_seek(file, 0, true);
    size_t rows = 0;
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
                   id >= GtaProgressBytes * 8u || rows >= count) {
                    ok = false;
                    break;
                }
                index[rows++] = ((uint32_t)id << GtaRowOffsetBits) | start;
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
    if(!ok || rows != count || start != size) {
        free(index);
        return false;
    }
    index[rows] = start;
    companion->rows = index;
    companion->explore_list.count = rows;
    return true;
}
static void gta_release_exploration(GtaCompanion* companion) {
    companion->money_guide = false;
    companion->money_guide_index = 0;
    companion->money_guide_label[0] = 0;
    companion->links_visible = false;
    companion->link_selected = 0;
    if(companion->list_view)
        with_view_model(
            companion->list_view, GtaListModel * model, { model->collection = NULL; }, false);
    companion->explore_section = NULL;
    companion->asset = NULL;
    companion->vehicle_asset = false;
    companion->standout_view = false;
    companion->selected_valid = false;
    gta_record_clear(&companion->selected_record);
    gta_url_links_release();
    free(companion->rows);
    companion->rows = NULL;
    free(companion->standout_ends);
    companion->standout_ends = NULL;
    companion->explore_list.count = 0;
}
/* All Standouts is a virtual list. The small ID-only order file defines alphabetical
 * order; every row's source/offset is resolved from the actual category at open time.
 * Reject missing, duplicate or stale IDs instead of displaying mismatched checkmarks. */
static bool gta_load_all_standouts(GtaCompanion* companion) {
    File* order = gta_open_tsv_asset(companion, "standouts_order.tsv");
    if(!order) return false;
    const size_t count = gta_tsv_row_count(order);
    uint32_t* rows = count ? gta_try_alloc(count * sizeof(*rows)) : NULL;
    uint32_t* ends = count ? gta_try_alloc(count * sizeof(*ends)) : NULL;
    bool ok = rows && ends && storage_file_seek(order, 0, true);
    size_t used = 0;
    for(size_t source = 0; ok && source < GTA_STANDOUT_CATEGORY_COUNT; ++source) {
        ok = gta_load_explore_asset(companion, gta_standout_categories[source].asset);
        if(ok && companion->explore_list.count <= count - used) {
            for(size_t i = 0; i < companion->explore_list.count; ++i) {
                rows[used] = companion->rows[i];
                ends[used++] = ((uint32_t)source << GtaRowOffsetBits) |
                               gta_row_offset(companion, i + 1u);
            }
        } else ok = false;
        free(companion->rows);
        companion->rows = NULL;
        companion->explore_list.count = 0;
    }
    if(used != count) ok = false;
    size_t out = 0;
    unsigned id = 0, digits = 0;
    uint8_t chunk[128];
    uint64_t remaining = storage_file_size(order);
    while(ok && remaining) {
        size_t n = remaining > sizeof(chunk) ? sizeof(chunk) : (size_t)remaining;
        if(storage_file_read(order, chunk, n) != n) { ok = false; break; }
        remaining -= n;
        for(size_t i = 0; ok && i < n; ++i) {
            uint8_t ch = chunk[i];
            if(ch == '\n') {
                if(!digits || !id || id >= GtaProgressBytes * 8u || out >= count) {
                    ok = false; break;
                }
                for(size_t previous = 0; previous < out; ++previous) {
                    if((rows[previous] >> GtaRowOffsetBits) == id) {
                        ok = false;
                        break;
                    }
                }
                if(!ok) break;
                size_t match = out;
                while(match < count && (rows[match] >> GtaRowOffsetBits) != id) ++match;
                if(match == count) { ok = false; break; }
                uint32_t swap = rows[out]; rows[out] = rows[match]; rows[match] = swap;
                swap = ends[out]; ends[out] = ends[match]; ends[match] = swap;
                ++out; id = digits = 0;
            } else if(ch >= '0' && ch <= '9' && digits < 4u) {
                id = id * 10u + ch - '0'; ++digits;
            } else ok = false;
        }
    }
    storage_file_free(order);
    if(!ok || out != count || digits) {
        free(rows); free(ends); return false;
    }
    companion->rows = rows;
    companion->standout_ends = ends;
    companion->explore_list.count = count;
    companion->asset = NULL;
    return true;
}

static bool gta_prepare_vehicle(GtaCompanion* companion, GtaCollectionId id) {
    gta_release_exploration(companion);
    companion->explore_list = gta_collection_catalog[id];
    companion->explore_list.count = 0;
    companion->asset = gta_vehicle_assets[id];
    companion->vehicle_asset = true;
    companion->standout_view = id == GtaCollectionStandout;
    return id == GtaCollectionStandout ? gta_load_all_standouts(companion) :
                                         gta_load_explore_asset(companion, companion->asset);
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
    if(!size || size > GTA_CATALOG_MAX_SIZE) {
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
                if(gta_row_id(companion, i) != target->id) continue;
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
}
void gta_companion_idle(GtaCompanion* companion) {
    if(!companion || companion->nfc_session) return;
    gta_link_menu_release(companion);
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

void gta_companion_open_standouts(GtaCompanion* companion, size_t category) {
    if(category > GTA_STANDOUT_CATEGORY_COUNT) return;
    bool loaded;
    if(!category) {
        loaded = gta_prepare_vehicle(companion, GtaCollectionStandout);
    } else {
        gta_release_exploration(companion);
        const GtaStandoutCategory* item = &gta_standout_categories[category - 1u];
        companion->explore_list = (GtaCollection){.title = item->title, .checked_label = "Acquired"};
        companion->vehicle_asset = true;
        companion->standout_view = true;
        loaded = gta_load_explore_asset(companion, item->asset);
    }
    gta_open_checklist(companion, &companion->explore_list,
                       &companion->standout_selections[category],
                       loaded ? NULL : "Collection unavailable");
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

/* Open a mapped location from a related walkthrough. Keep the normal collection
 * selection and NFC return path, rather than duplicating the NFC lifecycle. */
void gta_companion_open_exploration_item(
    GtaCompanion* companion,
    const GtaExploreSection* section,
    uint16_t id) {
    gta_companion_open_exploration(companion, section);
    for(size_t i = 0; i < companion->explore_list.count; ++i) {
        if(gta_row_id(companion, i) != id) continue;
        companion->explore_selection = i;
        gta_refresh_list(companion);
        gta_show_detail(companion);
        return;
    }
}

void gta_companion_open_money_glitch(GtaCompanion* companion, size_t index) {
    gta_release_exploration(companion);
    companion->dispatch = false;
    companion->links_visible = false;
    companion->money_guide = true;
    companion->money_guide_index = index;
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
        if(event == GTA_NFC_BACK_EVENT || event == GTA_NFC_EXIT_EVENT ||
           event == GTA_NFC_LINKS_EVENT)
            gta_nfc_return(companion, event == GTA_NFC_EXIT_EVENT, event == GTA_NFC_LINKS_EVENT);
        return true;
    }
    if(event == GTA_NFC_BACK_EVENT || event == GTA_NFC_EXIT_EVENT || event == GTA_NFC_LINKS_EVENT)
        return true;
    if(!companion->detail_visible) return false;
    if(companion->links_visible) {
        if(event >= GtaLinkSelect && event < GtaLinkSelect + GTA_URL_CHOICES_MAX)
            gta_emulate_link(companion, event - GtaLinkSelect);
        return true;
    }
    if(companion->money_guide) {
        if(event == GtaDetailLinks) gta_open_links(companion);
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
        gta_nfc_return(companion, false, false);
        return true;
    }
    if(companion->links_visible) {
        companion->links_visible = false;
        view_dispatcher_switch_to_view(companion->dispatcher, companion->detail_view_id);
        gta_link_menu_release(companion);
        if(companion->dispatch || companion->money_guide) {
            gta_render_detail(companion);
            return true;
        }
        // Fall through to the originating checklist with its selection intact.
    }
    if(companion->money_guide) {
        if(companion->detail) widget_reset(companion->detail);
        gta_release_exploration(companion);
        companion->detail_visible = false;
        return false;
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

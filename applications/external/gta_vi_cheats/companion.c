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
    GtaSaveMagic = 0x31415447,
    GtaSaveSlots = 2,
    GtaListRows = 4,
    GtaDetailToggle = 0x400,
    GtaDispatchNext,
};

#define GTA_DATA_DIRECTORY EXT_PATH("apps_data/gta_vi_cheats")

static const char* const gta_save_paths[GtaSaveSlots] = {
    GTA_DATA_DIRECTORY "/progress.a",
    GTA_DATA_DIRECTORY "/progress.b",
};

typedef struct {
    uint32_t magic;
    uint32_t generation;
    uint8_t checked[GtaProgressBytes];
    uint32_t checksum;
} GtaSave;

_Static_assert(sizeof(GtaSave) == 12 + GtaProgressBytes, "Progress record must stay stable");

typedef struct {
    const GtaCollection* collection;
    size_t selected;
    size_t checked_count;
    uint8_t checked[GtaProgressBytes];
    bool save_error;
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
    GtaCollectionId collection;
    size_t selections[GtaCollectionCount];
    const GtaCollectionEntry* dispatch_entry;
    GtaCollectionId dispatch_collection;
    size_t dispatch_cursor;
    bool detail_visible;
    bool dispatch;
    bool save_error;
};

static uint8_t gta_entry_mask(uint16_t id) {
    return (uint8_t)(1u << (id % 8u));
}

static bool gta_is_checked(const uint8_t* checked, uint16_t id) {
    return (checked[id / 8u] & gta_entry_mask(id)) != 0;
}

static uint32_t gta_save_checksum(const GtaSave* save) {
    const uint8_t* bytes = (const uint8_t*)save;
    uint32_t checksum = UINT32_C(2166136261);
    for(size_t i = 0; i < offsetof(GtaSave, checksum); ++i) {
        checksum ^= bytes[i];
        checksum *= UINT32_C(16777619);
    }
    return checksum;
}

static bool gta_read_save(Storage* storage, const char* path, GtaSave* save) {
    File* file = storage_file_alloc(storage);
    bool valid = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        valid = storage_file_size(file) == sizeof(*save) &&
                storage_file_read(file, save, sizeof(*save)) == sizeof(*save) &&
                save->magic == GtaSaveMagic && save->checksum == gta_save_checksum(save);
    }
    storage_file_free(file);
    return valid;
}

static void gta_load_progress(GtaCompanion* companion) {
    companion->save.magic = GtaSaveMagic;
    companion->save_slot = GtaSaveSlots;
    for(size_t slot = 0; slot < GtaSaveSlots; ++slot) {
        GtaSave candidate;
        if(!gta_read_save(companion->storage, gta_save_paths[slot], &candidate)) continue;
        const uint32_t age = candidate.generation - companion->save.generation;
        if(companion->save_slot == GtaSaveSlots || (age && age < UINT32_C(0x80000000))) {
            companion->save = candidate;
            companion->save_slot = slot;
        }
    }
}

static bool gta_save_checkmark(GtaCompanion* companion, uint16_t id) {
    Storage* storage = companion->storage;
    if(!storage_simply_mkdir(storage, EXT_PATH("apps_data")) ||
       !storage_simply_mkdir(storage, GTA_DATA_DIRECTORY)) {
        return false;
    }

    GtaSave next = companion->save;
    next.checked[id / 8u] ^= gta_entry_mask(id);
    ++next.generation;
    next.checksum = gta_save_checksum(&next);

    // Keep the last complete save available until the new slot is written and synced.
    const size_t slot = companion->save_slot == 0 ? 1 : 0;
    const char* path = gta_save_paths[slot];
    File* file = storage_file_alloc(storage);
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

    companion->save = next;
    companion->save_slot = slot;
    return true;
}

static void gta_draw_label(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    const char* text) {
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
    const GtaCollectionId id = companion->collection;
    return &gta_collection_catalog[id].entries[companion->selections[id]];
}

static void gta_refresh_list(GtaCompanion* companion) {
    const GtaCollectionId id = companion->collection;
    const GtaCollection* collection = &gta_collection_catalog[id];
    size_t checked_count = 0;
    for(size_t i = 0; i < collection->count; ++i) {
        if(gta_is_checked(companion->save.checked, collection->entries[i].id)) ++checked_count;
    }

    with_view_model(
        companion->list_view,
        GtaListModel * model,
        {
            model->collection = collection;
            model->selected = companion->selections[id];
            model->checked_count = checked_count;
            memcpy(model->checked, companion->save.checked, sizeof(model->checked));
            model->save_error = companion->save_error;
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

    if(entry) {
        const GtaCollectionId id =
            companion->dispatch ? companion->dispatch_collection : companion->collection;
        const GtaCollection* collection = &gta_collection_catalog[id];
        const bool checked = gta_is_checked(companion->save.checked, entry->id);
        if(companion->dispatch) furi_string_cat_printf(text, "Dispatch / %s\n", collection->title);
        furi_string_cat_printf(
            text, "%s\n%s\n", entry->name, checked ? collection->checked_label : "Unchecked");
        if(companion->save_error) furi_string_cat_printf(text, "SD save failed. Try again.\n");
        furi_string_cat_printf(
            text, "\n%s", entry->location ? entry->location : "Location unconfirmed.");
        if(entry->notes) furi_string_cat_printf(text, "\n\n%s", entry->notes);
        widget_add_button_element(
            detail, GuiButtonTypeCenter, checked ? "Undo" : "Check", gta_detail_button, companion);
        if(companion->dispatch) {
            widget_add_button_element(detail, GuiButtonTypeRight, "Next", gta_detail_button, companion);
        }
    } else {
        furi_string_cat_printf(text, "Dispatch\n\nNo unchecked targets with a location tip.");
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
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    const GtaCollectionId id = companion->collection;
    size_t* selected = &companion->selections[id];
    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        if(event->key == InputKeyUp && *selected > 0) --*selected;
        if(event->key == InputKeyDown && *selected + 1 < gta_collection_catalog[id].count) {
            ++*selected;
        }
        with_view_model(
            companion->list_view,
            GtaListModel * model,
            { model->selected = *selected; },
            true);
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

static void gta_dispatch_next(GtaCompanion* companion) {
    const GtaCollection* moses = &gta_collection_catalog[GtaCollectionMoses];
    const GtaCollection* randy = &gta_collection_catalog[GtaCollectionRandy];
    const size_t total = moses->count + randy->count;
    companion->dispatch_entry = NULL;
    for(size_t tried = 0; tried < total; ++tried) {
        const size_t index = companion->dispatch_cursor;
        companion->dispatch_cursor = (index + 1) % total;
        const bool is_moses = index < moses->count;
        const GtaCollectionEntry* entry =
            is_moses ? &moses->entries[index] : &randy->entries[index - moses->count];
        if(entry->location && !gta_is_checked(companion->save.checked, entry->id)) {
            companion->dispatch_entry = entry;
            companion->dispatch_collection = is_moses ? GtaCollectionMoses : GtaCollectionRandy;
            break;
        }
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
    view_free(companion->list_view);
    furi_string_free(companion->text);
    furi_record_close(RECORD_STORAGE);
    free(companion);
}

void gta_companion_open_list(GtaCompanion* companion, GtaCollectionId collection) {
    companion->collection = collection;
    companion->dispatch = false;
    companion->detail_visible = false;
    companion->save_error = false;
    gta_refresh_list(companion);
    view_dispatcher_switch_to_view(companion->dispatcher, companion->list_view_id);
}

void gta_companion_open_dispatch(GtaCompanion* companion) {
    companion->dispatch = true;
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
    return false;
}

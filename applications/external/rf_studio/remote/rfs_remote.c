#include "rfs_remote.h"
#include "../storage/rfs_storage.h"

#include <flipper_format/flipper_format.h>
#include <toolbox/path.h>

#define REMOTE_FILETYPE "RF Studio Remote"
#define NO_SIGNAL       "none"

static const char* const category_names[RfsCategoryCount] = {
    "General",
    "Lighting",
    "Fans",
    "Wireless Relays",
    "Smart Plugs",
    "Home Automation",
    "Access Devices",
    "Other",
};

static const char* const category_short[RfsCategoryCount] = {
    "Gen",
    "Lgt",
    "Fan",
    "Rly",
    "Plg",
    "Hom",
    "Acc",
    "Oth",
};

const char* rfs_category_name(RfsCategory category) {
    return category < RfsCategoryCount ? category_names[category] : "Other";
}

const char* rfs_category_short(RfsCategory category) {
    return category < RfsCategoryCount ? category_short[category] : "Oth";
}

static uint8_t rfs_category_parse(const char* name) {
    for(uint8_t i = 0; i < RfsCategoryCount; i++) {
        if(strcmp(category_names[i], name) == 0) return i;
    }
    return RfsCategoryOther;
}

static uint8_t rfs_radio_mode_parse(const char* name) {
    for(uint8_t i = 0; i < RfsRadioModeCount; i++) {
        if(strcmp(rfs_radio_mode_name(i), name) == 0) return i;
    }
    return RfsRadioModeAuto;
}

void rfs_remote_init(RfsRemote* remote) {
    memset(remote, 0, sizeof(RfsRemote));
}

void rfs_remote_clear(RfsRemote* remote) {
    free(remote->buttons);
    rfs_remote_init(remote);
}

void rfs_remote_new(Storage* storage, RfsRemote* remote, const char* name) {
    rfs_remote_clear(remote);
    rfs_storage_new_id(storage, 'R', RFS_REMOTES_DIR, RFS_REMOTE_EXT, remote->id);
    strlcpy(remote->name, name, sizeof(remote->name));
    remote->category = RfsCategoryGeneral;
    remote->created = rfs_storage_now();
    remote->updated = remote->created;
    remote->preferred_radio = RfsRadioModeAuto;
}

void rfs_remote_path(const char* id, FuriString* out) {
    furi_string_printf(out, "%s/%s%s", RFS_REMOTES_DIR, id, RFS_REMOTE_EXT);
}

static bool rfs_remote_reserve(RfsRemote* remote, size_t needed) {
    if(needed <= remote->capacity) return true;
    size_t capacity = remote->capacity ? remote->capacity * 2 : 8;
    while(capacity < needed)
        capacity *= 2;
    size_t grow = (capacity - remote->capacity) * sizeof(RfsButton);
    /* Practical limit comes from available memory, not from a fixed button count. */
    if(memmgr_get_free_heap() < grow + RFS_MIN_FREE_HEAP) return false;
    remote->buttons = realloc(remote->buttons, capacity * sizeof(RfsButton));
    remote->capacity = capacity;
    return true;
}

static bool
    rfs_read_into(FlipperFormat* ff, const char* key, FuriString* tmp, char* out, size_t size) {
    flipper_format_rewind(ff);
    if(!flipper_format_read_string(ff, key, tmp)) return false;
    strlcpy(out, furi_string_get_cstr(tmp), size);
    return true;
}

static bool rfs_read_u32(FlipperFormat* ff, const char* key, uint32_t* out) {
    flipper_format_rewind(ff);
    return flipper_format_read_uint32(ff, key, out, 1);
}

bool rfs_remote_load_path(Storage* storage, const char* path, RfsRemote* remote, FuriString* error) {
    rfs_remote_clear(remote);
    rfs_storage_recover(storage, path);

    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);
    FuriString* tmp = furi_string_alloc();
    bool ok = false;
    if(error) furi_string_reset(error);

    do {
        if(!flipper_format_buffered_file_open_existing(ff, path)) {
            if(error) furi_string_set(error, "Remote file not found");
            break;
        }
        uint32_t version = 0;
        if(!flipper_format_read_header(ff, tmp, &version) ||
           furi_string_cmp_str(tmp, REMOTE_FILETYPE) != 0) {
            if(error) furi_string_set(error, "Not an RF Studio remote");
            break;
        }
        if(version > RFS_REMOTE_SCHEMA_VERSION) {
            if(error) furi_string_set(error, "Made by a newer RF Studio");
            break;
        }
        /* Schema 1 had no extra keys RF Studio relies on; any missing key keeps its default
         * and the file is rewritten as schema 2 on the next save (Section 12.5). */

        if(!rfs_read_into(ff, "ID", tmp, remote->id, sizeof(remote->id))) {
            FuriString* base = furi_string_alloc_set(path);
            FuriString* name = furi_string_alloc();
            path_extract_filename(base, name, true);
            strlcpy(remote->id, furi_string_get_cstr(name), sizeof(remote->id));
            furi_string_free(name);
            furi_string_free(base);
        }
        if(!rfs_read_into(ff, "Name", tmp, remote->name, sizeof(remote->name))) {
            strlcpy(remote->name, "Unnamed", sizeof(remote->name));
        }
        rfs_read_into(ff, "Description", tmp, remote->description, sizeof(remote->description));
        flipper_format_rewind(ff);
        if(flipper_format_read_string(ff, "Category", tmp))
            remote->category = rfs_category_parse(furi_string_get_cstr(tmp));
        flipper_format_rewind(ff);
        flipper_format_read_bool(ff, "Favorite", &remote->favorite, 1);
        rfs_read_u32(ff, "Created", &remote->created);
        rfs_read_u32(ff, "Updated", &remote->updated);
        rfs_read_u32(ff, "Last_used", &remote->last_used);
        flipper_format_rewind(ff);
        if(flipper_format_read_string(ff, "Preferred_radio", tmp))
            remote->preferred_radio = rfs_radio_mode_parse(furi_string_get_cstr(tmp));
        rfs_read_u32(ff, "Preferred_frequency", &remote->preferred_frequency);
        rfs_read_into(
            ff,
            "Preferred_preset",
            tmp,
            remote->preferred_preset,
            sizeof(remote->preferred_preset));

        uint32_t count = 0;
        flipper_format_rewind(ff);
        if(!flipper_format_read_uint32(ff, "Buttons", &count, 1)) count = 0;

        /* Buttons follow the count in display order. Keys repeat, so read sequentially. */
        ok = true;
        for(uint32_t i = 0; i < count; i++) {
            RfsButton button;
            memset(&button, 0, sizeof(button));
            bool read = flipper_format_read_string(ff, "Button_id", tmp);
            if(read) strlcpy(button.id, furi_string_get_cstr(tmp), sizeof(button.id));
            read = read && flipper_format_read_string(ff, "Button_name", tmp);
            if(read) strlcpy(button.name, furi_string_get_cstr(tmp), sizeof(button.name));
            read = read && flipper_format_read_string(ff, "Button_signal", tmp);
            if(read && furi_string_cmp_str(tmp, NO_SIGNAL) != 0)
                strlcpy(button.signal_id, furi_string_get_cstr(tmp), sizeof(button.signal_id));
            read = read && flipper_format_read_uint32(ff, "Button_created", &button.created, 1);
            read = read && flipper_format_read_uint32(ff, "Button_updated", &button.updated, 1);
            if(!read) {
                if(error)
                    furi_string_printf(
                        error, "Read %lu of %lu buttons", (unsigned long)i, (unsigned long)count);
                break;
            }
            if(!rfs_remote_reserve(remote, remote->count + 1)) {
                if(error) furi_string_set(error, "Not enough memory for all buttons");
                break;
            }
            remote->buttons[remote->count++] = button;
        }
    } while(false);

    furi_string_free(tmp);
    flipper_format_free(ff);
    return ok;
}

bool rfs_remote_load(Storage* storage, const char* id, RfsRemote* remote, FuriString* error) {
    FuriString* path = furi_string_alloc();
    rfs_remote_path(id, path);
    bool ok = rfs_remote_load_path(storage, furi_string_get_cstr(path), remote, error);
    furi_string_free(path);
    return ok;
}

bool rfs_remote_save_path(Storage* storage, const char* path, const RfsRemote* remote) {
    FlipperFormat* ff = rfs_storage_atomic_begin(storage, path);
    bool ok = ff != NULL;
    if(ok) {
        uint32_t count = remote->count;
        ok = flipper_format_write_header_cstr(ff, REMOTE_FILETYPE, RFS_REMOTE_SCHEMA_VERSION);
        ok = ok && flipper_format_write_string_cstr(ff, "ID", remote->id);
        ok = ok && flipper_format_write_string_cstr(ff, "Name", remote->name);
        if(remote->description[0])
            ok = ok && flipper_format_write_string_cstr(ff, "Description", remote->description);
        ok = ok &&
             flipper_format_write_string_cstr(ff, "Category", rfs_category_name(remote->category));
        ok = ok && flipper_format_write_bool(ff, "Favorite", &remote->favorite, 1);
        ok = ok && flipper_format_write_uint32(ff, "Created", &remote->created, 1);
        ok = ok && flipper_format_write_uint32(ff, "Updated", &remote->updated, 1);
        ok = ok && flipper_format_write_uint32(ff, "Last_used", &remote->last_used, 1);
        ok = ok && flipper_format_write_string_cstr(
                       ff, "Preferred_radio", rfs_radio_mode_name(remote->preferred_radio));
        ok = ok && flipper_format_write_uint32(
                       ff, "Preferred_frequency", &remote->preferred_frequency, 1);
        if(remote->preferred_preset[0])
            ok = ok && flipper_format_write_string_cstr(
                           ff, "Preferred_preset", remote->preferred_preset);
        ok = ok && flipper_format_write_string_cstr(ff, "Display_mode", "list");
        ok = ok && flipper_format_write_uint32(ff, "Buttons", &count, 1);
        ok = ok && flipper_format_write_comment_cstr(ff, "Buttons in display order");
        for(size_t i = 0; ok && i < remote->count; i++) {
            const RfsButton* b = &remote->buttons[i];
            ok = ok && flipper_format_write_string_cstr(ff, "Button_id", b->id);
            ok = ok && flipper_format_write_string_cstr(ff, "Button_name", b->name);
            ok = ok && flipper_format_write_string_cstr(
                           ff, "Button_signal", b->signal_id[0] ? b->signal_id : NO_SIGNAL);
            ok = ok && flipper_format_write_uint32(ff, "Button_created", &b->created, 1);
            ok = ok && flipper_format_write_uint32(ff, "Button_updated", &b->updated, 1);
        }
    }
    return rfs_storage_atomic_commit(storage, ff, path, ok);
}

bool rfs_remote_save(Storage* storage, RfsRemote* remote) {
    remote->updated = rfs_storage_now();
    FuriString* path = furi_string_alloc();
    rfs_remote_path(remote->id, path);
    bool ok = rfs_remote_save_path(storage, furi_string_get_cstr(path), remote);
    furi_string_free(path);
    return ok;
}

bool rfs_remote_delete(Storage* storage, const char* id) {
    FuriString* path = furi_string_alloc();
    rfs_remote_path(id, path);
    FS_Error err = storage_common_remove(storage, furi_string_get_cstr(path));
    furi_string_free(path);
    return err == FSE_OK || err == FSE_NOT_EXIST;
}

static void rfs_button_new_id(RfsRemote* remote, char* out) {
    /* Button IDs only need to be unique within a remote. */
    while(true) {
        rfs_storage_new_id(NULL, 'B', NULL, NULL, out);
        bool clash = false;
        for(size_t i = 0; i < remote->count; i++) {
            if(strcmp(remote->buttons[i].id, out) == 0) clash = true;
        }
        if(!clash) return;
    }
}

RfsButton* rfs_remote_add_button(
    Storage* storage,
    RfsRemote* remote,
    const char* name,
    const char* signal_id) {
    UNUSED(storage);
    if(!rfs_remote_reserve(remote, remote->count + 1)) return NULL;
    RfsButton* button = &remote->buttons[remote->count];
    memset(button, 0, sizeof(RfsButton));
    rfs_button_new_id(remote, button->id);
    strlcpy(button->name, name, sizeof(button->name));
    if(signal_id) strlcpy(button->signal_id, signal_id, sizeof(button->signal_id));
    button->created = rfs_storage_now();
    button->updated = button->created;
    remote->count++;
    return button;
}

void rfs_remote_remove_button(RfsRemote* remote, size_t index) {
    if(index >= remote->count) return;
    memmove(
        &remote->buttons[index],
        &remote->buttons[index + 1],
        (remote->count - index - 1) * sizeof(RfsButton));
    remote->count--;
}

bool rfs_remote_move_button(RfsRemote* remote, size_t index, bool up) {
    if(index >= remote->count) return false;
    if(up && index == 0) return false;
    if(!up && index + 1 >= remote->count) return false;
    size_t other = up ? index - 1 : index + 1;
    RfsButton tmp = remote->buttons[index];
    remote->buttons[index] = remote->buttons[other];
    remote->buttons[other] = tmp;
    return true;
}

RfsButton* rfs_remote_duplicate_button(Storage* storage, RfsRemote* remote, size_t index) {
    UNUSED(storage);
    if(index >= remote->count) return NULL;
    if(!rfs_remote_reserve(remote, remote->count + 1)) return NULL;
    RfsButton copy = remote->buttons[index];
    rfs_button_new_id(remote, copy.id);
    copy.created = rfs_storage_now();
    copy.updated = copy.created;
    /* Insert directly after the original; the signal is shared, not copied (SIG-008). */
    memmove(
        &remote->buttons[index + 2],
        &remote->buttons[index + 1],
        (remote->count - index - 1) * sizeof(RfsButton));
    remote->buttons[index + 1] = copy;
    remote->count++;
    return &remote->buttons[index + 1];
}

bool rfs_remote_duplicate(Storage* storage, const RfsRemote* source, FuriString* out_name) {
    RfsRemote copy = *source;
    copy.buttons = NULL;
    copy.capacity = 0;
    copy.count = 0;
    if(source->count) {
        if(memmgr_get_free_heap() < source->count * sizeof(RfsButton) + RFS_MIN_FREE_HEAP)
            return false;
        copy.buttons = malloc(source->count * sizeof(RfsButton));
        memcpy(copy.buttons, source->buttons, source->count * sizeof(RfsButton));
        copy.count = source->count;
        copy.capacity = source->count;
    }
    rfs_storage_new_id(storage, 'R', RFS_REMOTES_DIR, RFS_REMOTE_EXT, copy.id);
    FuriString* name = furi_string_alloc_printf("%s copy", source->name);
    if(furi_string_size(name) > RFS_NAME_LEN) furi_string_left(name, RFS_NAME_LEN);
    strlcpy(copy.name, furi_string_get_cstr(name), sizeof(copy.name));
    if(out_name) furi_string_set(out_name, name);
    furi_string_free(name);
    copy.favorite = false;
    copy.created = rfs_storage_now();
    copy.last_used = 0;
    bool ok = rfs_remote_save(storage, &copy);
    free(copy.buttons);
    return ok;
}

void rfs_remote_list_init(RfsRemoteList* list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void rfs_remote_list_clear(RfsRemoteList* list) {
    free(list->items);
    rfs_remote_list_init(list);
}

static RfsRemoteSummary* rfs_remote_list_add(RfsRemoteList* list) {
    if(list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 16;
        if(memmgr_get_free_heap() <
           (capacity - list->capacity) * sizeof(RfsRemoteSummary) + RFS_MIN_FREE_HEAP)
            return NULL;
        list->items = realloc(list->items, capacity * sizeof(RfsRemoteSummary));
        list->capacity = capacity;
    }
    RfsRemoteSummary* item = &list->items[list->count++];
    memset(item, 0, sizeof(RfsRemoteSummary));
    return item;
}

static bool rfs_remote_read_summary(
    FlipperFormat* ff,
    const char* path,
    const char* file_id,
    FuriString* tmp,
    RfsRemoteSummary* out) {
    if(!flipper_format_buffered_file_open_existing(ff, path)) return false;
    bool ok = false;
    uint32_t version = 0;
    if(flipper_format_read_header(ff, tmp, &version) &&
       furi_string_cmp_str(tmp, REMOTE_FILETYPE) == 0) {
        ok = true;
        if(!rfs_read_into(ff, "ID", tmp, out->id, sizeof(out->id)))
            strlcpy(out->id, file_id, sizeof(out->id));
        if(!rfs_read_into(ff, "Name", tmp, out->name, sizeof(out->name)))
            strlcpy(out->name, "Unnamed", sizeof(out->name));
        flipper_format_rewind(ff);
        if(flipper_format_read_string(ff, "Category", tmp))
            out->category = rfs_category_parse(furi_string_get_cstr(tmp));
        flipper_format_rewind(ff);
        flipper_format_read_bool(ff, "Favorite", &out->favorite, 1);
        rfs_read_u32(ff, "Last_used", &out->last_used);
        rfs_read_u32(ff, "Updated", &out->updated);
        rfs_read_u32(ff, "Buttons", &out->button_count);
    }
    flipper_format_buffered_file_close(ff);
    return ok;
}

bool rfs_remote_list_load(Storage* storage, RfsRemoteList* list) {
    rfs_remote_list_clear(list);
    File* dir = storage_file_alloc(storage);
    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);
    FuriString* path = furi_string_alloc();
    FuriString* tmp = furi_string_alloc();
    char name[64];
    FileInfo info;
    bool ok = false;

    /* Finish any interrupted saves first so every remote is listed. */
    char pending[4][64];
    size_t pending_count = 0;
    if(storage_dir_open(dir, RFS_REMOTES_DIR)) {
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            size_t len = strlen(name);
            if(len > 4 && strcmp(name + len - 4, ".tmp") == 0 && pending_count < 4) {
                name[len - 4] = '\0';
                strlcpy(pending[pending_count++], name, sizeof(pending[0]));
            }
        }
    }
    storage_dir_close(dir);
    for(size_t i = 0; i < pending_count; i++) {
        furi_string_printf(path, "%s/%s", RFS_REMOTES_DIR, pending[i]);
        rfs_storage_recover(storage, furi_string_get_cstr(path));
    }

    if(storage_dir_open(dir, RFS_REMOTES_DIR)) {
        ok = true;
        size_t ext_len = strlen(RFS_REMOTE_EXT);
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            size_t len = strlen(name);
            if(len <= ext_len || strcmp(name + len - ext_len, RFS_REMOTE_EXT) != 0) continue;
            furi_string_printf(path, "%s/%s", RFS_REMOTES_DIR, name);
            name[len - ext_len] = '\0';
            RfsRemoteSummary summary;
            memset(&summary, 0, sizeof(summary));
            if(!rfs_remote_read_summary(ff, furi_string_get_cstr(path), name, tmp, &summary))
                continue;
            RfsRemoteSummary* item = rfs_remote_list_add(list);
            if(!item) break;
            *item = summary;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    furi_string_free(tmp);
    furi_string_free(path);
    flipper_format_free(ff);
    return ok;
}

static int
    rfs_summary_compare(const RfsRemoteSummary* a, const RfsRemoteSummary* b, RfsSort sort) {
    if(sort == RfsSortCategory && a->category != b->category) {
        return (int)a->category - (int)b->category;
    }
    if(sort == RfsSortLastUsed && a->last_used != b->last_used) {
        return a->last_used > b->last_used ? -1 : 1;
    }
    return rfs_strcasecmp(a->name, b->name);
}

void rfs_remote_list_sort(RfsRemoteList* list, RfsSort sort) {
    /* qsort is not exported to applications; insertion sort suits library-sized lists. */
    for(size_t i = 1; i < list->count; i++) {
        RfsRemoteSummary item = list->items[i];
        size_t j = i;
        while(j > 0 && rfs_summary_compare(&list->items[j - 1], &item, sort) > 0) {
            list->items[j] = list->items[j - 1];
            j--;
        }
        list->items[j] = item;
    }
}

size_t rfs_remote_signal_usage(Storage* storage, const char* signal_id, FuriString* out) {
    RfsRemoteList list;
    rfs_remote_list_init(&list);
    rfs_remote_list_load(storage, &list);
    RfsRemote remote;
    rfs_remote_init(&remote);
    size_t uses = 0;
    for(size_t i = 0; i < list.count; i++) {
        if(!rfs_remote_load(storage, list.items[i].id, &remote, NULL)) continue;
        for(size_t b = 0; b < remote.count; b++) {
            if(strcmp(remote.buttons[b].signal_id, signal_id) == 0) {
                uses++;
                if(out)
                    furi_string_cat_printf(out, " %s: %s\n", remote.name, remote.buttons[b].name);
            }
        }
    }
    rfs_remote_clear(&remote);
    rfs_remote_list_clear(&list);
    return uses;
}

bool rfs_remote_export(
    Storage* storage,
    const RfsRemote* remote,
    RfsSignalIndex* index,
    FuriString* out_dir) {
    furi_string_printf(out_dir, "%s/%s", RFS_EXPORTS_DIR, remote->id);
    const char* dir = furi_string_get_cstr(out_dir);
    FuriString* path = furi_string_alloc();
    FuriString* src = furi_string_alloc();
    FuriString* safe = furi_string_alloc();
    RfsSignalIndex subset;
    rfs_signal_index_init(&subset);
    bool ok = storage_simply_mkdir(storage, RFS_EXPORTS_DIR) && storage_simply_mkdir(storage, dir);

    furi_string_printf(path, "%s/signals", dir);
    ok = ok && storage_simply_mkdir(storage, furi_string_get_cstr(path));

    for(size_t i = 0; ok && i < remote->count; i++) {
        const char* sid = remote->buttons[i].signal_id;
        if(!sid[0] || rfs_signal_find(&subset, sid)) continue;
        RfsSignalMeta* meta = rfs_signal_find(index, sid);
        rfs_signal_path(sid, src);
        if(!meta || !storage_file_exists(storage, furi_string_get_cstr(src))) continue;
        furi_string_printf(path, "%s/signals/%s%s", dir, sid, RFS_SIGNAL_EXT);
        storage_common_remove(storage, furi_string_get_cstr(path));
        if(storage_common_copy(storage, furi_string_get_cstr(src), furi_string_get_cstr(path)) !=
           FSE_OK) {
            ok = false;
            break;
        }
        RfsSignalMeta* entry = rfs_signal_add(&subset);
        if(!entry) {
            ok = false;
            break;
        }
        *entry = *meta;
    }

    if(ok) {
        furi_string_printf(path, "%s/signals.index", dir);
        ok = rfs_signal_index_save_path(storage, furi_string_get_cstr(path), &subset);
    }
    if(ok) {
        rfs_storage_safe_filename(remote->name, safe);
        furi_string_printf(path, "%s/%s%s", dir, furi_string_get_cstr(safe), RFS_REMOTE_EXT);
        ok = rfs_remote_save_path(storage, furi_string_get_cstr(path), remote);
    }

    rfs_signal_index_clear(&subset);
    furi_string_free(safe);
    furi_string_free(src);
    furi_string_free(path);
    return ok;
}

bool rfs_remote_import(
    Storage* storage,
    const char* remote_path,
    RfsSignalIndex* index,
    FuriString* out_name,
    FuriString* error) {
    RfsRemote remote;
    rfs_remote_init(&remote);
    RfsSignalIndex exported;
    rfs_signal_index_init(&exported);
    FuriString* dir = furi_string_alloc();
    FuriString* src = furi_string_alloc();
    FuriString* dst = furi_string_alloc();
    bool ok = false;
    bool index_changed = false;

    do {
        if(!rfs_remote_load_path(storage, remote_path, &remote, error)) break;
        path_extract_dirname(remote_path, dir);
        furi_string_printf(src, "%s/signals.index", furi_string_get_cstr(dir));
        rfs_signal_index_load_path(storage, furi_string_get_cstr(src), &exported);

        for(size_t i = 0; i < remote.count; i++) {
            const char* sid = remote.buttons[i].signal_id;
            if(!sid[0]) continue;
            rfs_signal_path(sid, dst);
            if(rfs_signal_find(index, sid) &&
               storage_file_exists(storage, furi_string_get_cstr(dst)))
                continue; /* already in the library: reuse it */

            furi_string_printf(
                src, "%s/signals/%s%s", furi_string_get_cstr(dir), sid, RFS_SIGNAL_EXT);
            if(!storage_file_exists(storage, furi_string_get_cstr(src)))
                continue; /* stays missing */
            storage_common_remove(storage, furi_string_get_cstr(dst));
            if(storage_common_copy(
                   storage, furi_string_get_cstr(src), furi_string_get_cstr(dst)) != FSE_OK)
                continue;

            if(!rfs_signal_find(index, sid)) {
                RfsSignalMeta* meta = rfs_signal_add(index);
                if(!meta) break;
                RfsSignalMeta* from = rfs_signal_find(&exported, sid);
                if(from) {
                    *meta = *from;
                } else {
                    rfs_signal_inspect(storage, furi_string_get_cstr(dst), meta);
                    strlcpy(meta->id, sid, sizeof(meta->id));
                    strlcpy(meta->name, remote.buttons[i].name, sizeof(meta->name));
                    meta->source = RfsSourceImport;
                    meta->created = rfs_storage_now();
                }
                index_changed = true;
            }
        }
        if(index_changed && !rfs_signal_index_save(storage, index)) {
            if(error) furi_string_set(error, "Could not save signal index");
            break;
        }

        rfs_remote_path(remote.id, dst);
        if(storage_file_exists(storage, furi_string_get_cstr(dst))) {
            /* Keep the existing remote; import as a separate copy. */
            rfs_storage_new_id(storage, 'R', RFS_REMOTES_DIR, RFS_REMOTE_EXT, remote.id);
            FuriString* name = furi_string_alloc_printf("%s 2", remote.name);
            if(furi_string_size(name) > RFS_NAME_LEN) furi_string_left(name, RFS_NAME_LEN);
            strlcpy(remote.name, furi_string_get_cstr(name), sizeof(remote.name));
            furi_string_free(name);
        }
        if(!rfs_remote_save(storage, &remote)) {
            if(error) furi_string_set(error, "Could not save remote");
            break;
        }
        if(out_name) furi_string_set(out_name, remote.name);
        ok = true;
    } while(false);

    furi_string_free(dst);
    furi_string_free(src);
    furi_string_free(dir);
    rfs_signal_index_clear(&exported);
    rfs_remote_clear(&remote);
    return ok;
}

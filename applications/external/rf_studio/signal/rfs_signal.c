#include "rfs_signal.h"
#include "../storage/rfs_storage.h"
#include "../radio/rfs_radio.h"

#include <flipper_format/flipper_format.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/protocols/raw.h>
#include <toolbox/path.h>

#define INDEX_FILETYPE "RF Studio Signal Index"
#define INDEX_VERSION  1

#define SUBGHZ_KEY_FILETYPE "Flipper SubGhz Key File"
#define SUBGHZ_RAW_FILETYPE "Flipper SubGhz RAW File"

void rfs_signal_index_init(RfsSignalIndex* index) {
    index->items = NULL;
    index->count = 0;
    index->capacity = 0;
}

void rfs_signal_index_clear(RfsSignalIndex* index) {
    free(index->items);
    rfs_signal_index_init(index);
}

RfsSignalMeta* rfs_signal_add(RfsSignalIndex* index) {
    if(index->count == index->capacity) {
        size_t capacity = index->capacity ? index->capacity * 2 : 16;
        size_t grow = (capacity - index->capacity) * sizeof(RfsSignalMeta);
        if(memmgr_get_free_heap() < grow + RFS_MIN_FREE_HEAP) return NULL;
        index->items = realloc(index->items, capacity * sizeof(RfsSignalMeta));
        index->capacity = capacity;
    }
    RfsSignalMeta* meta = &index->items[index->count++];
    memset(meta, 0, sizeof(RfsSignalMeta));
    return meta;
}

RfsSignalMeta* rfs_signal_find(RfsSignalIndex* index, const char* id) {
    if(!id || !*id) return NULL;
    for(size_t i = 0; i < index->count; i++) {
        if(strcmp(index->items[i].id, id) == 0) return &index->items[i];
    }
    return NULL;
}

void rfs_signal_remove(RfsSignalIndex* index, const char* id) {
    for(size_t i = 0; i < index->count; i++) {
        if(strcmp(index->items[i].id, id) == 0) {
            memmove(
                &index->items[i],
                &index->items[i + 1],
                (index->count - i - 1) * sizeof(RfsSignalMeta));
            index->count--;
            return;
        }
    }
}

void rfs_signal_path(const char* id, FuriString* out) {
    furi_string_printf(out, "%s/%s%s", RFS_SIGNALS_DIR, id, RFS_SIGNAL_EXT);
}

RfsReplay rfs_signal_replay(const RfsSignalMeta* meta) {
    return rfs_protocol_replay(meta->kind, meta->protocol, meta->verified);
}

static bool
    rfs_read_cstr(FlipperFormat* ff, const char* key, FuriString* tmp, char* out, size_t size) {
    if(!flipper_format_read_string(ff, key, tmp)) return false;
    strlcpy(out, furi_string_get_cstr(tmp), size);
    return true;
}

bool rfs_signal_index_load_path(Storage* storage, const char* path, RfsSignalIndex* index) {
    rfs_signal_index_clear(index);
    rfs_storage_recover(storage, path);
    if(!storage_file_exists(storage, path)) return true; /* empty library */

    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);
    FuriString* tmp = furi_string_alloc();
    bool ok = false;
    do {
        if(!flipper_format_buffered_file_open_existing(ff, path)) break;
        uint32_t version = 0;
        if(!flipper_format_read_header(ff, tmp, &version)) break;
        if(furi_string_cmp_str(tmp, INDEX_FILETYPE) != 0 || version > INDEX_VERSION) break;

        /* Records are read in file order; every record writes every key. */
        while(true) {
            RfsSignalMeta entry;
            memset(&entry, 0, sizeof(entry));
            uint32_t value = 0;
            if(!rfs_read_cstr(ff, "ID", tmp, entry.id, sizeof(entry.id))) break;
            if(!rfs_read_cstr(ff, "Name", tmp, entry.name, sizeof(entry.name))) break;
            if(!flipper_format_read_uint32(ff, "Frequency", &entry.frequency, 1)) break;
            if(!rfs_read_cstr(ff, "Preset", tmp, entry.preset, sizeof(entry.preset))) break;
            if(!rfs_read_cstr(ff, "Protocol", tmp, entry.protocol, sizeof(entry.protocol))) break;
            if(!flipper_format_read_uint32(ff, "Kind", &value, 1)) break;
            entry.kind = value < RfsKindCount ? value : RfsKindUnknown;
            if(!flipper_format_read_uint32(ff, "Source", &value, 1)) break;
            entry.source = value < RfsSourceCount ? value : RfsSourceImport;
            if(!flipper_format_read_bool(ff, "Verified", &entry.verified, 1)) break;
            if(!flipper_format_read_uint32(ff, "Created", &entry.created, 1)) break;

            RfsSignalMeta* meta = rfs_signal_add(index);
            if(!meta) break;
            *meta = entry;
        }
        ok = true;
    } while(false);
    furi_string_free(tmp);
    flipper_format_free(ff);
    return ok;
}

bool rfs_signal_index_load(Storage* storage, RfsSignalIndex* index) {
    return rfs_signal_index_load_path(storage, RFS_INDEX_PATH, index);
}

bool rfs_signal_index_save_path(Storage* storage, const char* path, const RfsSignalIndex* index) {
    FlipperFormat* ff = rfs_storage_atomic_begin(storage, path);
    bool ok = ff != NULL;
    if(ok) ok = flipper_format_write_header_cstr(ff, INDEX_FILETYPE, INDEX_VERSION);
    for(size_t i = 0; ok && i < index->count; i++) {
        const RfsSignalMeta* m = &index->items[i];
        uint32_t value;
        ok = ok && flipper_format_write_empty_line(ff);
        ok = ok && flipper_format_write_string_cstr(ff, "ID", m->id);
        ok = ok && flipper_format_write_string_cstr(ff, "Name", m->name);
        ok = ok && flipper_format_write_uint32(ff, "Frequency", &m->frequency, 1);
        ok = ok && flipper_format_write_string_cstr(ff, "Preset", m->preset);
        ok = ok && flipper_format_write_string_cstr(ff, "Protocol", m->protocol);
        value = m->kind;
        ok = ok && flipper_format_write_uint32(ff, "Kind", &value, 1);
        value = m->source;
        ok = ok && flipper_format_write_uint32(ff, "Source", &value, 1);
        ok = ok && flipper_format_write_bool(ff, "Verified", &m->verified, 1);
        ok = ok && flipper_format_write_uint32(ff, "Created", &m->created, 1);
    }
    return rfs_storage_atomic_commit(storage, ff, path, ok);
}

bool rfs_signal_index_save(Storage* storage, const RfsSignalIndex* index) {
    return rfs_signal_index_save_path(storage, RFS_INDEX_PATH, index);
}

RfsSubResult rfs_signal_inspect(Storage* storage, const char* path, RfsSignalMeta* meta) {
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* tmp = furi_string_alloc();
    RfsSubResult result = RfsSubOk;
    bool raw_file = false;

    do {
        if(!flipper_format_file_open_existing(ff, path)) {
            result = RfsSubErrOpen;
            break;
        }
        uint32_t version = 0;
        if(!flipper_format_read_header(ff, tmp, &version)) {
            result = RfsSubErrHeader;
            break;
        }
        if(furi_string_cmp_str(tmp, SUBGHZ_RAW_FILETYPE) == 0) {
            raw_file = true;
        } else if(furi_string_cmp_str(tmp, SUBGHZ_KEY_FILETYPE) != 0) {
            result = RfsSubErrHeader;
            break;
        }
        if(!flipper_format_read_uint32(ff, "Frequency", &meta->frequency, 1) ||
           meta->frequency == 0) {
            result = RfsSubErrFrequency;
            break;
        }
        if(!flipper_format_read_string(ff, "Preset", tmp)) {
            result = RfsSubErrPreset;
            break;
        }
        rfs_radio_preset_from_file(furi_string_get_cstr(tmp), meta->preset, sizeof(meta->preset));
        if(!flipper_format_read_string(ff, "Protocol", tmp)) {
            result = RfsSubErrProtocol;
            break;
        }
        strlcpy(meta->protocol, furi_string_get_cstr(tmp), sizeof(meta->protocol));

        if(raw_file || strcmp(meta->protocol, SUBGHZ_PROTOCOL_RAW_NAME) == 0) {
            meta->kind = RfsKindRaw;
            /* A raw file must contain timing data. */
            if(!flipper_format_key_exist(ff, "RAW_Data")) result = RfsSubErrEmpty;
            break;
        }

        const SubGhzProtocol* protocol = rfs_protocol_find(meta->protocol);
        meta->kind = rfs_protocol_kind(protocol);
        if(!protocol || !rfs_protocol_can_send(protocol) || meta->kind == RfsKindDynamic) {
            /* Recognised or unknown, but not something to validate by encoding. */
            break;
        }

        /* Validate the key data by loading it into the firmware encoder. */
        SubGhzEnvironment* env = rfs_protocol_env_alloc();
        SubGhzTransmitter* transmitter = subghz_transmitter_alloc_init(env, meta->protocol);
        if(transmitter) {
            flipper_format_rewind(ff);
            if(subghz_transmitter_deserialize(transmitter, ff) != SubGhzProtocolStatusOk) {
                result = RfsSubErrData;
            }
            subghz_transmitter_free(transmitter);
        }
        subghz_environment_free(env);
    } while(false);

    furi_string_free(tmp);
    flipper_format_free(ff);
    return result;
}

const char* rfs_sub_result_text(RfsSubResult result) {
    switch(result) {
    case RfsSubOk:
        return "Valid";
    case RfsSubErrOpen:
        return "File not found";
    case RfsSubErrHeader:
        return "Not a Sub-GHz file";
    case RfsSubErrFrequency:
        return "Missing frequency";
    case RfsSubErrPreset:
        return "Missing preset";
    case RfsSubErrProtocol:
        return "Missing protocol";
    case RfsSubErrData:
        return "Invalid key data";
    default:
        return "No signal data";
    }
}

RfsSubResult rfs_signal_import(
    Storage* storage,
    RfsSignalIndex* index,
    const char* source_path,
    RfsSignalMeta** out_meta) {
    RfsSignalMeta probe;
    memset(&probe, 0, sizeof(probe));
    RfsSubResult result = rfs_signal_inspect(storage, source_path, &probe);
    if(result != RfsSubOk) return result;

    RfsSignalMeta* meta = rfs_signal_add(index);
    if(!meta) return RfsSubErrOpen;
    *meta = probe;
    rfs_storage_new_id(storage, 'S', RFS_SIGNALS_DIR, RFS_SIGNAL_EXT, meta->id);

    FuriString* name = furi_string_alloc();
    FuriString* src = furi_string_alloc_set(source_path);
    path_extract_filename(src, name, true);
    strlcpy(meta->name, furi_string_get_cstr(name), sizeof(meta->name));
    meta->source = RfsSourceImport;
    meta->created = rfs_storage_now();

    FuriString* dest = furi_string_alloc();
    rfs_signal_path(meta->id, dest);
    bool ok = storage_common_copy(storage, source_path, furi_string_get_cstr(dest)) == FSE_OK;
    if(ok) ok = rfs_signal_index_save(storage, index);
    if(!ok) {
        storage_common_remove(storage, furi_string_get_cstr(dest));
        rfs_signal_remove(index, meta->id);
        meta = NULL;
        result = RfsSubErrOpen;
    }
    if(out_meta) *out_meta = meta;

    furi_string_free(dest);
    furi_string_free(src);
    furi_string_free(name);
    return result;
}

bool rfs_signal_delete(Storage* storage, RfsSignalIndex* index, const char* id) {
    char id_copy[RFS_ID_LEN];
    strlcpy(id_copy, id, sizeof(id_copy));
    FuriString* path = furi_string_alloc();
    rfs_signal_path(id_copy, path);
    FS_Error err = storage_common_remove(storage, furi_string_get_cstr(path));
    furi_string_free(path);
    if(err != FSE_OK && err != FSE_NOT_EXIST) return false;
    rfs_signal_remove(index, id_copy);
    return rfs_signal_index_save(storage, index);
}

bool rfs_signal_export(Storage* storage, const RfsSignalMeta* meta, FuriString* out_path) {
    if(!storage_simply_mkdir(storage, EXT_PATH("subghz")) ||
       !storage_simply_mkdir(storage, RFS_SUBGHZ_EXPORT_DIR))
        return false;

    FuriString* base = furi_string_alloc();
    rfs_storage_safe_filename(meta->name, base);
    furi_string_printf(
        out_path, "%s/%s%s", RFS_SUBGHZ_EXPORT_DIR, furi_string_get_cstr(base), RFS_SIGNAL_EXT);
    for(uint8_t n = 2; storage_common_exists(storage, furi_string_get_cstr(out_path)) && n < 100;
        n++) {
        furi_string_printf(
            out_path,
            "%s/%s_%u%s",
            RFS_SUBGHZ_EXPORT_DIR,
            furi_string_get_cstr(base),
            n,
            RFS_SIGNAL_EXT);
    }

    FuriString* src = furi_string_alloc();
    rfs_signal_path(meta->id, src);
    bool ok = storage_common_copy(
                  storage, furi_string_get_cstr(src), furi_string_get_cstr(out_path)) == FSE_OK;
    furi_string_free(src);
    furi_string_free(base);
    return ok;
}

void rfs_signal_describe(const RfsSignalMeta* meta, FuriString* out) {
    FuriString* when = furi_string_alloc();
    rfs_storage_format_time(meta->created, when);
    furi_string_cat_printf(
        out,
        "Name: %s\n"
        "Frequency: %lu.%02lu MHz\n"
        "Modulation: %s\n"
        "Protocol: %s\n"
        "Type: %s\n"
        "Replay: %s\n"
        "Source: %s\n"
        "Added: %s\n"
        "File: signals/%s.sub\n",
        meta->name,
        (unsigned long)(meta->frequency / 1000000),
        (unsigned long)((meta->frequency % 1000000) / 10000),
        meta->preset,
        meta->protocol,
        rfs_kind_name(meta->kind),
        rfs_replay_name(rfs_signal_replay(meta)),
        meta->source == RfsSourceCapture ? "Captured" : "Imported",
        furi_string_get_cstr(when),
        meta->id);
    furi_string_free(when);
}

#include "meshcore_preset_store.h"

#include <storage/storage.h>

/* A preset file is a handful of short fields. Anything larger than this is not
 * a preset, and reading it into RAM on a device with 190 KB free is not a
 * favour to anyone. */
#define MESHCORE_PRESET_FILE_MAX 1024u

void meshcore_preset_store_init(MeshCorePresetStore* store) {
    furi_assert(store);

    memset(store, 0, sizeof(*store));

    for(size_t i = 0; i < MESHCORE_BUILTIN_PRESET_COUNT && i < MESHCORE_PRESET_STORE_MAX; i++) {
        store->items[store->count].preset = MESHCORE_BUILTIN_PRESETS[i];
        store->items[store->count].error[0] = '\0';
        store->items[store->count].file[0] = '\0';
        store->count++;
    }
}

/* One file into one entry. Failures are recorded rather than dropped. */
static void meshcore_preset_store_read_one(
    MeshCorePresetEntry* entry,
    Storage* storage,
    const char* name) {
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", MESHCORE_PRESET_DIR, name);
    snprintf(entry->file, sizeof(entry->file), "%.31s", name);
    entry->error[0] = '\0';

    memset(&entry->preset, 0, sizeof(entry->preset));
    /* Named after the file until the JSON says otherwise, so even a broken
     * entry is identifiable in the list. */
    snprintf(entry->preset.name, sizeof(entry->preset.name), "%.23s", name);

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_close(file);
        storage_file_free(file);
        snprintf(entry->error, sizeof(entry->error), "cannot open");
        return;
    }

    uint64_t size = storage_file_size(file);
    if(size >= MESHCORE_PRESET_FILE_MAX) {
        storage_file_close(file);
        storage_file_free(file);
        snprintf(entry->error, sizeof(entry->error), "too large");
        return;
    }

    char* text = malloc((size_t)size + 1);
    if(text == NULL) {
        /* Low RAM with a preset file present is a real state on this device:
         * the UART buffer, contact table and CSV buffers are all live. Report
         * it as a failed entry rather than dereferencing NULL. */
        storage_file_close(file);
        storage_file_free(file);
        snprintf(entry->error, sizeof(entry->error), "out of memory");
        return;
    }

    uint16_t got = storage_file_read(file, text, (uint16_t)size);
    text[got] = '\0';
    storage_file_close(file);
    storage_file_free(file);

    const char* why = NULL;
    if(!meshcore_preset_from_json(text, &entry->preset, &why)) {
        snprintf(entry->error, sizeof(entry->error), "%.39s", why ? why : "bad JSON");
        /* from_json memsets the preset before parsing, which wipes the
         * filename fallback set above; restore it so a broken file is still
         * named in the Profiles list rather than shown blank. */
        snprintf(entry->preset.name, sizeof(entry->preset.name), "%.23s", name);
    } else if(!meshcore_preset_validate(&entry->preset, &why)) {
        snprintf(entry->error, sizeof(entry->error), "%.39s", why ? why : "out of range");
    }

    free(text);
}

void meshcore_preset_store_scan(MeshCorePresetStore* store, Storage* storage) {
    furi_assert(store);
    furi_assert(storage);

    meshcore_preset_store_init(store);
    store->skipped = 0;
    store->scanned = true;

    File* dir = storage_file_alloc(storage);
    if(!storage_dir_open(dir, MESHCORE_PRESET_DIR)) {
        /* No directory is the normal case on a fresh card, not an error: the
         * built-ins are already loaded and that is a working list. */
        storage_dir_close(dir);
        storage_file_free(dir);
        return;
    }

    char name[64];
    FileInfo info;
    while(storage_dir_read(dir, &info, name, sizeof(name))) {
        if(file_info_is_dir(&info)) continue;

        size_t len = strlen(name);
        if(len < 6 || strcmp(name + len - 5, ".json") != 0) continue;

        if(store->count >= MESHCORE_PRESET_STORE_MAX) {
            store->skipped++;
            continue;
        }

        meshcore_preset_store_read_one(&store->items[store->count], storage, name);
        store->count++;
    }

    storage_dir_close(dir);
    storage_file_free(dir);
}

bool meshcore_preset_store_usable(const MeshCorePresetEntry* entry) {
    furi_assert(entry);
    return entry->error[0] == '\0';
}

bool meshcore_preset_store_save(Storage* storage, const MeshCorePreset* preset, const char** why) {
    furi_assert(storage);
    furi_assert(preset);

    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, EXT_PATH("apps_data/meshcore_cfg"));
    storage_simply_mkdir(storage, MESHCORE_PRESET_DIR);

    /* The name is the filename, so it has to survive being one. */
    char safe[MESHCORE_PRESET_NAME_LEN];
    size_t i = 0;
    for(; preset->name[i] != '\0' && i + 1 < sizeof(safe); i++) {
        char c = preset->name[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '-' || c == '_';
        safe[i] = ok ? c : '_';
    }
    safe[i] = '\0';
    if(i == 0) {
        if(why) *why = "preset has no name";
        return false;
    }

    char path[128];
    snprintf(path, sizeof(path), "%s/%s.json", MESHCORE_PRESET_DIR, safe);

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_close(file);
        storage_file_free(file);
        if(why) *why = "cannot write to the card";
        return false;
    }

    /* Written in the display units the loader parses, not the wire units the
     * struct holds — a file someone may edit by hand should read the way the
     * radio is talked about. */
    char text[512];
    int len = snprintf(
        text,
        sizeof(text),
        "{\n"
        "  \"name\": \"%s\",\n"
        "  \"freq_mhz\": %lu.%03lu,\n"
        /* Three decimals, matching the Hz resolution the value is stored at.
         * One digit (100 Hz) silently rounded quarter-step bandwidths like
         * 31.25 kHz down to 31.2 on a save->load round-trip. */
        "  \"bw_khz\": %lu.%03lu,\n"
        "  \"sf\": %u,\n"
        "  \"cr\": %u,\n"
        "  \"path_hash_bytes\": %u",
        preset->name,
        (unsigned long)(preset->freq_khz / 1000u),
        (unsigned long)(preset->freq_khz % 1000u),
        (unsigned long)(preset->bw_hz / 1000u),
        (unsigned long)(preset->bw_hz % 1000u),
        (unsigned)preset->sf,
        (unsigned)preset->cr,
        (unsigned)preset->path_hash_bytes);

    if(preset->has_node_name && len > 0 && (size_t)len < sizeof(text)) {
        len += snprintf(
            text + len, sizeof(text) - (size_t)len, ",\n  \"node_name\": \"%s\"", preset->node_name);
    }
    if(len > 0 && (size_t)len < sizeof(text)) {
        len += snprintf(text + len, sizeof(text) - (size_t)len, "\n}\n");
    }

    bool ok = len > 0 && storage_file_write(file, text, (uint16_t)len) == (uint16_t)len;
    storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);

    if(!ok && why) *why = "write failed";
    return ok;
}

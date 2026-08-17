#include "bambu_storage.h"
#include "bambu_tag.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

namespace {
bool supported_extension(const char* name) {
    const char* dot = strrchr(name, '.');
    if(!dot) return false;
    return strcasecmp(dot, ".nfc") == 0 || strcasecmp(dot, ".bin") == 0;
}

bool safe_name(const char* name) {
    return name && name[0] && !strchr(name, '/') && !strchr(name, '\\') && strcmp(name, ".") != 0 &&
           strcmp(name, "..") != 0;
}

bool safe_relative_path(const char* path) {
    if(!path || !path[0] || path[0] == '/' || strchr(path, '\\') || strstr(path, "..")) return false;
    return true;
}

void display_name_from_info(const BrTagInfo& info, char* out, size_t out_size) {
    if(info.detailed_filament_type[0]) {
        snprintf(out, out_size, "%s %s", info.detailed_filament_type, info.uid_hex);
    } else if(info.filament_type[0]) {
        snprintf(out, out_size, "%s %s", info.filament_type, info.uid_hex);
    } else {
        snprintf(out, out_size, "%s", info.uid_hex);
    }
}

int compare_entries(const BrSavedEntry& a, const BrSavedEntry& b) {
    return strcasecmp(a.display_name, b.display_name);
}

void sort_entries(BrSavedEntry* entries, uint16_t count) {
    for(uint16_t i = 1; i < count; ++i) {
        BrSavedEntry current = entries[i];
        uint16_t j = i;
        while(j > 0 && compare_entries(entries[j - 1], current) > 0) {
            entries[j] = entries[j - 1];
            --j;
        }
        entries[j] = current;
    }
}

bool already_listed(const BrSavedEntry* entries, uint16_t count, const char* uid) {
    for(uint16_t i = 0; i < count; ++i) {
        if(strcasecmp(entries[i].info.uid_hex, uid) == 0) return true;
    }
    return false;
}

bool add_entry(
    Storage* storage,
    BrSavedEntry* entries,
    uint16_t* count,
    uint16_t max_entries,
    const char* full_path,
    const char* source_name) {
    if(*count >= max_entries) return false;

    BrTagInfo parsed;
    if(!br_load_tag_file(storage, full_path, &parsed)) return false;
    if(already_listed(entries, *count, parsed.uid_hex)) return true;

    BrSavedEntry& entry = entries[(*count)++];
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.filename, sizeof(entry.filename), "%s", source_name);
    entry.info = parsed;
    display_name_from_info(parsed, entry.display_name, sizeof(entry.display_name));
    return true;
}

bool load_bundle_folder(
    Storage* storage,
    BrSavedEntry* entries,
    uint16_t* count,
    uint16_t max_entries,
    const char* folder_name) {
    if(!safe_name(folder_name)) return false;

    char full_path[BR_PATH_MAX];
    char source_name[BR_PATH_MAX];

    int length = snprintf(
        full_path,
        sizeof(full_path),
        "%s/%s/hf-mf-%s.nfc",
        BR_TAGS_DIR,
        folder_name,
        folder_name);
    if(length > 0 && static_cast<size_t>(length) < sizeof(full_path)) {
        snprintf(source_name, sizeof(source_name), "%s/hf-mf-%s.nfc", folder_name, folder_name);
        if(add_entry(storage, entries, count, max_entries, full_path, source_name)) return true;
    }

    length = snprintf(
        full_path,
        sizeof(full_path),
        "%s/%s/hf-mf-%s-dump.bin",
        BR_TAGS_DIR,
        folder_name,
        folder_name);
    if(length > 0 && static_cast<size_t>(length) < sizeof(full_path)) {
        snprintf(
            source_name,
            sizeof(source_name),
            "%s/hf-mf-%s-dump.bin",
            folder_name,
            folder_name);
        if(add_entry(storage, entries, count, max_entries, full_path, source_name)) return true;
    }

    // Also accept compatible folders whose file base does not match the directory name.
    char folder_path[BR_PATH_MAX];
    length = snprintf(folder_path, sizeof(folder_path), "%s/%s", BR_TAGS_DIR, folder_name);
    if(length <= 0 || static_cast<size_t>(length) >= sizeof(folder_path)) return false;

    File* dir = storage_file_alloc(storage);
    if(!dir || !storage_dir_open(dir, folder_path)) {
        if(dir) storage_file_free(dir);
        return false;
    }

    bool found = false;
    FileInfo info;
    char filename[BR_NAME_MAX];
    while(!found && storage_dir_read(dir, &info, filename, sizeof(filename))) {
        if((info.flags & FSF_DIRECTORY) || !safe_name(filename) || !supported_extension(filename)) continue;
        length = snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, filename);
        if(length <= 0 || static_cast<size_t>(length) >= sizeof(full_path)) continue;
        snprintf(source_name, sizeof(source_name), "%s/%s", folder_name, filename);
        found = add_entry(storage, entries, count, max_entries, full_path, source_name);
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    return found;
}
} // namespace

void br_storage_init(Storage* storage) {
    if(!storage) return;
    storage_common_mkdir(storage, BR_DATA_DIR);
    storage_common_mkdir(storage, BR_TAGS_DIR);
}

bool br_saved_path(const char* filename, char* out, size_t out_size) {
    if(!safe_relative_path(filename) || !out || out_size == 0U) return false;
    const int length = snprintf(out, out_size, "%s/%s", BR_TAGS_DIR, filename);
    return length > 0 && static_cast<size_t>(length) < out_size;
}

uint16_t br_saved_scan(Storage* storage, BrSavedEntry* entries, uint16_t max_entries) {
    if(!storage || !entries || max_entries == 0U) return 0;

    File* dir = storage_file_alloc(storage);
    if(!dir || !storage_dir_open(dir, BR_TAGS_DIR)) {
        if(dir) storage_file_free(dir);
        return 0;
    }

    uint16_t count = 0;
    FileInfo info;
    char filename[BR_NAME_MAX];
    char path[BR_PATH_MAX];
    while(count < max_entries && storage_dir_read(dir, &info, filename, sizeof(filename))) {
        if(!safe_name(filename)) continue;

        if(info.flags & FSF_DIRECTORY) {
            load_bundle_folder(storage, entries, &count, max_entries, filename);
            continue;
        }

        // Backward compatibility with the older flat .nfc/.bin save layout.
        if(!supported_extension(filename) || !br_saved_path(filename, path, sizeof(path))) continue;
        add_entry(storage, entries, &count, max_entries, path, filename);
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    sort_entries(entries, count);
    return count;
}

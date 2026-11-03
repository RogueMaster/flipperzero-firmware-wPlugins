#include "clipboard_library.h"
#include "transfer_protocol.h"
#include <furi.h>
#include <stdio.h>
#include <string.h>

#define LIBRARY_PATH APP_DATA_PATH("saved")

static void file_path(const uint8_t id[16], const char* extension, char output[128]) {
    char text[33];
    clipboard_saved_id(id, text);
    snprintf(output, 128, LIBRARY_PATH "/%s.%s", text, extension);
}

static bool owned_name(const char* name, const char* extension) {
    if(strlen(name) != 36 || name[32] != '.' || strcmp(name + 33, extension) != 0) return false;
    for(unsigned i = 0; i < 32; i++) {
        if(!((name[i] >= '0' && name[i] <= '9') || (name[i] >= 'a' && name[i] <= 'f')))
            return false;
    }
    return true;
}

static bool header_read(File* file, ClipboardSaved* saved) {
    uint8_t header[CLIPBOARD_SAVED_HEADER];
    return !storage_file_is_dir(file) &&
           storage_file_read(file, header, sizeof(header)) == sizeof(header) &&
           clipboard_saved_decode(header, saved) &&
           storage_file_size(file) == sizeof(header) + saved->size;
}

static bool body_read(void* context, size_t offset, uint8_t* output, size_t size) {
    File* file = context;
    return storage_file_seek(file, (uint32_t)(CLIPBOARD_SAVED_HEADER + offset), true) &&
           storage_file_read(file, output, size) == size;
}

static bool verify_file(ClipboardLibrary* library, const char* path) {
    File* file = storage_file_alloc(library->storage);
    ClipboardSaved saved;
    bool ok = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
              header_read(file, &saved);
    if(ok) {
        char id[33];
        clipboard_saved_id(saved.id, id);
        const char* name = strrchr(path, '/');
        ok = name && strncmp(name + 1, id, 32) == 0;
        ClipboardReader reader = {file, saved.size, body_read};
        ok = ok && clipboard_saved_verify(&saved, &reader);
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

static void recover_backups(ClipboardLibrary* library) {
    char backups[CLIPBOARD_SAVED_LIMIT][37];
    size_t count = 0;
    File* directory = storage_file_alloc(library->storage);
    if(storage_dir_open(directory, LIBRARY_PATH)) {
        char name[64];
        while(count < CLIPBOARD_SAVED_LIMIT &&
              storage_dir_read(directory, NULL, name, sizeof(name))) {
            if(owned_name(name, "bak")) memcpy(backups[count++], name, sizeof(backups[0]));
        }
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    for(size_t i = 0; i < count; i++) {
        char backup[128], final[128];
        snprintf(backup, sizeof(backup), LIBRARY_PATH "/%.36s", backups[i]);
        memcpy(backups[i] + 33, "bcm", 4);
        snprintf(final, sizeof(final), LIBRARY_PATH "/%.36s", backups[i]);
        if(verify_file(library, final))
            storage_simply_remove(library->storage, backup);
        else if(verify_file(library, backup)) {
            if(storage_file_exists(library->storage, final))
                storage_simply_remove(library->storage, final);
            storage_common_rename(library->storage, backup, final);
        }
    }
}

void clipboard_library_refresh(ClipboardLibrary* library) {
    library->count = 0;
    if(!storage_dir_exists(library->storage, LIBRARY_PATH)) return;
    recover_backups(library);
    File* directory = storage_file_alloc(library->storage);
    File* file = storage_file_alloc(library->storage);
    if(storage_dir_open(directory, LIBRARY_PATH)) {
        char name[64], path[128], id[33];
        while(library->count < CLIPBOARD_SAVED_LIMIT &&
              storage_dir_read(directory, NULL, name, sizeof(name))) {
            if(!owned_name(name, "bcm")) continue;
            snprintf(path, sizeof(path), LIBRARY_PATH "/%s", name);
            ClipboardSaved saved;
            bool ok = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                      header_read(file, &saved);
            storage_file_close(file);
            if(!ok) continue;
            clipboard_saved_id(saved.id, id);
            if(strncmp(id, name, 32) != 0) continue;
            library->entries[library->count++] = saved;
        }
    }
    storage_file_free(file);
    storage_dir_close(directory);
    storage_file_free(directory);
    for(size_t i = 1; i < library->count; i++) {
        for(size_t j = i;
            j > 0 && strcmp(library->entries[j - 1].name, library->entries[j].name) > 0;
            j--) {
            ClipboardSaved swap = library->entries[j];
            library->entries[j] = library->entries[j - 1];
            library->entries[j - 1] = swap;
        }
    }
}

bool clipboard_library_save(
    ClipboardLibrary* library,
    const ClipboardSaved* saved,
    const ClipboardReader* reader) {
    uint8_t header[CLIPBOARD_SAVED_HEADER];
    if(!clipboard_saved_encode(saved, header) || !clipboard_saved_verify(saved, reader) ||
       !storage_simply_mkdir(library->storage, APP_DATA_PATH("")) ||
       !storage_simply_mkdir(library->storage, LIBRARY_PATH))
        return false;
    clipboard_library_refresh(library);
    bool replacing = false;
    for(size_t i = 0; i < library->count; i++)
        replacing |= memcmp(library->entries[i].id, saved->id, 16) == 0;
    if(library->count >= CLIPBOARD_SAVED_LIMIT && !replacing) return false;
    char temporary[128], final[128], backup[128];
    file_path(saved->id, "tmp", temporary);
    file_path(saved->id, "bcm", final);
    file_path(saved->id, "bak", backup);
    if(storage_file_exists(library->storage, backup)) return false;
    File* file = storage_file_alloc(library->storage);
    bool ok = storage_file_open(file, temporary, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              storage_file_write(file, header, sizeof(header)) == sizeof(header);
    uint8_t bytes[256];
    for(size_t offset = 0; ok && offset < reader->size;) {
        size_t size = MIN(sizeof(bytes), reader->size - offset);
        ok = reader->read(reader->context, offset, bytes, size) &&
             storage_file_write(file, bytes, size) == size;
        offset += size;
    }
    if(ok) ok = storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    if(!ok || !verify_file(library, temporary)) return false;
    bool had_previous = storage_file_exists(library->storage, final);
    if(had_previous && storage_common_rename(library->storage, final, backup) != FSE_OK)
        return false;
    ok = storage_common_rename(library->storage, temporary, final) == FSE_OK &&
         verify_file(library, final);
    if(!ok) {
        if(had_previous) {
            storage_simply_remove(library->storage, final);
            storage_common_rename(library->storage, backup, final);
        }
        return false;
    }
    if(had_previous) storage_simply_remove(library->storage, backup);
    clipboard_library_refresh(library);
    return true;
}

bool clipboard_library_load(
    ClipboardLibrary* library,
    const ClipboardSaved* saved,
    void* context,
    bool (*write)(void*, size_t, const uint8_t*, size_t)) {
    char path[128];
    file_path(saved->id, "bcm", path);
    File* file = storage_file_alloc(library->storage);
    ClipboardSaved actual;
    bool ok = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
              header_read(file, &actual);
    uint8_t expected[CLIPBOARD_SAVED_HEADER], received[CLIPBOARD_SAVED_HEADER];
    if(ok)
        ok = clipboard_saved_encode(saved, expected) &&
             clipboard_saved_encode(&actual, received) &&
             memcmp(expected, received, sizeof(expected)) == 0;
    uint8_t bytes[256];
    uint32_t crc = 0xFFFFFFFFU;
    for(size_t offset = 0; ok && offset < saved->size;) {
        size_t size = MIN(sizeof(bytes), saved->size - offset);
        ok = storage_file_read(file, bytes, size) == size && write(context, offset, bytes, size);
        if(ok) crc = clipboard_crc_update(crc, bytes, size);
        offset += size;
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok && (crc ^ 0xFFFFFFFFU) == saved->crc;
}

bool clipboard_library_remove(ClipboardLibrary* library, const ClipboardSaved* saved) {
    char path[128], backup[128], temporary[128];
    file_path(saved->id, "bcm", path);
    file_path(saved->id, "bak", backup);
    file_path(saved->id, "tmp", temporary);
    // Remove the recovery copy first so a deleted entry cannot reappear after restart.
    if(!storage_simply_remove(library->storage, backup) ||
       !storage_simply_remove(library->storage, temporary) ||
       !storage_simply_remove(library->storage, path))
        return false;
    clipboard_library_refresh(library);
    return true;
}

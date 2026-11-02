#pragma once

#include <strings.h>

/* Enumerate existing components just as the file picker does. The workspace
 * and directory handle are released before header inspection or authentication.
 * Mount names are firmware selectors; only SD filenames are case insensitive. */
static bool launch_resolve_database_path(Storage* storage, const char* path, FuriString* out) {
    furi_string_set_str(out, path);
    if(!storage_file_exists(storage, path)) return true;
    const bool external = strncmp(path, "/ext/", 5U) == 0;
    if(!external && strncmp(path, "/int/", 5U) != 0) return true;
    char* resolved = launch_alloc(FLIPPASS_LAUNCH_PATH_MAX_BYTES + 1U);
    if(resolved == NULL) return false;
    File* directory = storage_file_alloc(storage);
    if(directory == NULL) {
        free(resolved);
        return false;
    }
    memcpy(resolved, path, 4U);
    size_t used = 4U;
    resolved[used] = '\0';
    const char* component = path + 5U;
    bool ok = true;
    while(*component != '\0' && ok) {
        const char* end = strchr(component, '/');
        if(end == NULL) end = component + strlen(component);
        const size_t length = (size_t)(end - component);
        const char* next = end;
        while(*next == '/')
            ++next;
        if(length == 0U || (length == 1U && component[0] == '.')) {
            component = next;
            continue;
        }
        if(length == 2U && component[0] == '.' && component[1] == '.') {
            if(used == 4U) {
                ok = false;
                break;
            }
            while(used > 4U && resolved[used - 1U] != '/')
                --used;
            resolved[--used] = '\0';
            component = next;
            continue;
        }
        char name[256];
        FileInfo info;
        bool found = false;
        if(storage_dir_open(directory, resolved)) {
            while(storage_dir_read(directory, &info, name, sizeof(name))) {
                if(strlen(name) != length || (external ? strncasecmp(name, component, length) :
                                                         strncmp(name, component, length)))
                    continue;
                if((*next != '\0') != ((info.flags & FSF_DIRECTORY) != 0U)) break;
                if(used + 1U + length > FLIPPASS_LAUNCH_PATH_MAX_BYTES) break;
                resolved[used++] = '/';
                memcpy(resolved + used, name, length + 1U);
                used += length;
                found = true;
                break;
            }
        }
        storage_dir_close(directory);
        ok = found;
        component = next;
    }
    if(ok) furi_string_set_str(out, resolved);
    storage_file_free(directory);
    free(resolved);
    return ok;
}

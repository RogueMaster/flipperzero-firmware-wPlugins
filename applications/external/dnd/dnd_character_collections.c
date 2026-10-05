#include "dnd_character_collections.h"

#include "dnd_profile_handoff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dnd_fs.h"
#define DND_CHARACTER_COLLECTION_LINE_MAX 128U
#define DND_LANGUAGES_HEADER              "DNDLanguages=1\n"
#define DND_PROFICIENCIES_HEADER          "DNDProficiencies=1\n"

static void dnd_character_copy(char* output, size_t size, const char* input) {
    if(!output || !size) return;
    const char* source = input ? input : "";
    size_t length = strlen(source);
    if(length >= size) length = size - 1U;
    memcpy(output, source, length);
    output[length] = '\0';
}

void dnd_character_languages_path(char* output, size_t size, uint32_t profile) {
    if(!output || !size) return;
    snprintf(
        output, size, "%s/languages_%lu.txt", DND_CHARACTER_DATA_ROOT, (unsigned long)profile);
}

void dnd_character_proficiencies_path(char* output, size_t size, uint32_t profile) {
    if(!output || !size) return;
    snprintf(
        output, size, "%s/proficiencies_%lu.txt", DND_CHARACTER_DATA_ROOT, (unsigned long)profile);
}

static bool dnd_character_append_line(
    Storage* storage,
    const char* path,
    const char* header,
    const char* line) {
    if(!storage || !path || !header || !line || !line[0]) return false;
    storage_common_mkdir(storage, DND_CHARACTER_DATA_ROOT);
    char temp[DND_FS_LONG_PATH_LEN], backup[DND_FS_LONG_PATH_LEN];
    snprintf(temp, sizeof(temp), "%s.tmp", path);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    if(!input || !output) {
        if(input) storage_file_free(input);
        if(output) storage_file_free(output);
        return false;
    }
    bool exists = storage_file_exists(storage, path);
    bool ok = (!exists || storage_file_open(input, path, FSAM_READ, FSOM_OPEN_EXISTING)) &&
              storage_file_open(output, temp, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    uint8_t buffer[96];
    size_t bytes = 0U;
    char last = '\n';
    while(ok && exists) {
        size_t n = storage_file_read(input, buffer, sizeof(buffer));
        if(!n) break;
        ok = storage_file_write(output, buffer, n) == n;
        bytes += n;
        last = (char)buffer[n - 1U];
    }
    if(ok && exists) ok = storage_file_get_error(input) == FSE_OK;
    if(ok && !bytes) ok = storage_file_write(output, header, strlen(header)) == strlen(header);
    if(ok && last != '\n') ok = storage_file_write(output, "\n", 1U) == 1U;
    if(ok)
        ok = storage_file_write(output, line, strlen(line)) == strlen(line) &&
             storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    if(ok) ok = dnd_fs_publish(storage, temp, path, backup);
    if(!ok) storage_common_remove(storage, temp);
    return ok;
}

typedef bool (*DndCharacterLineVisitor)(uint16_t index, const char* line, void* context);

static bool dnd_character_visit(
    Storage* storage,
    const char* path,
    char prefix,
    DndCharacterLineVisitor visitor,
    void* context,
    uint16_t* total_out) {
    if(total_out) *total_out = 0U;
    if(!storage || !path) return false;
    if(!storage_file_exists(storage, path)) return true;
    File* file = storage_file_alloc(storage);
    if(!file) return false;
    bool ok = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    char line[DND_CHARACTER_COLLECTION_LINE_MAX];
    size_t used = 0U;
    uint16_t total = 0U;
    uint8_t buffer[96];
    while(ok) {
        size_t got = storage_file_read(file, buffer, sizeof(buffer));
        if(!got) break;
        for(size_t i = 0U; i < got; ++i) {
            char ch = (char)buffer[i];
            if(ch == '\r') continue;
            if(ch != '\n') {
                if(used + 1U < sizeof(line)) line[used++] = ch;
                continue;
            }
            line[used] = '\0';
            used = 0U;
            if(line[0] != '\0' && line[1] != '\0' && line[0] == prefix && line[1] == '|') {
                bool keep = !visitor || visitor(total, line + 2U, context);
                if(total < UINT16_MAX) ++total;
                if(!keep) goto done;
            }
        }
    }
    if(used) {
        line[used] = '\0';
        if(line[0] != '\0' && line[1] != '\0' && line[0] == prefix && line[1] == '|') {
            bool keep = !visitor || visitor(total, line + 2U, context);
            if(total < UINT16_MAX) ++total;
            if(!keep) goto done;
        }
    }
done:
    if(storage_file_get_error(file) != FSE_OK) ok = false;
    storage_file_close(file);
    storage_file_free(file);
    if(total_out) *total_out = total;
    return ok;
}

typedef struct {
    uint16_t start;
    uint8_t count;
    char (*languages)[DND_CATALOG_NAME_LEN];
} DndLanguageWindow;

static bool
    dnd_character_language_window_visitor(uint16_t index, const char* line, void* context) {
    DndLanguageWindow* window = context;
    if(index < window->start) return true;
    if(window->count >= DND_CHARACTER_COLLECTION_WINDOW) return true;
    dnd_character_copy(window->languages[window->count], DND_CATALOG_NAME_LEN, line);
    ++window->count;
    return true;
}

bool dnd_character_languages_count(Storage* storage, uint32_t profile, uint16_t* total) {
    char path[DND_FS_PATH_LEN];
    dnd_character_languages_path(path, sizeof(path), profile);
    return dnd_character_visit(storage, path, 'L', NULL, NULL, total);
}

bool dnd_character_languages_load_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    char entries[DND_CHARACTER_COLLECTION_WINDOW][DND_CATALOG_NAME_LEN],
    uint8_t* count,
    uint16_t* total) {
    if(!entries || !count) return false;
    memset(entries, 0, DND_CHARACTER_COLLECTION_WINDOW * DND_CATALOG_NAME_LEN);
    DndLanguageWindow window = {.start = start, .count = 0U, .languages = entries};
    char path[DND_FS_PATH_LEN];
    dnd_character_languages_path(path, sizeof(path), profile);
    bool ok = dnd_character_visit(
        storage, path, 'L', dnd_character_language_window_visitor, &window, total);
    *count = window.count;
    return ok;
}

typedef struct {
    const char* name;
    bool found;
} DndLanguageContains;
static bool
    dnd_character_language_contains_visitor(uint16_t index, const char* line, void* context) {
    (void)index;
    DndLanguageContains* lookup = context;
    if(!strcmp(line, lookup->name)) {
        lookup->found = true;
        return false;
    }
    return true;
}

bool dnd_character_languages_contains(
    Storage* storage,
    uint32_t profile,
    const char* name,
    bool* found) {
    if(!found || !name) return false;
    *found = false;
    DndLanguageContains lookup = {.name = name, .found = false};
    char path[DND_FS_PATH_LEN];
    dnd_character_languages_path(path, sizeof(path), profile);
    bool ok = dnd_character_visit(
        storage, path, 'L', dnd_character_language_contains_visitor, &lookup, NULL);
    *found = lookup.found;
    return ok;
}

bool dnd_character_languages_append(Storage* storage, uint32_t profile, const char* name) {
    if(!name || !name[0] || strlen(name) >= DND_CATALOG_NAME_LEN || strchr(name, '\n') ||
       strchr(name, '\r') || strchr(name, '|'))
        return false;
    bool found = false;
    if(!dnd_character_languages_contains(storage, profile, name, &found) || found)
        return !found ? false : true;
    char path[DND_FS_PATH_LEN];
    dnd_character_languages_path(path, sizeof(path), profile);
    char line[DND_CHARACTER_COLLECTION_LINE_MAX];
    int n = snprintf(line, sizeof(line), "L|%s\n", name);
    return n > 0 && (size_t)n < sizeof(line) &&
           dnd_character_append_line(storage, path, DND_LANGUAGES_HEADER, line);
}

typedef struct {
    uint16_t start;
    uint8_t count;
    DndCharacterProficiency* entries;
} DndProficiencyWindow;

static bool dnd_character_proficiency_parse(const char* line, DndCharacterProficiency* entry) {
    if(!line || !entry) return false;
    const char* separator = strchr(line, '|');
    if(!separator || separator == line || !separator[1]) return false;
    size_t type_len = (size_t)(separator - line);
    if(type_len >= sizeof(entry->type)) type_len = sizeof(entry->type) - 1U;
    memcpy(entry->type, line, type_len);
    entry->type[type_len] = '\0';
    dnd_character_copy(entry->name, sizeof(entry->name), separator + 1U);
    return entry->name[0] != '\0';
}

static bool
    dnd_character_proficiency_window_visitor(uint16_t index, const char* line, void* context) {
    DndProficiencyWindow* window = context;
    if(index < window->start || window->count >= DND_CHARACTER_COLLECTION_WINDOW) return true;
    if(dnd_character_proficiency_parse(line, &window->entries[window->count])) ++window->count;
    return true;
}

bool dnd_character_proficiencies_count(Storage* storage, uint32_t profile, uint16_t* total) {
    char path[DND_FS_PATH_LEN];
    dnd_character_proficiencies_path(path, sizeof(path), profile);
    return dnd_character_visit(storage, path, 'P', NULL, NULL, total);
}

bool dnd_character_proficiencies_load_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacterProficiency entries[DND_CHARACTER_COLLECTION_WINDOW],
    uint8_t* count,
    uint16_t* total) {
    if(!entries || !count) return false;
    memset(entries, 0, sizeof(DndCharacterProficiency) * DND_CHARACTER_COLLECTION_WINDOW);
    DndProficiencyWindow window = {.start = start, .count = 0U, .entries = entries};
    char path[DND_FS_PATH_LEN];
    dnd_character_proficiencies_path(path, sizeof(path), profile);
    bool ok = dnd_character_visit(
        storage, path, 'P', dnd_character_proficiency_window_visitor, &window, total);
    *count = window.count;
    return ok;
}

typedef struct {
    const char* type;
    const char* name;
    bool found;
} DndProficiencyContains;
static bool
    dnd_character_proficiency_contains_visitor(uint16_t index, const char* line, void* context) {
    (void)index;
    DndProficiencyContains* lookup = context;
    DndCharacterProficiency entry;
    memset(&entry, 0, sizeof(entry));
    if(dnd_character_proficiency_parse(line, &entry) && !strcmp(entry.type, lookup->type) &&
       !strcmp(entry.name, lookup->name)) {
        lookup->found = true;
        return false;
    }
    return true;
}

bool dnd_character_proficiencies_contains(
    Storage* storage,
    uint32_t profile,
    const char* type,
    const char* name,
    bool* found) {
    if(!found || !type || !name) return false;
    *found = false;
    DndProficiencyContains lookup = {.type = type, .name = name, .found = false};
    char path[DND_FS_PATH_LEN];
    dnd_character_proficiencies_path(path, sizeof(path), profile);
    bool ok = dnd_character_visit(
        storage, path, 'P', dnd_character_proficiency_contains_visitor, &lookup, NULL);
    *found = lookup.found;
    return ok;
}

bool dnd_character_proficiencies_append(
    Storage* storage,
    uint32_t profile,
    const char* type,
    const char* name) {
    if(!type || !name || !type[0] || !name[0] ||
       strlen(type) >= DND_CHARACTER_PROFICIENCY_TYPE_LEN ||
       strlen(name) >= DND_CATALOG_NAME_LEN || strchr(type, '\r') || strchr(name, '\r') ||
       strchr(type, '|') || strchr(name, '|') || strchr(type, '\n') || strchr(name, '\n'))
        return false;
    bool found = false;
    if(!dnd_character_proficiencies_contains(storage, profile, type, name, &found) || found)
        return !found ? false : true;
    char path[DND_FS_PATH_LEN];
    dnd_character_proficiencies_path(path, sizeof(path), profile);
    char line[DND_CHARACTER_COLLECTION_LINE_MAX];
    int n = snprintf(line, sizeof(line), "P|%s|%s\n", type, name);
    return n > 0 && (size_t)n < sizeof(line) &&
           dnd_character_append_line(storage, path, DND_PROFICIENCIES_HEADER, line);
}

typedef struct {
    File* output;
    uint16_t target;
    const char* replacement;
    char prefix;
    bool found;
    bool ok;
} DndCollectionRewrite;

static bool dnd_character_rewrite_visitor(uint16_t index, const char* line, void* context) {
    DndCollectionRewrite* rewrite = context;
    if(index == rewrite->target) {
        rewrite->found = true;
        if(!rewrite->replacement) return true;
        line = rewrite->replacement;
    }
    char prefix[2] = {rewrite->prefix, '|'};
    rewrite->ok = storage_file_write(rewrite->output, prefix, sizeof(prefix)) == sizeof(prefix) &&
                  storage_file_write(rewrite->output, line, strlen(line)) == strlen(line) &&
                  storage_file_write(rewrite->output, "\n", 1U) == 1U;
    return rewrite->ok;
}

static bool dnd_character_rewrite_line(
    Storage* storage,
    const char* path,
    const char* header,
    char prefix,
    uint16_t index,
    const char* replacement) {
    if(!storage || !path || !storage_file_exists(storage, path)) return false;
    char temp[DND_FS_LONG_PATH_LEN], backup[DND_FS_LONG_PATH_LEN];
    snprintf(temp, sizeof(temp), "%s.tmp", path);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    File* output = storage_file_alloc(storage);
    if(!output) return false;
    bool ok = storage_file_open(output, temp, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              storage_file_write(output, header, strlen(header)) == strlen(header);
    DndCollectionRewrite rewrite = {
        .output = output,
        .target = index,
        .replacement = replacement,
        .prefix = prefix,
        .ok = true,
        .found = false};
    if(ok)
        ok = dnd_character_visit(
                 storage, path, prefix, dnd_character_rewrite_visitor, &rewrite, NULL) &&
             rewrite.ok && rewrite.found && storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    if(ok) ok = dnd_fs_publish(storage, temp, path, backup);
    if(!ok) storage_common_remove(storage, temp);
    return ok;
}

bool dnd_character_languages_replace(
    Storage* storage,
    uint32_t profile,
    uint16_t index,
    const char* name) {
    if(!name || !name[0] || strlen(name) >= DND_CATALOG_NAME_LEN || strchr(name, '|') ||
       strchr(name, '\r') || strchr(name, '\n'))
        return false;
    char path[DND_FS_PATH_LEN];
    dnd_character_languages_path(path, sizeof(path), profile);
    return dnd_character_rewrite_line(storage, path, DND_LANGUAGES_HEADER, 'L', index, name);
}

bool dnd_character_proficiencies_replace(
    Storage* storage,
    uint32_t profile,
    uint16_t index,
    const char* type,
    const char* name) {
    if(!type || !type[0] || !name || !name[0] ||
       strlen(type) >= DND_CHARACTER_PROFICIENCY_TYPE_LEN ||
       strlen(name) >= DND_CATALOG_NAME_LEN || strchr(type, '|') || strchr(type, '\r') ||
       strchr(type, '\n') || strchr(name, '|') || strchr(name, '\r') || strchr(name, '\n'))
        return false;
    char path[DND_FS_PATH_LEN], line[DND_CHARACTER_COLLECTION_LINE_MAX];
    dnd_character_proficiencies_path(path, sizeof(path), profile);
    snprintf(line, sizeof(line), "%s|%s", type, name);
    return dnd_character_rewrite_line(storage, path, DND_PROFICIENCIES_HEADER, 'P', index, line);
}

bool dnd_character_languages_delete(Storage* storage, uint32_t profile, uint16_t logical_index) {
    char path[DND_FS_PATH_LEN];
    dnd_character_languages_path(path, sizeof(path), profile);
    return dnd_character_rewrite_line(
        storage, path, DND_LANGUAGES_HEADER, 'L', logical_index, NULL);
}

bool dnd_character_proficiencies_delete(Storage* storage, uint32_t profile, uint16_t logical_index) {
    char path[DND_FS_PATH_LEN];
    dnd_character_proficiencies_path(path, sizeof(path), profile);
    return dnd_character_rewrite_line(
        storage, path, DND_PROFICIENCIES_HEADER, 'P', logical_index, NULL);
}

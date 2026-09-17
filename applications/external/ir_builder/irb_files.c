#include "irb_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool fail(char* error, const char* text) {
    snprintf(error, IRB_ERROR_SIZE, "%s", text);
    return false;
}
static bool report(IrbLoadProgress progress, void* context, IrbLoadPhase phase, uint32_t done,
                   uint32_t total) {
    return !progress || progress(phase, done, total, context);
}
static bool same(Storage* storage, const IrbCatalog* catalog, IrbLoadProgress progress,
                 void* context) {
    uint32_t hash, size;
    return irb_file_fingerprint(storage, catalog->path, &hash, &size, IrbLoadVerify, progress,
                                context) &&
           hash == catalog->hash && size == catalog->size;
}
static void catalog_label(char output[IRB_NAME_SIZE], const char* input) {
    unsigned length = 0;
    while(*input && length < IRB_NAME_SIZE - 1) {
        unsigned char c = (unsigned char)*input++;
        output[length++] = c >= 32 && c <= 126 ? (char)c : '_';
    }
    while(length && output[length - 1] == ' ')
        --length;
    unsigned start = 0;
    while(start < length && output[start] == ' ')
        ++start;
    if(start) {
        memmove(output, output + start, length - start);
        length -= start;
    }
    if(!length) {
        memcpy(output, "Button", 7);
        return;
    }
    output[length] = 0;
}
bool irb_catalog_load(Storage* storage, IrbCatalog* catalog, const char* path, char* error,
                      IrbLoadProgress progress, void* context) {
    memset(catalog, 0, sizeof(*catalog));
    if(!irb_library_path_valid(path)) return fail(error, "Choose an SD-card .ir file.");
    snprintf(catalog->path, sizeof(catalog->path), "%s", path);
    if(!irb_file_fingerprint(storage, path, &catalog->hash, &catalog->size, IrbLoadHash, progress,
                             context))
        return fail(error, "Cannot read import file.");
    FlipperFormat* reader = flipper_format_buffered_file_alloc(storage);
    FuriString* name = furi_string_alloc();
    InfraredSignal* signal = infrared_signal_alloc();
    uint32_t version;
    bool ok = flipper_format_buffered_file_open_existing(reader, path);
    flipper_format_set_strict_mode(reader, true);
    ok = ok && flipper_format_read_header(reader, name, &version) && version == 1 &&
         (furi_string_equal(name, "IR signals file") || furi_string_equal(name, "IR library file"));
    while(ok) {
        if(!report(progress, context, IrbLoadIndex, flipper_format_tell(reader), catalog->size)) {
            ok = false;
            break;
        }
        if(infrared_signal_read_name(reader, name) != InfraredErrorCodeNone) {
            ok = flipper_format_tell(reader) + 1 >= catalog->size;
            break;
        }
        if(catalog->count == IRB_CATALOG_LIMIT) {
            ok = false;
            break;
        }
        IrbImportEntry* entry = &catalog->entries[catalog->count++];
        entry->offset = flipper_format_tell(reader);
        entry->usable = true;
        catalog_label(entry->name, furi_string_get_cstr(name));
        ok = infrared_signal_read_body(signal, reader) == InfraredErrorCodeNone &&
             infrared_signal_is_valid(signal);
    }
    flipper_format_free(reader); // Close before fingerprinting the same path.
    infrared_signal_free(signal);
    furi_string_free(name);
    return (ok && catalog->count && same(storage, catalog, progress, context)) ||
           fail(error, "Invalid import file, changed file, or more than 256 buttons.");
}
static bool copy_source(Storage* storage, const IrbCatalog* catalog, char* target,
                        IrbLoadProgress progress, void* context) {
    if(!irb_storage_prepare(storage)) return false;
    bool available = false;
    for(unsigned i = 1; i <= 9999; ++i) {
        snprintf(target, IRB_PATH_SIZE, IRB_IMPORT_DIR "/IR_%04u.ir", i);
        if(storage_common_stat(storage, target, NULL) == FSE_NOT_EXIST) {
            available = true;
            break;
        }
    }
    if(!available) return false;
    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    bool created = false;
    bool ok = storage_file_open(input, catalog->path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(ok) ok = created = storage_file_open(output, target, FSAM_WRITE, FSOM_CREATE_NEW);
    uint8_t bytes[512];
    uint32_t done = 0;
    while(ok && done < catalog->size) {
        size_t amount = catalog->size - done;
        if(amount > sizeof(bytes)) amount = sizeof(bytes);
        ok = report(progress, context, IrbLoadIndex, done, catalog->size) &&
             storage_file_read(input, bytes, amount) == amount &&
             storage_file_write(output, bytes, amount) == amount;
        done += amount;
    }
    storage_file_close(input);
    if(!storage_file_close(output)) ok = false;
    storage_file_free(input);
    storage_file_free(output);
    uint32_t hash, size;
    ok = ok &&
         irb_file_fingerprint(storage, target, &hash, &size, IrbLoadVerify, progress, context) &&
         hash == catalog->hash && size == catalog->size;
    if(!ok && created) storage_common_remove(storage, target);
    return ok;
}
static void normalized(char output[IRB_NAME_SIZE], const char* input) {
    unsigned length = 0;
    while(*input && length < IRB_NAME_SIZE - 1) {
        char c = *input++;
        if(c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) output[length++] = c;
    }
    output[length] = 0;
}
static unsigned map_score(const char* name, unsigned target) {
    static const char* const exact[IRB_POSITION_SLOTS][6] = {
        {"poweron", "turnon", "on", NULL, NULL, NULL},
        {"poweroff", "turnoff", "off", NULL, NULL, NULL},
        {"volumeup", "volup", "volumeplus", "volplus", NULL, NULL},
        {"volumedown", "voldown", "volumeminus", "volminus", NULL, NULL},
        {"channelup", "chup", "channelnext", "chnext", "channelplus", "programup"},
        {"channeldown", "chdown", "channelprev", "chprev", "channelminus", "programdown"},
        {"mute", "mutetoggle", NULL, NULL, NULL, NULL},
        {"unmute", NULL, NULL, NULL, NULL, NULL},
        {"up", "arrowup", "dpadup", "cursorup", NULL, NULL},
        {"left", "arrowleft", "dpadleft", "cursorleft", NULL, NULL},
        {"ok", "enter", "select", "confirm", "center", "centre"},
        {"right", "arrowright", "dpadright", "cursorright", NULL, NULL},
        {"down", "arrowdown", "dpaddown", "cursordown", NULL, NULL},
        {"menu", NULL, NULL, NULL, NULL, NULL},
        {"back", "return", NULL, NULL, NULL, NULL},
        {"input", "source", "av", "tvinput", "tvav", NULL},
    };
    char clean[IRB_NAME_SIZE];
    normalized(clean, name);
    const char* alternate = !strncmp(clean, "button", 6) ? clean + 6
                            : !strncmp(clean, "key", 3)  ? clean + 3
                                                         : clean;
    for(unsigned i = 0; i < 6 && exact[target][i]; ++i)
        if(!strcmp(clean, exact[target][i]) || !strcmp(alternate, exact[target][i])) return 100 - i;
    if(target == 0 &&
       (!strcmp(clean, "power") || !strcmp(clean, "powertoggle") || !strcmp(clean, "standby")))
        return 50;
    bool plus = strchr(name, '+') != NULL, minus = strchr(name, '-') != NULL;
    if((target == 2 && plus) || (target == 3 && minus))
        if(!strcmp(clean, "vol") || !strcmp(clean, "volume")) return 90;
    if((target == 4 && plus) || (target == 5 && minus))
        if(!strcmp(clean, "ch") || !strcmp(clean, "channel")) return 90;
    return 0;
}
static uint32_t target_slot(unsigned target) {
    return target < IRB_SLOTS ? target : IRB_NAV_SLOT_BASE + target - IRB_SLOTS;
}
static bool label_equal(const char* left, const char* right) {
    while(*left && *right) {
        char a = *left++, b = *right++;
        if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if(a != b) return false;
    }
    return *left == *right;
}
static bool reference_used(const IrbProject* project, uint32_t source, uint32_t offset) {
    for(unsigned i = 0; i < IRB_POSITION_SLOTS; ++i)
        if(project->mapped[i].offset == offset && project->mapped[i].source == source) return true;
    for(unsigned i = 0; i < project->extra_count; ++i)
        if(project->extras[i].offset == offset && project->extras[i].source == source) return true;
    return false;
}
static bool reserved_label(const IrbProject* project, const char* label) {
    for(unsigned i = 0; i < IRB_SLOTS + project->extra_count; ++i)
        if(label_equal(label, irb_project_label(project, i))) return true;
    for(unsigned i = 0; i < IRB_NAV_KEYS; ++i)
        if(label_equal(label, irb_nav_labels[i])) return true;
    return false;
}
static bool unique_label(const IrbProject* project, const char* original,
                         char output[IRB_NAME_SIZE]) {
    snprintf(output, IRB_NAME_SIZE, "%s", original);
    if(!reserved_label(project, output)) return true;
    for(unsigned suffix = 2; suffix < 1000; ++suffix) {
        char number[8];
        snprintf(number, sizeof(number), " %u", suffix);
        size_t tail = strlen(number);
        size_t keep = strlen(original);
        if(keep + tail >= IRB_NAME_SIZE) keep = IRB_NAME_SIZE - tail - 1;
        memcpy(output, original, keep);
        memcpy(output + keep, number, tail + 1);
        if(!reserved_label(project, output)) return true;
    }
    return false;
}
bool irb_catalog_add(Storage* storage, const IrbCatalog* catalog, IrbProject* project, int entry,
                     uint32_t* added, uint32_t* mapped, char* error, IrbLoadProgress progress,
                     void* context) {
    error[0] = 0;
    *added = *mapped = 0;
    if(entry < -1 || entry >= (int)catalog->count || !irb_project_valid(project))
        return fail(error, "Invalid import selection.");
    if(!same(storage, catalog, progress, context)) return fail(error, "Import changed. Reopen it.");
    IrbProject* next = malloc(sizeof(*next));
    if(!next) return fail(error, "Not enough memory.");
    *next = *project;
    uint32_t source = next->import_count;
    for(unsigned i = 0; i < next->import_count; ++i) {
        if(next->imports[i].hash != catalog->hash || next->imports[i].size != catalog->size)
            continue;
        uint32_t hash, size;
        if(irb_file_fingerprint(storage, next->imports[i].path, &hash, &size, IrbLoadVerify,
                                progress, context) &&
           hash == catalog->hash && size == catalog->size) {
            source = i;
            break;
        }
    }
    bool ok = source < IRB_MAX_IMPORTS;
    bool used[IRB_CATALOG_LIMIT] = {0};
    for(unsigned target = 0; ok && target < IRB_POSITION_SLOTS; ++target) {
        uint32_t slot = target_slot(target);
        if(irb_project_active(next, slot)) continue;
        unsigned best = IRB_CATALOG_LIMIT, best_score = 0;
        for(unsigned i = 0; i < catalog->count; ++i) {
            if(used[i] || (entry >= 0 && (unsigned)entry != i) || !catalog->entries[i].usable ||
               reference_used(next, source, catalog->entries[i].offset))
                continue;
            unsigned score = map_score(catalog->entries[i].name, target);
            if(score > best_score) {
                best = i;
                best_score = score;
            }
        }
        if(best < IRB_CATALOG_LIMIT &&
           irb_project_set_imported(next, slot, source, catalog->entries[best].offset)) {
            used[best] = true;
            ++*mapped;
            ++*added;
        }
    }
    for(unsigned i = 0; ok && i < catalog->count; ++i) {
        if(used[i] || (entry >= 0 && (unsigned)entry != i)) continue;
        const IrbImportEntry* item = &catalog->entries[i];
        if(!item->usable || reference_used(next, source, item->offset)) continue;
        if(next->extra_count == IRB_MAX_EXTRAS) {
            ok = false;
            snprintf(error, IRB_ERROR_SIZE,
                     "Remote has too many unmatched keys. Limit: 48 Other buttons.");
            break;
        }
        char label[IRB_NAME_SIZE];
        if(!unique_label(next, item->name, label)) {
            ok = false;
            break;
        }
        IrbExtraButton* extra = &next->extras[next->extra_count++];
        snprintf(extra->label, sizeof(extra->label), "%s", label);
        extra->offset = item->offset;
        extra->source = source;
        ++*added;
    }
    ok = ok && *added;
    bool created = false;
    if(ok && source == next->import_count) {
        IrbImportSource* target = &next->imports[source];
        ok = copy_source(storage, catalog, target->path, progress, context);
        created = ok;
        target->hash = catalog->hash;
        target->size = catalog->size;
        ++next->import_count;
    }
    ok = ok && same(storage, catalog, progress, context) && irb_project_valid(next) &&
         report(progress, context, IrbLoadVerify, 1, 1);
    if(ok)
        *project = *next;
    else if(created)
        storage_common_remove(storage, next->imports[source].path);
    free(next);
    if(ok) return true;
    if(!error[0]) fail(error, "Nothing imported. Check the 64-key / 4-file limit and SD card.");
    return false;
}

void irb_files_clear(IrbFileList* files) {
    for(unsigned i = 0; i < files->count; ++i)
        free(files->entries[i].name);
    free(files->entries);
    memset(files, 0, sizeof(*files));
}
static int file_compare(const void* left, const void* right) {
    const IrbFileEntry* a = left;
    const IrbFileEntry* b = right;
    if(!strcmp(a->name, "..")) return -1;
    if(!strcmp(b->name, "..")) return 1;
    if(a->directory != b->directory) return a->directory ? -1 : 1;
    const unsigned char* x = (const unsigned char*)a->name;
    const unsigned char* y = (const unsigned char*)b->name;
    while(*x && *y) {
        unsigned char cx = *x++, cy = *y++;
        if(cx >= 'A' && cx <= 'Z') cx += 'a' - 'A';
        if(cy >= 'A' && cy <= 'Z') cy += 'a' - 'A';
        if(cx != cy) return cx < cy ? -1 : 1;
    }
    if(*x != *y) return *x ? 1 : -1;
    return strcmp(a->name, b->name);
}
static void file_sort(IrbFileList* files) {
    // In-place heapsort: qsort is not exported by the Flipper FAP API.
    IrbFileEntry* entries = files->entries;
    for(unsigned i = 1; i < files->count; ++i) {
        unsigned child = i;
        while(child) {
            unsigned parent = (child - 1) / 2;
            if(file_compare(&entries[parent], &entries[child]) >= 0) break;
            IrbFileEntry swap = entries[parent];
            entries[parent] = entries[child];
            entries[child] = swap;
            child = parent;
        }
    }
    for(unsigned end = files->count; end > 1;) {
        --end;
        IrbFileEntry swap = entries[0];
        entries[0] = entries[end];
        entries[end] = swap;
        unsigned parent = 0;
        while(parent * 2 + 1 < end) {
            unsigned child = parent * 2 + 1;
            if(child + 1 < end && file_compare(&entries[child], &entries[child + 1]) < 0) ++child;
            if(file_compare(&entries[parent], &entries[child]) >= 0) break;
            swap = entries[parent];
            entries[parent] = entries[child];
            entries[child] = swap;
            parent = child;
        }
    }
}
static bool file_add(IrbFileList* files, const char* name, bool directory) {
    size_t length = strlen(name) + 1;
    if(files->count == IRB_FILE_LIMIT || files->name_bytes + length > IRB_FILE_NAME_BYTES)
        return false;
    if(files->count == files->capacity) {
        unsigned capacity = files->capacity + 32;
        IrbFileEntry* entries = realloc(files->entries, capacity * sizeof(*entries));
        if(!entries) return false;
        files->entries = entries;
        files->capacity = capacity;
    }
    char* copy = malloc(length);
    if(!copy) return false;
    memcpy(copy, name, length);
    files->entries[files->count++] = (IrbFileEntry){.name = copy, .directory = directory};
    files->name_bytes += length;
    return true;
}
bool irb_files_load(Storage* storage, IrbFileList* files, const char* path, bool projects,
                    char* error, IrbLoadProgress progress, void* context) {
    irb_files_clear(files);
    snprintf(files->path, sizeof(files->path), "%s", path);
    files->projects = projects;
    File* directory = storage_file_alloc(storage);
    bool ok = storage_dir_open(directory, path);
    FileInfo info;
    char name[IRB_PATH_SIZE];
    bool parent = !projects && !irb_path_equal(path, "/ext");
    while(ok) {
        bool is_dir;
        if(parent) {
            strcpy(name, "..");
            is_dir = true;
            parent = false;
        } else {
            if(!storage_dir_read(directory, &info, name, sizeof(name))) {
                // Flipper SD storage reports FSE_NOT_EXIST at normal directory EOF.
                FS_Error result = storage_file_get_error(directory);
                ok = result == FSE_OK || result == FSE_NOT_EXIST;
                break;
            }
            is_dir = (info.flags & FSF_DIRECTORY) != 0;
            if(!strcmp(name, ".") || !strcmp(name, "..")) continue;
            const char* extension = strrchr(name, '.');
            if(projects ? (is_dir || !extension || !irb_path_equal(extension, ".irb"))
                        : (!is_dir && (!extension || !irb_path_equal(extension, ".ir"))))
                continue;
        }
        if(!file_add(files, name, is_dir)) {
            ok = fail(error, "Folder too large for memory. Use smaller subfolders (512 entries / "
                             "16 KiB of names).");
            break;
        }
        ok = report(progress, context, IrbLoadIndex, 0, 0);
    }
    storage_dir_close(directory);
    storage_file_free(directory);
    if(ok) {
        file_sort(files);
    } else {
        irb_files_clear(files);
        if(!error[0]) fail(error, "Cannot read folder.");
    }
    return ok;
}
void irb_files_get_page(const IrbFileList* files, uint32_t start, IrbFilePage* page) {
    memset(page, 0, sizeof(*page));
    snprintf(page->path, sizeof(page->path), "%s", files->path);
    page->projects = files->projects;
    page->total = files->count;
    page->start = start < files->count ? start : 0;
    for(unsigned i = page->start; i < files->count && page->count < IRB_PAGE_SIZE; ++i) {
        snprintf(page->names[page->count], IRB_PATH_SIZE, "%s", files->entries[i].name);
        page->directories[page->count++] = files->entries[i].directory;
    }
}
bool irb_files_page(Storage* storage, IrbFilePage* page, char* error, IrbLoadProgress progress,
                    void* context) {
    IrbFileList files = {0};
    bool ok = irb_files_load(storage, &files, page->path, page->projects, error, progress, context);
    if(ok) irb_files_get_page(&files, page->start, page);
    irb_files_clear(&files);
    return ok;
}

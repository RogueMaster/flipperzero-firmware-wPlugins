#include "irb_storage.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IRB_MAX_LIBRARY_BYTES (8U * 1024U * 1024U)
#define IRB_MAX_PROJECT_BYTES (16U * 1024U)
#define IRB_MAX_TEXT_LINE_BYTES 512U
#define IRB_MAX_DATA_LINE_BYTES (12U * 1024U)
#define IRB_MAX_SIGNALS 8192U

static bool irb_fail(char* error, const char* message) {
    snprintf(error, IRB_ERROR_SIZE, "%s", message);
    return false;
}

static bool file_exists(Storage* storage, const char* path) {
    return storage_common_stat(storage, path, NULL) == FSE_OK;
}
static bool target_is_directory(Storage* storage, const char* path) {
    FileInfo info;
    return storage_common_stat(storage, path, &info) == FSE_OK && (info.flags & FSF_DIRECTORY);
}
static void clear_staging(Storage* storage) {
    storage_common_remove(storage, IRB_DATA_DIR "/export.new");
    storage_common_remove(storage, IRB_DATA_DIR "/project.new");
}

bool irb_path_equal(const char* a, const char* b) {
    while(*a && *b) {
        char ca = *a++, cb = *b++;
        if(ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if(cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if(ca != cb) return false;
    }
    return *a == *b;
}

static bool report_progress(IrbLoadProgress progress, void* context, IrbLoadPhase phase,
                            uint32_t done, uint32_t total) {
    return !progress || progress(phase, done, total, context);
}

static bool file_fingerprint(Storage* storage, const char* path, uint32_t* hash, uint32_t* size,
                             uint32_t max_bytes, IrbLoadPhase phase, IrbLoadProgress progress,
                             void* context) {
    if(!report_progress(progress, context, phase, 0, 0)) return false;
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    *hash = UINT32_C(2166136261);
    *size = 0;
    if(ok) {
        uint64_t expected = storage_file_size(file);
        ok = expected > 0 && expected <= max_bytes;
        uint8_t buffer[512];
        uint32_t line_length = 0;
        char prefix[5];
        size_t prefix_length = 0;
        while(ok && *size < expected) {
            size_t remaining = expected - *size;
            size_t length = storage_file_read(
                file, buffer, remaining < sizeof(buffer) ? remaining : sizeof(buffer));
            if(!length) break;
            // Bound string allocations before handing text to FlipperFormat.
            // RAW data may contain 1024 uint32 timings and gets a larger limit.
            for(size_t i = 0; ok && i < length; ++i) {
                unsigned char c = buffer[i];
                if(c == '\n') {
                    line_length = 0;
                    prefix_length = 0;
                    continue;
                }
                if(prefix_length < sizeof(prefix) && (prefix_length || (c != ' ' && c != '\t')))
                    prefix[prefix_length++] = c;
                uint32_t limit = prefix_length == sizeof(prefix) && !memcmp(prefix, "data:", 5)
                                     ? IRB_MAX_DATA_LINE_BYTES
                                     : IRB_MAX_TEXT_LINE_BYTES;
                ok = c != 0 && ++line_length <= limit;
            }
            *hash = irb_hash_update(*hash, buffer, length);
            *size += length;
            ok = ok && report_progress(progress, context, phase, *size, expected);
        }
        ok = ok && *size == expected && storage_file_size(file) == expected &&
             storage_file_get_error(file) == FSE_OK;
    }
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

void irb_library_clear(IrbLibrary* library) {
    for(size_t i = 0; i < IRB_GROUPS; ++i)
        free(library->offsets[i]);
    memset(library, 0, sizeof(*library));
}

bool irb_file_fingerprint(Storage* storage, const char* path, uint32_t* hash, uint32_t* size,
                          IrbLoadPhase phase, IrbLoadProgress progress, void* context) {
    return file_fingerprint(storage, path, hash, size, IRB_MAX_LIBRARY_BYTES, phase, progress,
                            context);
}

bool irb_library_open(Storage* storage, IrbLibrary* library, const char* path, char* error) {
    return irb_library_open_progress(storage, library, path, error, NULL, NULL);
}

bool irb_library_open_progress(Storage* storage, IrbLibrary* library, const char* path, char* error,
                               IrbLoadProgress progress, void* context) {
    return irb_library_open_cached(storage, library, library, path, error, progress, context);
}

bool irb_library_open_cached(Storage* storage, IrbLibrary* library, const IrbLibrary* cache,
                             const char* path, char* error, IrbLoadProgress progress,
                             void* context) {
    IrbLibrary next = {0};
    if(!irb_library_path_valid(path))
        return irb_fail(error, "Choose a TV library\non the SD card.");
    snprintf(next.path, sizeof(next.path), "%s", path);
    if(!file_fingerprint(storage, path, &next.hash, &next.size, IRB_MAX_LIBRARY_BYTES, IrbLoadHash,
                         progress, context))
        return irb_fail(error,
                        "Cannot read TV library.\nFile missing, too large,\nor invalid text data.");
    if(cache && cache->storage && irb_path_equal(path, cache->path) && next.hash == cache->hash &&
       next.size == cache->size) {
        if(!report_progress(progress, context, IrbLoadCache, next.size, next.size)) return false;
        for(unsigned i = 0; i < IRB_GROUPS; ++i) {
            if(!cache->counts[i]) continue;
            next.offsets[i] = malloc(cache->counts[i] * sizeof(uint32_t));
            if(!next.offsets[i]) {
                irb_library_clear(&next);
                return irb_fail(error, "Not enough memory.");
            }
            memcpy(next.offsets[i], cache->offsets[i], cache->counts[i] * sizeof(uint32_t));
            next.counts[i] = next.capacity[i] = cache->counts[i];
        }
        next.storage = storage;
        next.cache_hit = true;
        irb_library_clear(library);
        *library = next;
        return true;
    }
    FlipperFormat* reader = flipper_format_buffered_file_alloc(storage);
    FuriString* name = furi_string_alloc();
    InfraredSignal* signal = infrared_signal_alloc();
    bool ok = false;
    uint32_t version = 0, total = 0;
    do {
        if(!report_progress(progress, context, IrbLoadIndex, 0, next.size)) break;
        if(!flipper_format_buffered_file_open_existing(reader, path)) break;
        flipper_format_set_strict_mode(reader, true);
        if(!flipper_format_read_header(reader, name, &version) || version != 1 ||
           !(furi_string_equal(name, "IR library file") ||
             furi_string_equal(name, "IR signals file"))) {
            irb_fail(error, "Not a supported IR\nlibrary (version 1).");
            break;
        }
        bool valid = true;
        while(true) {
            if(!report_progress(progress, context, IrbLoadIndex, flipper_format_tell(reader),
                                next.size)) {
                valid = false;
                break;
            }
            if(infrared_signal_read_name(reader, name) != InfraredErrorCodeNone) {
                if(flipper_format_tell(reader) + 1 < next.size) valid = false;
                break;
            }
            size_t offset = flipper_format_tell(reader);
            if(offset > INT32_MAX ||
               infrared_signal_read_body(signal, reader) != InfraredErrorCodeNone ||
               !infrared_signal_is_valid(signal) || flipper_format_tell(reader) <= offset) {
                valid = false;
                break;
            }
            int group = irb_group_from_name(furi_string_get_cstr(name));
            if(group < 0) continue;
            if(++total > IRB_MAX_SIGNALS) {
                valid = false;
                break;
            }
            if(next.counts[group] == next.capacity[group]) {
                uint32_t capacity = next.capacity[group] + 32;
                uint32_t* offsets = realloc(next.offsets[group], capacity * sizeof(uint32_t));
                if(!offsets) {
                    valid = false;
                    break;
                }
                next.offsets[group] = offsets;
                next.capacity[group] = capacity;
            }
            next.offsets[group][next.counts[group]++] = offset;
        }
        if(!valid || !total) {
            irb_fail(error, "Invalid or oversized\nTV library. No entries\nwere silently skipped.");
            break;
        }
        // Close the scanner BEFORE opening the checksum reader. Reopening an
        // already-open path blocks forever in Flipper's storage_file_open.
        flipper_format_free(reader);
        reader = NULL;
        uint32_t hash, size;
        if(!file_fingerprint(storage, path, &hash, &size, IRB_MAX_LIBRARY_BYTES, IrbLoadVerify,
                             progress, context) ||
           hash != next.hash || size != next.size) {
            irb_fail(error, "Library changed while\nreading. Try again.");
            break;
        }
        next.storage = storage;
        ok = true;
    } while(false);
    furi_string_free(name);
    infrared_signal_free(signal);
    if(reader) flipper_format_free(reader);
    if(ok) {
        irb_library_clear(library);
        *library = next;
    } else {
        irb_library_clear(&next);
        if(!error[0]) irb_fail(error, "Cannot open the library.");
    }
    return ok;
}

bool irb_library_unchanged(Storage* storage, const IrbLibrary* library, char* error) {
    uint32_t hash, size;
    if(!library->storage || !file_fingerprint(storage, library->path, &hash, &size,
                                              IRB_MAX_LIBRARY_BYTES, IrbLoadVerify, NULL, NULL))
        return irb_fail(error,
                        "Library is unavailable.\nRestore the same file\nand reopen the draft.");
    if(hash != library->hash || size != library->size)
        return irb_fail(error, "tv.ir has changed.\nRestore the original\nlibrary, or start a "
                               "new\nremote and recheck codes.");
    return true;
}

bool irb_library_read(IrbLibrary* library, uint32_t group, uint32_t position,
                      InfraredSignal* signal) {
    IrbLibraryReader reader = {0};
    bool ok = irb_library_reader_open(&reader, library) &&
              irb_library_reader_read(&reader, group, position, signal);
    irb_library_reader_close(&reader);
    return ok;
}

bool irb_library_reader_open(IrbLibraryReader* reader, const IrbLibrary* library) {
    if(reader->file) return reader->library == library;
    if(!library->storage) return false;
    reader->file = flipper_format_buffered_file_alloc(library->storage);
    reader->library = library;
    flipper_format_set_strict_mode(reader->file, true);
    if(flipper_format_buffered_file_open_existing(reader->file, library->path)) return true;
    irb_library_reader_close(reader);
    return false;
}

void irb_library_reader_close(IrbLibraryReader* reader) {
    if(reader->file) flipper_format_free(reader->file);
    memset(reader, 0, sizeof(*reader));
}

bool irb_library_reader_read(IrbLibraryReader* reader, uint32_t group, uint32_t position,
                             InfraredSignal* signal) {
    const IrbLibrary* library = reader->library;
    return reader->file && library && group < IRB_GROUPS && position &&
           position <= library->counts[group] &&
           flipper_format_seek(reader->file, library->offsets[group][position - 1],
                               FlipperFormatOffsetFromStart) &&
           infrared_signal_read_body(signal, reader->file) == InfraredErrorCodeNone &&
           infrared_signal_is_valid(signal);
}

bool irb_builder_library_install(Storage* storage, char* error) {
    if(!irb_storage_prepare(storage))
        return irb_fail(error, "Cannot prepare app storage for the bundled TV library.");
    if(file_exists(storage, IRB_DEFAULT_LIBRARY)) return true;
    const char* temporary = IRB_DATA_DIR "/tv_builder.new";
    storage_common_remove(storage, temporary);
    if(storage_common_copy(storage, IRB_BUNDLED_LIBRARY, temporary) != FSE_OK ||
       storage_common_rename(storage, temporary, IRB_DEFAULT_LIBRARY) != FSE_OK) {
        storage_common_remove(storage, temporary);
        return irb_fail(error, "Cannot install bundled tv_builder.ir. Check SD-card space.");
    }
    return true;
}

bool irb_project_matches(const IrbProject* project, const IrbLibrary* library) {
    if(!irb_project_valid(project) || project->source_hash != library->hash ||
       project->source_size != library->size || !irb_path_equal(project->library, library->path))
        return false;
    for(size_t i = 0; i < IRB_SLOTS; ++i)
        if(project->positions[i] > library->counts[irb_slot_group[i]]) return false;
    for(size_t i = 0; i < IRB_NAV_KEYS; ++i)
        if(project->nav_positions[i] > library->counts[irb_nav_group[i]]) return false;
    return true;
}

bool irb_storage_prepare(Storage* storage) {
    const char* paths[] = {"/ext/apps_data", IRB_DATA_DIR,    IRB_PROJECT_DIR,
                           IRB_IMPORT_DIR,   "/ext/infrared", IRB_EXPORT_DIR};
    for(size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        FS_Error result = storage_common_mkdir(storage, paths[i]);
        if(result != FSE_OK && result != FSE_EXIST) return false;
    }
    return true;
}

void irb_recover_file(Storage* storage, const char* path) {
    char backup[IRB_PATH_SIZE];
    snprintf(backup, sizeof(backup), "%s.bak", path);
    if(!file_exists(storage, path) && file_exists(storage, backup))
        storage_common_rename(storage, backup, path);
}

static bool read_string(FlipperFormat* ff, const char* key, char* output, size_t capacity) {
    FuriString* value = furi_string_alloc();
    bool ok = flipper_format_read_string(ff, key, value) && furi_string_size(value) < capacity;
    if(ok) snprintf(output, capacity, "%s", furi_string_get_cstr(value));
    furi_string_free(value);
    return ok;
}

static bool read_project(Storage* storage, const char* path, IrbProject* project) {
    uint32_t hash, size;
    if(!file_fingerprint(storage, path, &hash, &size, IRB_MAX_PROJECT_BYTES, IrbLoadHash, NULL,
                         NULL))
        return false;
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* type = furi_string_alloc();
    IrbProject* next = calloc(1, sizeof(*next));
    irb_project_init(next);
    uint32_t version = 0;
    bool ok = false;
    do {
        if(!flipper_format_file_open_existing(ff, path)) break;
        flipper_format_set_strict_mode(ff, true);
        if(!flipper_format_read_header(ff, type, &version) ||
           (version != 1 && version != 2 && version != 3 && version != 4) ||
           !furi_string_equal(type, "IR Builder project"))
            break;
        if(!read_string(ff, "Library", next->library, sizeof(next->library)) ||
           !read_string(ff, "Name", next->name, sizeof(next->name)) ||
           !flipper_format_read_uint32(ff, "SourceHash", &next->source_hash, 1) ||
           !flipper_format_read_uint32(ff, "SourceSize", &next->source_size, 1) ||
           !flipper_format_read_uint32(ff, "Positions", next->positions, IRB_SLOTS))
            break;
        if(version >= 3 &&
           !flipper_format_read_uint32(ff, "NavPositions", next->nav_positions, IRB_NAV_KEYS))
            break;
        if(version >= 4) {
            uint32_t sources[IRB_POSITION_SLOTS], offsets[IRB_POSITION_SLOTS];
            if(!flipper_format_read_uint32(ff, "MappedSources", sources, IRB_POSITION_SLOTS) ||
               !flipper_format_read_uint32(ff, "MappedOffsets", offsets, IRB_POSITION_SLOTS))
                break;
            for(unsigned i = 0; i < IRB_POSITION_SLOTS; ++i)
                next->mapped[i] = (IrbSignalRef){.source = sources[i], .offset = offsets[i]};
        }
        if(version >= 4) {
            if(!flipper_format_read_uint32(ff, "Order", next->order, IRB_MAX_BUTTONS)) break;
        } else {
            enum { IrbLegacyButtons = 32 };
            uint32_t legacy[IrbLegacyButtons];
            unsigned count = version == 1 ? IRB_SLOTS : IrbLegacyButtons;
            if(!flipper_format_read_uint32(ff, "Order", legacy, count)) break;
            memcpy(next->order, legacy, count * sizeof(uint32_t));
            for(unsigned i = count; i < IRB_MAX_BUTTONS; ++i)
                next->order[i] = i;
        }
        bool labels_ok = true;
        for(size_t i = 0; i < IRB_SLOTS; ++i) {
            char key[12];
            snprintf(key, sizeof(key), "Label%u", (unsigned)i);
            if(!read_string(ff, key, next->labels[i], IRB_NAME_SIZE)) labels_ok = false;
        }
        if(version >= 2) {
            if(!flipper_format_read_uint32(ff, "ExtraCount", &next->extra_count, 1) ||
               !flipper_format_read_uint32(ff, "ImportCount", &next->import_count, 1) ||
               next->extra_count > IRB_MAX_EXTRAS || next->import_count > IRB_MAX_IMPORTS)
                break;
            for(unsigned i = 0; i < next->import_count; ++i) {
                char key[24];
                snprintf(key, sizeof(key), "Import%uPath", i);
                labels_ok &= read_string(ff, key, next->imports[i].path, IRB_PATH_SIZE);
                snprintf(key, sizeof(key), "Import%uHash", i);
                labels_ok &= flipper_format_read_uint32(ff, key, &next->imports[i].hash, 1);
                snprintf(key, sizeof(key), "Import%uSize", i);
                labels_ok &= flipper_format_read_uint32(ff, key, &next->imports[i].size, 1);
            }
            for(unsigned i = 0; i < next->extra_count; ++i) {
                char key[24];
                snprintf(key, sizeof(key), "Extra%uLabel", i);
                labels_ok &= read_string(ff, key, next->extras[i].label, IRB_NAME_SIZE);
                snprintf(key, sizeof(key), "Extra%uSource", i);
                labels_ok &= flipper_format_read_uint32(ff, key, &next->extras[i].source, 1);
                snprintf(key, sizeof(key), "Extra%uOffset", i);
                labels_ok &= flipper_format_read_uint32(ff, key, &next->extras[i].offset, 1);
            }
        }
        // Versions 2 and 3 represented navigation controls as generic imported
        // buttons. Promote recognized aliases into the designed navigation page.
        if(labels_ok && version < 4) {
            for(unsigned key = 0; key < IRB_NAV_KEYS; ++key) {
                int slot = irb_nav_slot(next, key);
                if(slot < IRB_SLOTS || irb_slot_is_nav((uint32_t)slot)) continue;
                IrbExtraButton extra = next->extras[slot - IRB_SLOTS];
                if(irb_project_set_imported(next, IRB_NAV_SLOT_BASE + key, extra.source,
                                            extra.offset))
                    irb_project_remove(next, slot);
            }
        }
        if(!labels_ok || !irb_project_valid(next)) break;
        *project = *next;
        ok = true;
    } while(false);
    flipper_format_free(ff);
    furi_string_free(type);
    free(next);
    return ok;
}

bool irb_project_load(Storage* storage, const char* path, IrbProject* project, char* error) {
    if(read_project(storage, path, project)) return true;
    char backup[IRB_PATH_SIZE];
    if(snprintf(backup, sizeof(backup), "%s.bak", path) >= (int)sizeof(backup))
        return irb_fail(error, "Project path is too long.");
    IrbProject* recovered = calloc(1, sizeof(*recovered));
    bool ok = read_project(storage, backup, recovered) &&
              storage_common_rename(storage, backup, path) == FSE_OK;
    if(ok) *project = *recovered;
    free(recovered);
    return ok ||
           irb_fail(error,
                    "Cannot read this project.\nThe previous file, if\nany, is kept as .bak.");
}

static bool write_project(Storage* storage, const char* path, const IrbProject* project) {
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    uint32_t sources[IRB_POSITION_SLOTS], offsets[IRB_POSITION_SLOTS];
    for(unsigned i = 0; i < IRB_POSITION_SLOTS; ++i) {
        sources[i] = project->mapped[i].source;
        offsets[i] = project->mapped[i].offset;
    }
    bool ok =
        flipper_format_file_open_always(ff, path) &&
        flipper_format_write_header_cstr(ff, "IR Builder project", 4) &&
        flipper_format_write_string_cstr(ff, "Library", project->library) &&
        flipper_format_write_string_cstr(ff, "Name", project->name) &&
        flipper_format_write_uint32(ff, "SourceHash", &project->source_hash, 1) &&
        flipper_format_write_uint32(ff, "SourceSize", &project->source_size, 1) &&
        flipper_format_write_uint32(ff, "Positions", project->positions, IRB_SLOTS) &&
        flipper_format_write_uint32(ff, "NavPositions", project->nav_positions, IRB_NAV_KEYS) &&
        flipper_format_write_uint32(ff, "MappedSources", sources, IRB_POSITION_SLOTS) &&
        flipper_format_write_uint32(ff, "MappedOffsets", offsets, IRB_POSITION_SLOTS) &&
        flipper_format_write_uint32(ff, "Order", project->order, IRB_MAX_BUTTONS);
    for(size_t i = 0; ok && i < IRB_SLOTS; ++i) {
        char key[12];
        snprintf(key, sizeof(key), "Label%u", (unsigned)i);
        ok = flipper_format_write_string_cstr(ff, key, project->labels[i]);
    }
    ok = ok && flipper_format_write_uint32(ff, "ExtraCount", &project->extra_count, 1) &&
         flipper_format_write_uint32(ff, "ImportCount", &project->import_count, 1);
    for(unsigned i = 0; ok && i < project->import_count; ++i) {
        char key[24];
        snprintf(key, sizeof(key), "Import%uPath", i);
        ok = flipper_format_write_string_cstr(ff, key, project->imports[i].path);
        snprintf(key, sizeof(key), "Import%uHash", i);
        ok = ok && flipper_format_write_uint32(ff, key, &project->imports[i].hash, 1);
        snprintf(key, sizeof(key), "Import%uSize", i);
        ok = ok && flipper_format_write_uint32(ff, key, &project->imports[i].size, 1);
    }
    for(unsigned i = 0; ok && i < project->extra_count; ++i) {
        char key[24];
        snprintf(key, sizeof(key), "Extra%uLabel", i);
        ok = flipper_format_write_string_cstr(ff, key, project->extras[i].label);
        snprintf(key, sizeof(key), "Extra%uSource", i);
        ok = ok && flipper_format_write_uint32(ff, key, &project->extras[i].source, 1);
        snprintf(key, sizeof(key), "Extra%uOffset", i);
        ok = ok && flipper_format_write_uint32(ff, key, &project->extras[i].offset, 1);
    }
    bool closed = flipper_format_file_close(ff);
    flipper_format_free(ff);
    return ok && closed;
}

static bool promote(Storage* storage, const char* temporary, const char* target, bool* had_old) {
    char backup[IRB_PATH_SIZE];
    snprintf(backup, sizeof(backup), "%s.bak", target);
    irb_recover_file(storage, target);
    *had_old = file_exists(storage, target);
    if(*had_old) {
        FS_Error removed = storage_common_remove(storage, backup);
        if(removed != FSE_OK && removed != FSE_NOT_EXIST) return false;
        if(storage_common_rename(storage, target, backup) != FSE_OK) return false;
    }
    if(storage_common_rename(storage, temporary, target) == FSE_OK) return true;
    if(*had_old)
        storage_common_rename(storage, backup, target);
    else
        storage_common_remove(storage, target); // A failed copy may have created a partial file.
    return false;
}

static void discard_backup(Storage* storage, const char* path) {
    char backup[IRB_PATH_SIZE];
    snprintf(backup, sizeof(backup), "%s.bak", path);
    storage_common_remove(storage, backup);
}

static bool read_settings(Storage* storage, const char* path, char* library,
                          uint32_t* saved_version) {
    uint32_t hash, size, version;
    if(!file_fingerprint(storage, path, &hash, &size, 1024, IrbLoadHash, NULL, NULL)) return false;
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* type = furi_string_alloc();
    char next[IRB_PATH_SIZE];
    flipper_format_set_strict_mode(ff, true);
    bool ok = flipper_format_file_open_existing(ff, path) &&
              flipper_format_read_header(ff, type, &version) && (version == 1 || version == 2) &&
              furi_string_equal(type, "IR Builder settings") &&
              read_string(ff, "Library", next, sizeof(next)) && irb_library_path_valid(next);
    if(ok) {
        snprintf(library, IRB_PATH_SIZE, "%s", next);
        if(saved_version) *saved_version = version;
    }
    furi_string_free(type);
    flipper_format_free(ff);
    return ok;
}

void irb_settings_load(Storage* storage, char* library) {
    snprintf(library, IRB_PATH_SIZE, "%s", IRB_DEFAULT_LIBRARY);
    uint32_t version = 0;
    bool loaded = read_settings(storage, IRB_SETTINGS_PATH, library, &version);
    // Recover a previous setting after an interrupted promotion.
    if(!loaded && read_settings(storage, IRB_SETTINGS_PATH ".bak", library, &version)) {
        storage_common_rename(storage, IRB_SETTINGS_PATH ".bak", IRB_SETTINGS_PATH);
        loaded = true;
    }
    // Version 1 used the stock tv.ir as its implicit default. Move that old default to
    // the bundled builder library once, while keeping every explicit version-2 choice.
    if(loaded && version == 1 && irb_path_equal(library, IRB_LEGACY_DEFAULT_LIBRARY)) {
        snprintf(library, IRB_PATH_SIZE, "%s", IRB_DEFAULT_LIBRARY);
        char ignored[IRB_ERROR_SIZE];
        irb_settings_save(storage, library, ignored);
    }
}

bool irb_settings_save(Storage* storage, const char* library, char* error) {
    if(!irb_library_path_valid(library) || !irb_storage_prepare(storage))
        return irb_fail(error, "Setting not saved. Check the SD card and library path.");
    const char* temporary = IRB_SETTINGS_PATH ".new";
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = flipper_format_file_open_always(ff, temporary) &&
              flipper_format_write_header_cstr(ff, "IR Builder settings", 2) &&
              flipper_format_write_string_cstr(ff, "Library", library);
    bool closed = flipper_format_file_close(ff);
    flipper_format_free(ff);
    char verified[IRB_PATH_SIZE];
    bool had_old;
    ok = ok && closed && read_settings(storage, temporary, verified, NULL) &&
         !strcmp(library, verified) && promote(storage, temporary, IRB_SETTINGS_PATH, &had_old);
    if(ok) discard_backup(storage, IRB_SETTINGS_PATH);
    storage_common_remove(storage, temporary);
    return ok || irb_fail(error, "Setting not saved. Previous library kept. Check the SD card.");
}

bool irb_draft_save(Storage* storage, const IrbProject* project, char* error) {
    if(!irb_project_valid(project) || !irb_storage_prepare(storage))
        return irb_fail(error, "Draft not saved.\nCheck the SD card.");
    const char* temporary = IRB_DRAFT_PATH ".new";
    bool had_old;
    if(!write_project(storage, temporary, project) ||
       !promote(storage, temporary, IRB_DRAFT_PATH, &had_old))
        return irb_fail(error, "Draft write failed.\nKeep the app open and\ncheck the SD card.");
    discard_backup(storage, IRB_DRAFT_PATH);
    return true;
}

void irb_remote_path(char* buffer, size_t size, const char* name, bool metadata) {
    snprintf(buffer, size, "%s/%s.%s", metadata ? IRB_PROJECT_DIR : IRB_EXPORT_DIR, name,
             metadata ? "irb" : "ir");
}

bool irb_remote_exists(Storage* storage, const char* name) {
    char path[IRB_PATH_SIZE], backup[IRB_PATH_SIZE + 4];
    for(unsigned metadata = 0; metadata < 2; ++metadata) {
        irb_remote_path(path, sizeof(path), name, metadata);
        snprintf(backup, sizeof(backup), "%s.bak", path);
        if(file_exists(storage, path) || file_exists(storage, backup)) return true;
    }
    return false;
}

bool irb_remote_save(Storage* storage, IrbLibrary* library, const IrbProject* project, bool replace,
                     char* error) {
    return irb_remote_save_progress(storage, library, project, replace, error, NULL, NULL);
}

bool irb_remote_save_progress(Storage* storage, IrbLibrary* library, const IrbProject* project,
                              bool replace, char* error, IrbLoadProgress progress, void* context) {
    if(!irb_project_matches(project, library) || !irb_project_count(project))
        return irb_fail(error, "Select at least one\nvalid button first.");
    if(!irb_storage_prepare(storage))
        return irb_fail(error, "Cannot save the remote.\nCheck the SD card.");
    if(!irb_project_check_sources(storage, project, IrbLoadHash, error, progress, context))
        return false;
    char remote[IRB_PATH_SIZE], metadata[IRB_PATH_SIZE];
    irb_remote_path(remote, sizeof(remote), project->name, false);
    irb_remote_path(metadata, sizeof(metadata), project->name, true);
    if(irb_project_uses_path(project, remote))
        return irb_fail(error, "Cannot overwrite the\nsource TV library.");
    if(!replace && irb_remote_exists(storage, project->name))
        return irb_fail(error, "That name already exists.\nChoose another name.");
    char backup[IRB_PATH_SIZE + 4];
    snprintf(backup, sizeof(backup), "%s.bak", remote);
    if(target_is_directory(storage, remote) || target_is_directory(storage, backup))
        return irb_fail(error, "A folder uses that name.\nChoose another name.");
    snprintf(backup, sizeof(backup), "%s.bak", metadata);
    if(target_is_directory(storage, metadata) || target_is_directory(storage, backup))
        return irb_fail(error, "A folder uses that name.\nChoose another name.");
    // Staging lives in the app directory, so no third-party .ir is truncated.
    const char* remote_temp = IRB_DATA_DIR "/export.new";
    const char* metadata_temp = IRB_DATA_DIR "/project.new";
    FlipperFormat* output = flipper_format_file_alloc(storage);
    InfraredSignal* signal = infrared_signal_alloc();
    bool ok =
        flipper_format_file_open_always(output, remote_temp) &&
        flipper_format_write_header_cstr(output, "IR signals file", 1) &&
        flipper_format_write_comment_cstr(
            output, "Created with IR Builder; positions are 1-based within each source button.");
    uint32_t written = 0, count = irb_project_count(project);
    for(size_t i = 0; ok && i < IRB_MAX_BUTTONS; ++i) {
        uint32_t slot = project->order[i];
        if(!irb_project_active(project, slot)) continue;
        ok = report_progress(progress, context, IrbLoadIndex, written, count) &&
             irb_project_read_signal(storage, library, project, slot, signal) &&
             infrared_signal_save(signal, output, irb_project_label(project, slot)) ==
                 InfraredErrorCodeNone;
        ++written;
    }
    for(size_t key = 0; ok && key < IRB_NAV_KEYS; ++key) {
        uint32_t slot = IRB_NAV_SLOT_BASE + key;
        if(!irb_project_active(project, slot)) continue;
        ok = report_progress(progress, context, IrbLoadIndex, written, count) &&
             irb_project_read_signal(storage, library, project, slot, signal) &&
             infrared_signal_save(signal, output, irb_project_label(project, slot)) ==
                 InfraredErrorCodeNone;
        ++written;
    }
    bool closed = flipper_format_file_close(output);
    flipper_format_free(output);
    infrared_signal_free(signal);
    if(!ok || !closed || !write_project(storage, metadata_temp, project)) {
        clear_staging(storage);
        return irb_fail(
            error, "Save failed. Existing\nremotes were not changed.\nCheck free SD-card space.");
    }
    if(!irb_project_check_sources(storage, project, IrbLoadVerify, error, progress, context)) {
        clear_staging(storage);
        return false;
    }
    if(!report_progress(progress, context, IrbLoadCommit, 0, 1)) {
        clear_staging(storage);
        return irb_fail(error, "Save cancelled.");
    }
    bool old_remote = false, old_metadata = false;
    if(!promote(storage, remote_temp, remote, &old_remote))
        return irb_fail(
            error, "Cannot finish the save.\nThe old file is retained\nor recoverable as .bak.");
    if(!promote(storage, metadata_temp, metadata, &old_metadata)) {
        storage_common_remove(storage, remote);
        if(old_remote) irb_recover_file(storage, remote);
        return irb_fail(error,
                        "Project save failed.\nPrevious remote restored\nwhere storage permitted.");
    }
    discard_backup(storage, remote);
    discard_backup(storage, metadata);
    return true;
}

bool irb_project_uses_path(const IrbProject* project, const char* path) {
    if(irb_path_equal(project->library, path)) return true;
    for(unsigned i = 0; i < project->import_count; ++i)
        if(irb_path_equal(project->imports[i].path, path)) return true;
    return false;
}

bool irb_project_check_sources(Storage* storage, const IrbProject* project, IrbLoadPhase phase,
                               char* error, IrbLoadProgress progress, void* context) {
    for(unsigned i = 0; i <= project->import_count; ++i) {
        const char* path = i ? project->imports[i - 1].path : project->library;
        uint32_t expected_hash = i ? project->imports[i - 1].hash : project->source_hash;
        uint32_t expected_size = i ? project->imports[i - 1].size : project->source_size;
        uint32_t hash, size;
        if(!irb_file_fingerprint(storage, path, &hash, &size, phase, progress, context) ||
           hash != expected_hash || size != expected_size)
            return irb_fail(error, "Source unavailable\nor changed. Restore\nthe original file.");
    }
    return true;
}

bool irb_project_read_signal(Storage* storage, IrbLibrary* library, const IrbProject* project,
                             uint32_t slot, InfraredSignal* signal) {
    if(!irb_project_active(project, slot)) return false;
    const IrbSignalRef* mapped = irb_project_imported(project, slot);
    if(slot < IRB_SLOTS && !mapped)
        return irb_library_read(library, irb_slot_group[slot], project->positions[slot], signal);
    if(irb_slot_is_nav(slot) && !mapped) {
        unsigned key = irb_nav_key_from_slot(slot);
        return irb_library_read(library, irb_nav_group[key], project->nav_positions[key], signal);
    }
    uint32_t source, offset;
    if(mapped) {
        source = mapped->source;
        offset = mapped->offset;
    } else {
        const IrbExtraButton* extra = &project->extras[slot - IRB_SLOTS];
        source = extra->source;
        offset = extra->offset;
    }
    FlipperFormat* reader = flipper_format_buffered_file_alloc(storage);
    flipper_format_set_strict_mode(reader, true);
    bool ok = source < project->import_count &&
              flipper_format_buffered_file_open_existing(reader, project->imports[source].path) &&
              flipper_format_seek(reader, offset, FlipperFormatOffsetFromStart) &&
              infrared_signal_read_body(signal, reader) == InfraredErrorCodeNone &&
              infrared_signal_is_valid(signal);
    flipper_format_free(reader);
    return ok;
}

bool irb_project_delete(Storage* storage, const IrbProject* project, bool delete_remote,
                        char* error) {
    if(!irb_project_valid(project)) return irb_fail(error, "Invalid project.");
    char path[IRB_PATH_SIZE], backup[IRB_PATH_SIZE + 4];
    irb_remote_path(path, sizeof(path), project->name, false);
    if(delete_remote && irb_project_uses_path(project, path))
        return irb_fail(error, "This is a source file.\nDelete project only.");
    // Check every target before the first deletion. Never remove a directory
    // whose name happens to match the export or its recovery backup.
    for(unsigned metadata = delete_remote ? 0 : 1; metadata < 2; ++metadata) {
        irb_remote_path(path, sizeof(path), project->name, metadata);
        snprintf(backup, sizeof(backup), "%s.bak", path);
        FileInfo info;
        if((storage_common_stat(storage, path, &info) == FSE_OK && (info.flags & FSF_DIRECTORY)) ||
           (storage_common_stat(storage, backup, &info) == FSE_OK && (info.flags & FSF_DIRECTORY)))
            return irb_fail(error, "A folder uses that name.\nNo files deleted.");
    }
    for(unsigned metadata = delete_remote ? 0 : 1; metadata < 2; ++metadata) {
        irb_remote_path(path, sizeof(path), project->name, metadata);
        FS_Error result = storage_common_remove(storage, path);
        if(result != FSE_OK && result != FSE_NOT_EXIST)
            return irb_fail(error, "Delete incomplete.\nSome files remain.\nCheck the SD card.");
        snprintf(backup, sizeof(backup), "%s.bak", path);
        result = storage_common_remove(storage, backup);
        if(result != FSE_OK && result != FSE_NOT_EXIST)
            return irb_fail(error, "Backup not removed.\nCheck the SD card.");
    }
    return true;
}

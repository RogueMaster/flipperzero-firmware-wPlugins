#pragma once

#include "irb_model.h"
#include "vendor/infrared_signal.h"
#include <storage/storage.h>

#define IRB_DATA_DIR "/ext/apps_data/ir_builder"
#define IRB_PROJECT_DIR IRB_DATA_DIR "/remotes"
#define IRB_IMPORT_DIR IRB_DATA_DIR "/imports"
#define IRB_EXPORT_DIR "/ext/infrared/ir_builder"
#define IRB_DRAFT_PATH IRB_DATA_DIR "/draft.irb"
#define IRB_SETTINGS_PATH IRB_DATA_DIR "/settings.irb"
#define IRB_ERROR_SIZE 160

typedef struct {
    uint32_t* offsets[IRB_GROUPS];
    uint32_t counts[IRB_GROUPS];
    uint32_t capacity[IRB_GROUPS];
    uint32_t hash;
    uint32_t size;
    char path[IRB_PATH_SIZE];
    // Keep offsets, not an open file: Flipper permits only one handle per path.
    Storage* storage;
    bool cache_hit;
} IrbLibrary;

// A scan owns one reader. Close it before fingerprinting or reopening its source.
typedef struct {
    FlipperFormat* file;
    const IrbLibrary* library;
} IrbLibraryReader;

typedef enum { IrbLoadHash, IrbLoadIndex, IrbLoadVerify, IrbLoadCache, IrbLoadCommit } IrbLoadPhase;
// Return false to cancel. Called between bounded file reads and signal records.
typedef bool (*IrbLoadProgress)(IrbLoadPhase phase, uint32_t done, uint32_t total, void* context);

void irb_library_clear(IrbLibrary* library);
bool irb_library_open(Storage* storage, IrbLibrary* library, const char* path, char* error);
bool irb_library_open_progress(Storage* storage, IrbLibrary* library, const char* path, char* error,
                               IrbLoadProgress progress, void* context);
bool irb_library_open_cached(Storage* storage, IrbLibrary* library, const IrbLibrary* cache,
                             const char* path, char* error, IrbLoadProgress progress,
                             void* context);
bool irb_file_fingerprint(Storage* storage, const char* path, uint32_t* hash, uint32_t* size,
                          IrbLoadPhase phase, IrbLoadProgress progress, void* context);
bool irb_library_unchanged(Storage* storage, const IrbLibrary* library, char* error);
bool irb_library_read(IrbLibrary* library, uint32_t group, uint32_t position,
                      InfraredSignal* signal);
bool irb_library_reader_open(IrbLibraryReader* reader, const IrbLibrary* library);
void irb_library_reader_close(IrbLibraryReader* reader);
bool irb_library_reader_read(IrbLibraryReader* reader, uint32_t group, uint32_t position,
                             InfraredSignal* signal);
bool irb_builder_library_install(Storage* storage, char* error);
void irb_settings_load(Storage* storage, char* library);
bool irb_settings_save(Storage* storage, const char* library, char* error);
bool irb_project_matches(const IrbProject* project, const IrbLibrary* library);
bool irb_storage_prepare(Storage* storage);
bool irb_project_load(Storage* storage, const char* path, IrbProject* project, char* error);
bool irb_draft_save(Storage* storage, const IrbProject* project, char* error);
bool irb_remote_save(Storage* storage, IrbLibrary* library, const IrbProject* project, bool replace,
                     char* error);
bool irb_remote_save_progress(Storage* storage, IrbLibrary* library, const IrbProject* project,
                              bool replace, char* error, IrbLoadProgress progress, void* context);
bool irb_project_check_sources(Storage* storage, const IrbProject* project, IrbLoadPhase phase,
                               char* error, IrbLoadProgress progress, void* context);
bool irb_project_read_signal(Storage* storage, IrbLibrary* library, const IrbProject* project,
                             uint32_t slot, InfraredSignal* signal);
bool irb_project_uses_path(const IrbProject* project, const char* path);
bool irb_project_delete(Storage* storage, const IrbProject* project, bool delete_remote,
                        char* error);
void irb_remote_path(char* buffer, size_t size, const char* name, bool metadata);
bool irb_remote_exists(Storage* storage, const char* name);
void irb_recover_file(Storage* storage, const char* path);
bool irb_path_equal(const char* a, const char* b);

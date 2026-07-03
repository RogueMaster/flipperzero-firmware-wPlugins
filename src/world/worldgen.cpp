//
// World generator .fal plugin: a thin furi/storage wrapper around gen_core.h.
// Mapped into RAM by the host only while a world is being generated.
//
#include "gen_core.h"
#include "../plugin_api.h"

#include <furi.h>
#include <flipper_application/flipper_application.h>
#include <storage/storage.h>

static bool storageWriteAt(void* ctx, uint32_t offset, const void* data, size_t n) {
    File* file = reinterpret_cast<File*>(ctx);
    return storage_file_seek(file, offset, true) && storage_file_write(file, data, n) == n;
}

static bool flipcraft_worldgen_generate(
    const char* path,
    uint8_t chunks,
    uint32_t seed,
    FlipcraftGenProgress progress,
    void* progress_ctx) {
    Storage* storage = reinterpret_cast<Storage*>(furi_record_open(RECORD_STORAGE));
    File* file = storage_file_alloc(storage);

    bool ok = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) {
        fcgen::Writer out = {storageWriteAt, file};
        ok = fcgen::generate(chunks, seed, out, progress, progress_ctx);
        storage_file_close(file);
        if(!ok) storage_simply_remove(storage, path); // no truncated saves
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static const FlipcraftWorldgenApi flipcraft_worldgen_api = {
    .generate = flipcraft_worldgen_generate,
};

static const FlipperAppPluginDescriptor flipcraft_worldgen_descriptor = {
    .appid = FLIPCRAFT_WORLDGEN_APP_ID,
    .ep_api_version = FLIPCRAFT_WORLDGEN_API_VERSION,
    .entry_point = &flipcraft_worldgen_api,
};

extern "C" const FlipperAppPluginDescriptor* flipcraft_worldgen_ep(void) {
    return &flipcraft_worldgen_descriptor;
}

#include "secret_progress.h"
#include <stddef.h>
#include <string.h>

#define GTA_SECRET_DIRECTORY EXT_PATH("apps_data/gta_vi_cheats")
enum {
    GtaSecretMagic = 0x31534547u
}; /* GES1 */
static const char* const gta_secret_paths[GtaSecretProgressSlots] = {
    GTA_SECRET_DIRECTORY "/secrets.a",
    GTA_SECRET_DIRECTORY "/secrets.b",
};
_Static_assert(sizeof(GtaSecretSave) == 20, "Secret progress save layout changed");

static uint32_t gta_secret_checksum(const void* data, size_t len) {
    uint32_t result = UINT32_C(2166136261);
    const uint8_t* bytes = data;
    for(size_t i = 0; i < len; ++i) {
        result ^= bytes[i];
        result *= UINT32_C(16777619);
    }
    return result;
}
static bool gta_secret_read(Storage* storage, const char* path, GtaSecretSave* out) {
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    const bool good = storage_file_size(file) == sizeof(*out) &&
                      storage_file_read(file, out, sizeof(*out)) == sizeof(*out) &&
                      out->magic == GtaSecretMagic &&
                      out->checksum == gta_secret_checksum(out, offsetof(GtaSecretSave, checksum));
    const bool closed = storage_file_close(file);
    storage_file_free(file);
    return good && closed;
}
void gta_secret_progress_load(GtaSecretProgress* progress, Storage* storage) {
    memset(progress, 0, sizeof(*progress));
    progress->save.magic = GtaSecretMagic;
    progress->slot = GtaSecretProgressSlots;
    for(size_t slot = 0; slot < GtaSecretProgressSlots; ++slot) {
        GtaSecretSave candidate;
        if(!gta_secret_read(storage, gta_secret_paths[slot], &candidate)) continue;
        const uint32_t age = candidate.generation - progress->save.generation;
        if(progress->slot == GtaSecretProgressSlots || (age && age < UINT32_C(0x80000000))) {
            progress->save = candidate;
            progress->slot = slot;
        }
    }
}
bool gta_secret_progress_is_checked(const GtaSecretProgress* progress, uint8_t id) {
    if(!id || id >= GtaSecretProgressBytes * 8u) return false;
    return (progress->save.checked[id / 8u] & (1u << (id % 8u))) != 0;
}
bool gta_secret_progress_toggle(GtaSecretProgress* progress, Storage* storage, uint8_t id) {
    if(!id || id >= GtaSecretProgressBytes * 8u) return false;
    if(!storage_simply_mkdir(storage, EXT_PATH("apps_data")) ||
       !storage_simply_mkdir(storage, GTA_SECRET_DIRECTORY)) {
        progress->save_error = true;
        return false;
    }
    GtaSecretSave next = progress->save;
    next.checked[id / 8u] ^= (uint8_t)(1u << (id % 8u));
    next.generation++;
    next.checksum = gta_secret_checksum(&next, offsetof(GtaSecretSave, checksum));
    const size_t new_slot = progress->slot == 0 ? 1 : 0;
    const char* path = gta_secret_paths[new_slot];
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        progress->save_error = true;
        return false;
    }
    const bool written = storage_file_write(file, &next, sizeof(next)) == sizeof(next) &&
                         storage_file_sync(file);
    const bool closed = storage_file_close(file);
    storage_file_free(file);
    if(!written || !closed) {
        storage_simply_remove(storage, path);
        progress->save_error = true;
        return false;
    }
    progress->save = next;
    progress->slot = new_slot;
    progress->save_error = false;
    return true;
}

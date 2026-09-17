#include "irb_cache.h"
#include <stdio.h>
#include <string.h>

void irb_cache_clear(IrbSignalCache* cache) {
    for(unsigned i = 0; i < IRB_SIGNAL_CACHE_SIZE; ++i)
        if(cache->entries[i].signal) infrared_signal_free(cache->entries[i].signal);
    memset(cache, 0, sizeof(*cache));
}
static uint32_t stamp(IrbSignalCache* cache) {
    if(++cache->clock == 0) {
        for(unsigned i = 0; i < IRB_SIGNAL_CACHE_SIZE; ++i)
            cache->entries[i].used = 0;
        cache->clock = 1;
    }
    return cache->clock;
}
bool irb_signal_key(const IrbLibrary* library, const IrbProject* project, unsigned slot,
                    IrbSignalKey* key) {
    memset(key, 0, sizeof(*key));
    if(!irb_project_active(project, slot)) return false;
    const IrbSignalRef* mapped = irb_project_imported(project, slot);
    if(mapped) {
        if(mapped->source >= project->import_count) return false;
        const IrbImportSource* source = &project->imports[mapped->source];
        snprintf(key->path, sizeof(key->path), "%s", source->path);
        key->hash = source->hash;
        key->size = source->size;
        key->offset = mapped->offset;
    } else if(slot < IRB_SLOTS) {
        unsigned group = irb_slot_group[slot], position = project->positions[slot];
        if(!library->storage || !position || position > library->counts[group] ||
           library->hash != project->source_hash || library->size != project->source_size ||
           !irb_path_equal(library->path, project->library))
            return false;
        snprintf(key->path, sizeof(key->path), "%s", project->library);
        key->hash = project->source_hash;
        key->size = project->source_size;
        key->offset = library->offsets[group][position - 1];
    } else if(irb_slot_is_nav(slot)) {
        unsigned nav = irb_nav_key_from_slot(slot);
        unsigned group = irb_nav_group[nav], position = project->nav_positions[nav];
        if(!library->storage || !position || position > library->counts[group] ||
           library->hash != project->source_hash || library->size != project->source_size ||
           !irb_path_equal(library->path, project->library))
            return false;
        snprintf(key->path, sizeof(key->path), "%s", project->library);
        key->hash = project->source_hash;
        key->size = project->source_size;
        key->offset = library->offsets[group][position - 1];
    } else {
        const IrbExtraButton* extra = &project->extras[slot - IRB_SLOTS];
        if(extra->source >= project->import_count) return false;
        const IrbImportSource* source = &project->imports[extra->source];
        snprintf(key->path, sizeof(key->path), "%s", source->path);
        key->hash = source->hash;
        key->size = source->size;
        key->offset = extra->offset;
    }
    return key->offset < key->size;
}
bool irb_signal_source_check(Storage* storage, const IrbSignalKey* key, char* error,
                             IrbLoadProgress progress, void* context) {
    uint32_t hash, size;
    if(irb_file_fingerprint(storage, key->path, &hash, &size, IrbLoadVerify, progress, context) &&
       hash == key->hash && size == key->size)
        return true;
    snprintf(error, IRB_ERROR_SIZE, "Signal source changed or unavailable. Reopen the remote.");
    return false;
}
static bool same(const IrbSignalKey* a, const IrbSignalKey* b) {
    return a->hash == b->hash && a->size == b->size && a->offset == b->offset &&
           irb_path_equal(a->path, b->path);
}
const InfraredSignal* irb_cache_find(IrbSignalCache* cache, const IrbSignalKey* key) {
    for(unsigned i = 0; i < IRB_SIGNAL_CACHE_SIZE; ++i) {
        IrbCachedSignal* entry = &cache->entries[i];
        if(entry->signal && same(&entry->key, key)) {
            entry->used = stamp(cache);
            return entry->signal;
        }
    }
    return NULL;
}
const InfraredSignal* irb_cache_put(IrbSignalCache* cache, const IrbSignalKey* key,
                                    const InfraredSignal* signal) {
    IrbCachedSignal* entry = &cache->entries[0];
    for(unsigned i = 0; i < IRB_SIGNAL_CACHE_SIZE; ++i) {
        IrbCachedSignal* candidate = &cache->entries[i];
        if(!candidate->signal || same(&candidate->key, key)) {
            entry = candidate;
            break;
        }
        if(candidate->used < entry->used) entry = candidate;
    }
    if(!entry->signal) entry->signal = infrared_signal_alloc();
    if(entry->signal != signal) infrared_signal_set_signal(entry->signal, signal);
    entry->key = *key;
    entry->used = stamp(cache);
    return entry->signal;
}
const InfraredSignal* irb_cache_read(IrbSignalCache* cache, Storage* storage,
                                     const IrbSignalKey* key, char* error, IrbLoadProgress progress,
                                     void* context) {
    const InfraredSignal* found = irb_cache_find(cache, key);
    if(found) return found;
    if(!irb_signal_source_check(storage, key, error, progress, context)) return NULL;
    InfraredSignal* signal = infrared_signal_alloc();
    FlipperFormat* reader = flipper_format_buffered_file_alloc(storage);
    flipper_format_set_strict_mode(reader, true);
    bool ok = flipper_format_buffered_file_open_existing(reader, key->path) &&
              flipper_format_seek(reader, key->offset, FlipperFormatOffsetFromStart) &&
              infrared_signal_read_body(signal, reader) == InfraredErrorCodeNone &&
              infrared_signal_is_valid(signal);
    flipper_format_free(reader); // Exclusive file must close before verification.
    if(ok) ok = irb_signal_source_check(storage, key, error, progress, context);
    if(ok)
        found = irb_cache_put(cache, key, signal);
    else if(!error[0])
        snprintf(error, IRB_ERROR_SIZE, "Cannot read signal. Reopen the remote.");
    infrared_signal_free(signal);
    return found;
}

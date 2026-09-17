#pragma once
#include "irb_storage.h"

#define IRB_SIGNAL_CACHE_SIZE 4
typedef struct {
    char path[IRB_PATH_SIZE];
    uint32_t hash, size, offset;
} IrbSignalKey;
typedef struct {
    IrbSignalKey key;
    InfraredSignal* signal;
    uint32_t used;
} IrbCachedSignal;
typedef struct {
    IrbCachedSignal entries[IRB_SIGNAL_CACHE_SIZE];
    uint32_t clock;
} IrbSignalCache;

void irb_cache_clear(IrbSignalCache* cache);
bool irb_signal_key(const IrbLibrary* library, const IrbProject* project, unsigned slot,
                    IrbSignalKey* key);
bool irb_signal_source_check(Storage* storage, const IrbSignalKey* key, char* error,
                             IrbLoadProgress progress, void* context);
const InfraredSignal* irb_cache_find(IrbSignalCache* cache, const IrbSignalKey* key);
const InfraredSignal* irb_cache_put(IrbSignalCache* cache, const IrbSignalKey* key,
                                    const InfraredSignal* signal);
const InfraredSignal* irb_cache_read(IrbSignalCache* cache, Storage* storage,
                                     const IrbSignalKey* key, char* error, IrbLoadProgress progress,
                                     void* context);

#pragma once

#include <nfc/protocols/mf_classic/mf_classic.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MfClassicKeyCache MfClassicKeyCache;

MfClassicKeyCache* mf_classic_key_cache_alloc(void);

void mf_classic_key_cache_free(MfClassicKeyCache* instance);

void mf_classic_key_cache_load_from_data(MfClassicKeyCache* instance, const MfClassicData* data);

bool mf_classic_key_cache_save(MfClassicKeyCache* instance, const MfClassicData* data);

#ifdef __cplusplus
}
#endif

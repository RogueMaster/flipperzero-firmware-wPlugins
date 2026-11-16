#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dfc_crypto.h"
#include "dfc_crypto_internal.h"

// AN10922 pads its fixed-size CMAC input before invoking CMAC. The last block
// must therefore select K2 without adding another padding marker.
typedef enum {
    DfcCmacFinalStandard,
    DfcCmacFinalPrePadded,
} DfcCmacFinalMode;

typedef struct {
    DfcCryptoAesBlock cipher;
    uint8_t subkey1[16];
    uint8_t subkey2[16];
    bool initialized;
} DfcAesCmacContext;

typedef struct {
    DfcCryptoDesBlock cipher;
    uint8_t subkey1[8];
    uint8_t subkey2[8];
    bool initialized;
} DfcDesCmacContext;

bool dfc_aes_cmac_context_init(DfcAesCmacContext* context, const uint8_t* key, size_t key_len);
bool dfc_aes_cmac_context_compute(
    DfcAesCmacContext* context,
    const DfcCryptoSegment* segments,
    size_t segment_count,
    const uint8_t iv[16],
    DfcCmacFinalMode final_mode,
    uint8_t cmac[16]);
void dfc_aes_cmac_context_clear(DfcAesCmacContext* context);

bool dfc_des_cmac_context_init(DfcDesCmacContext* context, const uint8_t* key, size_t key_len);
bool dfc_des_cmac_context_compute(
    DfcDesCmacContext* context,
    const DfcCryptoSegment* segments,
    size_t segment_count,
    const uint8_t iv[8],
    DfcCmacFinalMode final_mode,
    uint8_t cmac[8]);
void dfc_des_cmac_context_clear(DfcDesCmacContext* context);

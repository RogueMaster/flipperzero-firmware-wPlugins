// SPDX-FileCopyrightText: 2026 Sven Würth
// SPDX-License-Identifier: MIT

#include "vf_device.h"

#include <furi_hal_crypto.h>
#include <string.h>

static const uint8_t vf_device_iv[16] =
    {'V', 'a', 'u', 'l', 't', 'F', 'l', 'i', 'p', 'p', 'e', 'r', '-', 'i', 'v', '1'};
static const uint8_t vf_device_label[32] = "VaultFlipper device secret v1...";

bool vf_device_secret(uint8_t out[32]) {
    const uint8_t slot = FURI_HAL_CRYPTO_ENCLAVE_UNIQUE_KEY_SLOT;
    if(!furi_hal_crypto_enclave_ensure_key(slot)) return false;
    if(!furi_hal_crypto_enclave_load_key(slot, vf_device_iv)) return false;
    bool ok = furi_hal_crypto_encrypt(vf_device_label, out, sizeof(vf_device_label));
    furi_hal_crypto_enclave_unload_key(slot);
    if(!ok) memset(out, 0, 32);
    return ok;
}

/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "esp_secure_boot.h"
#include "rom/ecdsa.h"
#include "secure_boot_signature_priv.h"
#include <tiny_crypto/ec.h>
#include <stdlib.h>

esp_err_t verify_ecdsa_signature_block(const ets_secure_boot_signature_t* signatures,
    const uint8_t* digest, const ets_secure_boot_sig_block_t* trusted)
{
  struct verification {
    TC_ECDSA_workspace workspace;
    uint8_t public_key[65];
    uint8_t signature[64];
  } *state;
  TC_EC_curve curve;
  TC_status result;
  size_t bytes, i;
  if (!signatures || !digest || !trusted) return ESP_ERR_INVALID_ARG;
  switch (trusted->ecdsa.key.curve_id) {
    case ECDSA_CURVE_P192: curve = TC_EC_P192; bytes = 24; break;
    case ECDSA_CURVE_P256: curve = TC_EC_P256; bytes = 32; break;
    default: return ESP_ERR_INVALID_ARG;
  }
  state = malloc(sizeof *state);
  if (!state) return ESP_ERR_NO_MEM;
  state->public_key[0] = 4;
  /* The image stores each coordinate and signature integer little-endian. */
  for (i = 0; i < bytes; ++i) {
    state->public_key[1 + i] = trusted->ecdsa.key.point[bytes - 1 - i];
    state->public_key[1 + bytes + i] = trusted->ecdsa.key.point[2 * bytes - 1 - i];
    state->signature[i] = trusted->ecdsa.signature[bytes - 1 - i];
    state->signature[bytes + i] = trusted->ecdsa.signature[2 * bytes - 1 - i];
  }
  result = TC_ECDSA_verify_digest(curve, state->public_key, 1 + 2 * bytes,
      digest, ESP_SECURE_BOOT_DIGEST_LEN, state->signature, 2 * bytes, &state->workspace);
  TC_secure_zero(state, sizeof *state);
  free(state);
  return result == TC_OK ? ESP_OK : ESP_ERR_IMAGE_INVALID;
}

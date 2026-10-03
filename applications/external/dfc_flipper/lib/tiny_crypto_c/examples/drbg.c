/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "drbg.h"
#include <string.h>

TC_DRBG_result example_random_start(ExampleRandom* random, TC_random_source entropy,
                                    TC_bytes device_id)
{
  const TC_bytes drawn_nonce = {NULL, 0};
  TC_DRBG_config config;
  if (random == NULL)
    return TC_DRBG_ARGUMENT;
  memset(&config, 0, sizeof config);
  config.mechanism = TC_DRBG_HMAC;
  config.hash = TC_HASH_SHA256;
  /* Instantiation wipes the context itself when it fails. */
  return TC_DRBG_instantiate(&random->drbg, &config, entropy, drawn_nonce, device_id);
}

TC_DRBG_result example_random_session_key(ExampleRandom* random, TC_bytes label, uint8_t key[32])
{
  TC_DRBG_result result;
  if (random == NULL || key == NULL)
    return TC_DRBG_ARGUMENT;
  result = TC_DRBG_generate(&random->drbg, (TC_buffer){key, 32}, 0, label);
  /* A primitive failure leaves the generator unusable, so stop it. Entropy
   * failures during an automatic reseed leave it usable for a later retry. */
  if (result == TC_DRBG_ERROR)
    example_random_stop(random);
  return result;
}

TC_DRBG_result example_random_refresh(ExampleRandom* random)
{
  const TC_bytes no_additional_input = {NULL, 0};
  if (random == NULL)
    return TC_DRBG_ARGUMENT;
  return TC_DRBG_reseed(&random->drbg, no_additional_input);
}

void example_random_stop(ExampleRandom* random)
{
  if (random != NULL)
    TC_DRBG_uninstantiate(&random->drbg);
}

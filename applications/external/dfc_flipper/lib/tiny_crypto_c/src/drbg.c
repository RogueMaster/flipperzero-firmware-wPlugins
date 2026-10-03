/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * SP 800-90A section 9 envelope: instantiate, reseed, generate and
 * uninstantiate for every mechanism. Argument checks, entropy acquisition,
 * reseed scheduling, prediction resistance and error handling live here
 * once. The mechanism files implement the section 10 algorithms. */
#include <tiny_crypto/drbg.h>

#if TC_ENABLE_DRBG
#include <string.h>
#include "drbg_internal.h"
#include "internal.h"

#define TC_DRBG_MARKER UINT32_C(0x44524247) /* "DRBG": instantiated */

static int live(const TC_DRBG* drbg)
{
  return drbg != NULL && drbg->marker == TC_DRBG_MARKER && !drbg->failed;
}

static int outside(const TC_DRBG* drbg, const void* data, size_t length)
{
  return tc_internal_ranges_disjoint(drbg, sizeof *drbg, data, length);
}

static TC_DRBG_result mechanism_parameters(const TC_DRBG_config* config, tc_drbg_parameters* out)
{
  switch (config->mechanism) {
#if TC_DRBG_HAVE_HASH
  case TC_DRBG_HASH:
    return tc_drbg_hash_parameters(config->hash, out);
#endif
#if TC_DRBG_HAVE_HMAC
  case TC_DRBG_HMAC:
    return tc_drbg_hmac_parameters(config->hash, out);
#endif
#if TC_DRBG_HAVE_CTR
  case TC_DRBG_CTR:
    return tc_drbg_ctr_parameters(config->aes_key_bytes, config->derivation_function, out);
#endif
    /* Known mechanisms compiled out of this build. */
#if !TC_DRBG_HAVE_HASH
  case TC_DRBG_HASH:
#endif
#if !TC_DRBG_HAVE_HMAC
  case TC_DRBG_HMAC:
#endif
#if !TC_DRBG_HAVE_CTR
  case TC_DRBG_CTR:
#endif
#if !TC_DRBG_HAVE_HASH || !TC_DRBG_HAVE_HMAC || !TC_DRBG_HAVE_CTR
    return TC_DRBG_UNSUPPORTED;
#endif
  default:
    return TC_DRBG_ARGUMENT;
  }
}

static TC_DRBG_result mechanism_seed(TC_DRBG* drbg, const TC_bytes* parts, size_t count, int reseed)
{
  switch (drbg->mechanism) {
#if TC_DRBG_HAVE_HASH
  case TC_DRBG_HASH:
    return tc_drbg_hash_seed(drbg, parts, count, reseed);
#endif
#if TC_DRBG_HAVE_HMAC
  case TC_DRBG_HMAC:
    return tc_drbg_hmac_seed(drbg, parts, count, reseed);
#endif
#if TC_DRBG_HAVE_CTR
  case TC_DRBG_CTR:
    return tc_drbg_ctr_seed(drbg, parts, count, reseed);
#endif
  default:
    return TC_DRBG_ERROR;
  }
}

static TC_DRBG_result mechanism_generate(TC_DRBG* drbg, uint8_t* output, size_t length,
                                         TC_bytes additional)
{
  switch (drbg->mechanism) {
#if TC_DRBG_HAVE_HASH
  case TC_DRBG_HASH:
    return tc_drbg_hash_generate(drbg, output, length, additional);
#endif
#if TC_DRBG_HAVE_HMAC
  case TC_DRBG_HMAC:
    return tc_drbg_hmac_generate(drbg, output, length, additional);
#endif
#if TC_DRBG_HAVE_CTR
  case TC_DRBG_CTR:
    return tc_drbg_ctr_generate(drbg, output, length, additional);
#endif
  default:
    return TC_DRBG_ERROR;
  }
}

/* Every input is at most TC_DRBG_MAX_INPUT_BYTES. */
static int input_within_limit(size_t length)
{
#if SIZE_MAX > TC_DRBG_MAX_INPUT_BYTES
  return length <= TC_DRBG_MAX_INPUT_BYTES;
#else
  /* size_t cannot exceed the limit, so length needs no comparison. */
  (void)length;
  return 1;
#endif
}

/* CTR_DRBG without a derivation function XORs inputs into the seed, so they
 * are at most seedlen. Other mechanisms hash their inputs. */
static int input_length_valid(const TC_DRBG* drbg, size_t length)
{
  return input_within_limit(length) && (drbg->mechanism != TC_DRBG_CTR ||
                                        drbg->derivation_function || length <= drbg->seed_bytes);
}

/* Read length bytes from the entropy source into drbg->input. */
static TC_DRBG_result read_entropy(TC_DRBG* drbg, size_t length)
{
  if (drbg->entropy.fill(drbg->entropy.context, drbg->input, length) == TC_OK)
    return TC_DRBG_OK;
  TC_secure_zero(drbg->input, sizeof drbg->input);
  return TC_DRBG_ENTROPY;
}

/* A primitive failure leaves the working state undefined, so the DRBG stays
 * unusable until it is uninstantiated. */
static TC_DRBG_result record(TC_DRBG* drbg, TC_DRBG_result result)
{
  if (result == TC_DRBG_ERROR)
    drbg->failed = 1;
  return result;
}

TC_DRBG_result TC_DRBG_instantiate(TC_DRBG* drbg, const TC_DRBG_config* config,
                                   TC_random_source entropy, TC_bytes nonce,
                                   TC_bytes personalization)
{
  tc_drbg_parameters parameters;
  TC_bytes parts[3];
  size_t minimum, entropy_bytes, nonce_bytes;
  TC_DRBG_result result;

  if (drbg == NULL)
    return TC_DRBG_ARGUMENT;
  TC_DRBG_uninstantiate(drbg);
  if (config == NULL || entropy.fill == NULL || !tc_internal_span_valid(nonce.data, nonce.length) ||
      !tc_internal_span_valid(personalization.data, personalization.length) ||
      !input_within_limit(nonce.length) || !input_within_limit(personalization.length) ||
      personalization.length > TC_DRBG_MAX_INPUT_BYTES - nonce.length ||
      config->prediction_resistance > 1 || config->derivation_function > 1 ||
      config->reseed_interval > TC_DRBG_MAX_RESEED_INTERVAL ||
      !outside(drbg, nonce.data, nonce.length) ||
      !outside(drbg, personalization.data, personalization.length) ||
      !outside(drbg, config, sizeof *config))
    return TC_DRBG_ARGUMENT;
  result = mechanism_parameters(config, &parameters);
  if (result != TC_DRBG_OK)
    return result;

  /* Section 8.6.3: entropy input carries the full security strength. Without
   * a derivation function it is the seed itself. */
  minimum = parameters.input_is_seed ? parameters.seed_bytes : parameters.strength_bits / 8u;
  entropy_bytes = config->entropy_bytes == 0 ? minimum : config->entropy_bytes;
  /* Section 8.6.7: the nonce has at least half the strength. An empty nonce
   * is drawn from the entropy source along with the entropy input. */
  nonce_bytes = parameters.uses_nonce && nonce.length == 0 ? parameters.strength_bits / 16u : 0;
  if (entropy_bytes < minimum || (parameters.input_is_seed && entropy_bytes != minimum) ||
      entropy_bytes > TC_DRBG_MAX_ENTROPY_BYTES - nonce_bytes ||
      (!parameters.uses_nonce && nonce.length != 0) ||
      (parameters.uses_nonce && nonce.length != 0 &&
       nonce.length < parameters.strength_bits / 16u) ||
      (parameters.input_is_seed && personalization.length > parameters.seed_bytes))
    return TC_DRBG_ARGUMENT;

  drbg->mechanism = (uint8_t)config->mechanism;
  drbg->hash = (uint8_t)config->hash;
  drbg->key_bytes = parameters.key_bytes;
  drbg->seed_bytes = parameters.seed_bytes;
  drbg->output_bytes = parameters.output_bytes;
  drbg->derivation_function = config->derivation_function;
  drbg->prediction_resistance = config->prediction_resistance;
  drbg->strength_bits = parameters.strength_bits;
  drbg->entropy_bytes = entropy_bytes;
  drbg->reseed_interval =
      config->reseed_interval == 0 ? TC_DRBG_MAX_RESEED_INTERVAL : config->reseed_interval;
  drbg->entropy = entropy;

  result = read_entropy(drbg, entropy_bytes + nonce_bytes);
  if (result == TC_DRBG_OK) {
    parts[0] = (TC_bytes){drbg->input, entropy_bytes};
    parts[1] = nonce_bytes != 0 ? (TC_bytes){drbg->input + entropy_bytes, nonce_bytes} : nonce;
    parts[2] = personalization;
    result = mechanism_seed(drbg, parts, 3, 0);
  }
  TC_secure_zero(drbg->input, sizeof drbg->input);
  if (result != TC_DRBG_OK) {
    TC_DRBG_uninstantiate(drbg);
    return result;
  }
  drbg->reseed_counter = 1;
  drbg->marker = TC_DRBG_MARKER;
  return TC_DRBG_OK;
}

/* Section 9.2 steps 4 to 7 for an instantiated DRBG and checked input. */
static TC_DRBG_result reseed(TC_DRBG* drbg, TC_bytes additional)
{
  TC_bytes parts[2];
  TC_DRBG_result result = read_entropy(drbg, drbg->entropy_bytes);
  if (result != TC_DRBG_OK)
    return result;
  parts[0] = (TC_bytes){drbg->input, drbg->entropy_bytes};
  parts[1] = additional;
  result = record(drbg, mechanism_seed(drbg, parts, 2, 1));
  TC_secure_zero(drbg->input, sizeof drbg->input);
  if (result == TC_DRBG_OK)
    drbg->reseed_counter = 1;
  return result;
}

TC_DRBG_result TC_DRBG_reseed(TC_DRBG* drbg, TC_bytes additional)
{
  if (!live(drbg) || !tc_internal_span_valid(additional.data, additional.length) ||
      !outside(drbg, additional.data, additional.length) ||
      !input_length_valid(drbg, additional.length))
    return TC_DRBG_ARGUMENT;
  return reseed(drbg, additional);
}

/* Argument checks and the request-size preflight run before any write, so
 * a rejected request leaves the output and the state unchanged. */
TC_DRBG_result TC_DRBG_generate(TC_DRBG* drbg, TC_buffer output, int prediction_resistance,
                                TC_bytes additional)
{
  TC_DRBG_result result;
  const size_t length = output.capacity;

  if ((output.data == NULL && length != 0) || !live(drbg) ||
      !tc_internal_span_valid(additional.data, additional.length) ||
      !outside(drbg, output.data, length) || !outside(drbg, additional.data, additional.length) ||
      !tc_internal_ranges_disjoint(output.data, length, additional.data, additional.length) ||
      !input_length_valid(drbg, additional.length) ||
      (prediction_resistance && !drbg->prediction_resistance))
    return TC_DRBG_ARGUMENT;
#if SIZE_MAX > TC_DRBG_MAX_REQUEST_BYTES
  if (length > TC_DRBG_MAX_REQUEST_BYTES)
    return TC_DRBG_LIMIT;
#endif

  /* Section 9.3.1 steps 7 and 9: reseed on request or when the interval is
   * spent. The reseed consumes the additional input. */
  if (prediction_resistance || drbg->reseed_counter > drbg->reseed_interval) {
    const TC_bytes empty = {NULL, 0};
    result = reseed(drbg, additional);
    if (result != TC_DRBG_OK) {
      TC_secure_zero(output.data, length);
      return result;
    }
    additional = empty;
  }
  result = record(drbg, mechanism_generate(drbg, output.data, length, additional));
  if (result != TC_DRBG_OK) {
    TC_secure_zero(output.data, length);
    return result;
  }
  ++drbg->reseed_counter;
  return TC_DRBG_OK;
}

void TC_DRBG_uninstantiate(TC_DRBG* drbg)
{
  if (drbg != NULL)
    TC_secure_zero(drbg, sizeof *drbg);
}

#endif

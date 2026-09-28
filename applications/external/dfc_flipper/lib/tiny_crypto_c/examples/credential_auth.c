/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "credential_auth.h"

/* Map a key accepted by card_key_parameters_select to its card algorithm ID. */
static ExampleCardAlgorithm card_algorithm(const TC_X509_public_key* key)
{
  if (key->type == TC_KEY_RSA) {
    if (key->bits == 1024) return EXAMPLE_CARD_ALGORITHM_RSA_1024;
    if (key->bits == 2048) return EXAMPLE_CARD_ALGORITHM_RSA_2048;
    return EXAMPLE_CARD_ALGORITHM_RSA_3072;
  }
  return key->curve == TC_EC_P384 ?
      EXAMPLE_CARD_ALGORITHM_EC_P384 : EXAMPLE_CARD_ALGORITHM_EC_P256;
}

/* Extract the single signature from a complete 7C response template.
 * signature borrows encoded and changes only on OK. */
static TC_TLV_result response_signature(TC_bytes encoded, TC_bytes* signature)
{
  enum { AUTH_TEMPLATE = 0x7c, AUTH_RESPONSE = 0x82 };
  const TC_TLV_limits limits = {
    EXAMPLE_CARD_KEY_RESPONSE_BYTES,EXAMPLE_CARD_KEY_RESPONSE_BYTES,2,2};
  TC_TLV_element outer, inner;
  TC_TLV_result result = TC_TLV_read(encoded.data,encoded.length,TC_TLV_ISO7816,&limits,&outer);
  if (result != TC_TLV_OK) return result;
  if (outer.header.tag_length != 1 || outer.header.tag[0] != AUTH_TEMPLATE ||
      outer.encoded.length != encoded.length) return TC_TLV_INVALID;
  result = TC_TLV_read(outer.value.data,outer.value.length,TC_TLV_ISO7816,&limits,&inner);
  if (result != TC_TLV_OK) return result;
  if (inner.header.tag_length != 1 || inner.header.tag[0] != AUTH_RESPONSE ||
      inner.encoded.length != outer.value.length || !inner.value.length) return TC_TLV_INVALID;
  *signature = inner.value;
  return TC_TLV_OK;
}

ExampleCardKeyResult example_card_check_key(ExampleCardIO* io, ExampleCardKeyReference reference,
    const TC_X509_public_key* key, const ExampleCardKeyPolicy* policy,
    const TC_X509_signature_provider* provider, TC_random_fn random, void* random_context,
    ExampleCardKeyWorkspace* workspace, size_t* work)
{
  if (!io || !io->transmit || !key || !policy || !provider || !random || !workspace || !work)
    return EXAMPLE_CARD_KEY_ERROR;
  if (reference != EXAMPLE_CARD_KEY_PIV_AUTHENTICATION &&
      reference != EXAMPLE_CARD_KEY_CARD_AUTHENTICATION) return EXAMPLE_CARD_KEY_ERROR;
  if (!provider->verify_digest) return EXAMPLE_CARD_KEY_UNSUPPORTED;
  if (io->stopped) return EXAMPLE_CARD_KEY_TRANSPORT;
  if (!*work || !io->exchanges_left) return EXAMPLE_CARD_KEY_LIMIT;

  ExampleCardKeyResult result;
  ExampleCardKeyParameters parameters;
  switch (example_card_key_parameters_select(key,policy,&parameters)) {
    case EXAMPLE_CARD_KEY_POLICY_OK: break;
    case EXAMPLE_CARD_KEY_POLICY_INVALID: result = EXAMPLE_CARD_KEY_INVALID; goto cleanup;
    case EXAMPLE_CARD_KEY_POLICY_UNSUPPORTED: result = EXAMPLE_CARD_KEY_UNSUPPORTED; goto cleanup;
    default: result = EXAMPLE_CARD_KEY_ERROR; goto cleanup;
  }
  TC_bytes challenge;
  TC_work_budget challenge_work = {*work > UINT32_MAX ? UINT32_MAX : (uint32_t)*work};
  const uint32_t before_prepare = challenge_work.remaining;
  switch (TC_key_challenge_prepare(key,&parameters.challenge,
      (TC_random_source){random,random_context},&workspace->key,&challenge_work,&challenge)) {
    case TC_KEY_CHALLENGE_OK: break;
    case TC_KEY_CHALLENGE_INVALID: result = EXAMPLE_CARD_KEY_INVALID; goto prepare_cleanup;
    case TC_KEY_CHALLENGE_UNSUPPORTED: result = EXAMPLE_CARD_KEY_UNSUPPORTED; goto prepare_cleanup;
    case TC_KEY_CHALLENGE_LIMIT: result = EXAMPLE_CARD_KEY_LIMIT; goto prepare_cleanup;
    default: result = EXAMPLE_CARD_KEY_ERROR; goto prepare_cleanup;
  }
  *work -= before_prepare - challenge_work.remaining;
  ExampleCardResponse response;
  const ExampleCardResult sent = example_card_authenticate(io,card_algorithm(key),
      reference,challenge,
      workspace->response,sizeof workspace->response,&response);
  if (sent != EXAMPLE_CARD_OK) {
    result = sent == EXAMPLE_CARD_LIMIT ? EXAMPLE_CARD_KEY_LIMIT :
        sent == EXAMPLE_CARD_STATUS ? EXAMPLE_CARD_KEY_INVALID : EXAMPLE_CARD_KEY_TRANSPORT;
    goto cleanup;
  }
  TC_bytes value;
  if (*work < response.length) {
    result = EXAMPLE_CARD_KEY_LIMIT;
    goto cleanup;
  }
  *work -= response.length;
  if (response_signature((TC_bytes){workspace->response,response.length},&value) != TC_TLV_OK) {
    result = EXAMPLE_CARD_KEY_INVALID;
    goto cleanup;
  }
  challenge_work.remaining = *work > UINT32_MAX ? UINT32_MAX : (uint32_t)*work;
  const uint32_t before_verify = challenge_work.remaining;
  switch (TC_key_challenge_verify(key,value,provider,&workspace->key,&challenge_work)) {
    case TC_KEY_CHALLENGE_OK: result = EXAMPLE_CARD_KEY_VERIFIED; break;
    case TC_KEY_CHALLENGE_INVALID: result = EXAMPLE_CARD_KEY_INVALID; break;
    case TC_KEY_CHALLENGE_UNSUPPORTED: result = EXAMPLE_CARD_KEY_UNSUPPORTED; break;
    case TC_KEY_CHALLENGE_LIMIT: result = EXAMPLE_CARD_KEY_LIMIT; break;
    default: result = EXAMPLE_CARD_KEY_ERROR; break;
  }
  *work -= before_verify - challenge_work.remaining;
  goto cleanup;
prepare_cleanup:
  *work -= before_prepare - challenge_work.remaining;
cleanup:
  TC_secure_zero(workspace,sizeof *workspace);
  return result;
}

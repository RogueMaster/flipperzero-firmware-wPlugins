/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Key proofs over GENERAL AUTHENTICATE (SP 800-73-5 Part 2 3.2.4 and
 * Appendix A.4, TWIC Part 2 v5 5.3). */
#include <tiny_crypto/piv_key_proof.h>
#if TC_ENABLE_PIV_KEY_PROOF
#include "internal.h"
#include "piv_key_proof_internal.h"
#include "piv_link_internal.h"
#include "piv_template_internal.h"

enum { GENERAL_AUTHENTICATE = 0x87 };

/* Key references of Part 1 Table 5 that sign challenges on the selected
 * application. The TWIC application offers 9E only (TWIC Part 2 v5 5.3
 * syntax table, TWIC Part 3 v4 4.4.4). */
static int key_reference_valid(const TC_PIV_link* link, uint8_t key)
{
  if (link->application == TC_PIV_APPLICATION_TWIC)
    return key == TC_PIV_KEY_CARD_AUTHENTICATION;
  return key == TC_PIV_KEY_PIV_AUTHENTICATION || key == TC_PIV_KEY_DIGITAL_SIGNATURE ||
         key == TC_PIV_KEY_CARD_AUTHENTICATION;
}

/* The TWIC application proves under the profile of its SELECT. A TWIC
 * card's PIV application reports TC_PIV_CARD, so the caller names the TWIC
 * profile there. */
static int profile_valid(const TC_PIV_link* link, TC_PIV_card_profile profile)
{
  return link->application != TC_PIV_APPLICATION_TWIC || profile == link->profile;
}

/* 1 when the inputs stay outside the link storage, which every exchange
 * rewrites, and the workspace is disjoint from every input. */
static int inputs_disjoint(const TC_PIV_link* link, const TC_PIV_key_proof_request* request,
                           const TC_X509_signature_provider* provider,
                           const TC_PIV_key_proof_workspace* workspace, const TC_work_budget* work)
{
  const TC_X509_certificate* certificate = request->certificate;
  const TC_X509_public_key* key = &certificate->public_key;
  const struct {
    const void* data;
    size_t length;
  } inputs[] = {{request, sizeof *request},
                {certificate, sizeof *certificate},
                {certificate->encoded.data, certificate->encoded.length},
                {key->key.data, key->key.length},
                {key->modulus.data, key->modulus.length},
                {key->exponent.data, key->exponent.length},
                {provider, sizeof *provider},
                {work, sizeof *work}};
  const size_t size = sizeof *workspace;
  if (!tc_piv_link_disjoint(link, workspace, size))
    return 0;
  for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i)
    if (!tc_piv_link_disjoint(link, inputs[i].data, inputs[i].length) ||
        !tc_internal_ranges_disjoint(workspace, size, inputs[i].data, inputs[i].length))
      return 0;
  return 1;
}

static int arguments_valid(const TC_PIV_link* link, const TC_PIV_key_proof_request* request,
                           TC_random_source random, const TC_X509_signature_provider* provider,
                           const TC_PIV_key_proof_workspace* workspace, const TC_work_budget* work)
{
  if (!tc_piv_link_ready(link) || !request || !request->certificate || !random.fill || !provider ||
      !workspace || !work)
    return 0;
  return tc_piv_key_policy_valid(&request->policy) &&
         inputs_disjoint(link, request, provider, workspace, work);
}

/* Link state rules checked before any random request or command. */
static TC_PIV_result proof_allowed(const TC_PIV_link* link, const TC_PIV_key_proof_request* request)
{
  if (link->application == TC_PIV_APPLICATION_NONE)
    return TC_PIV_REFUSED;
  if (!key_reference_valid(link, request->key_reference) ||
      !profile_valid(link, request->policy.profile))
    return TC_PIV_ARGUMENT;
  /* TWIC Part 2 v5 5.3: GENERAL AUTHENTICATE is a NEXGEN command of the TWIC
   * application. A Legacy card proves 9E on its PIV application. */
  if (link->application == TC_PIV_APPLICATION_TWIC && link->profile != TC_TWIC_NEXGEN_CARD)
    return TC_PIV_UNSUPPORTED;
  if (link->flags & TC_PIV_LINK_SM_LOST)
    return TC_PIV_REFUSED;
  /* Part 1 Table 5: on contactless, 9A and 9C need the VCI and 9E is
   * Always. */
  if (link->interface == TC_PIV_CONTACTLESS &&
      request->key_reference != TC_PIV_KEY_CARD_AUTHENTICATION && !(link->flags & TC_PIV_LINK_VCI))
    return TC_PIV_REFUSED;
  return TC_PIV_OK;
}

static TC_PIV_result challenge_result(TC_key_challenge_result result)
{
  switch (result) {
  case TC_KEY_CHALLENGE_OK:
    return TC_PIV_OK;
  case TC_KEY_CHALLENGE_INVALID:
    return TC_PIV_INVALID;
  case TC_KEY_CHALLENGE_LIMIT:
    return TC_PIV_LIMIT;
  case TC_KEY_CHALLENGE_UNSUPPORTED:
    return TC_PIV_UNSUPPORTED;
  default:
    return TC_PIV_ERROR;
  }
}

/* Send GENERAL AUTHENTICATE with 7C {82 00, 81 L challenge} in the order of
 * Part 2 A.4.1, and borrow the signature from 7C {82 L signature}. */
static TC_PIV_result sign_challenge(TC_PIV_link* link, uint8_t algorithm, uint8_t key,
                                    TC_bytes challenge, TC_PIV_key_proof_workspace* workspace,
                                    TC_bytes* signature)
{
  const tc_piv_template_item items[] = {{TC_PIV_TEMPLATE_RESPONSE, {{NULL, 0}}},
                                        {TC_PIV_TEMPLATE_CHALLENGE, {challenge}}};
  const size_t request_length = tc_piv_template_size(items, 2);
  if (!request_length || request_length > sizeof workspace->request)
    return TC_PIV_LIMIT;
  (void)tc_piv_template_write(workspace->request, items, 2);
  /* Le 00 (Part 2 A.4.1). A secured link sends it under secure messaging. */
  const TC_APDU_command command = {{workspace->request, request_length},
                                   link->response_ne,
                                   TC_PIV_PLAIN_CLA,
                                   GENERAL_AUTHENTICATE,
                                   algorithm,
                                   key};
  const TC_buffer response = {workspace->response, sizeof workspace->response};
  TC_APDU_response answer;
  const TC_PIV_result result = tc_piv_link_transceive(link, TC_PIV_COMMAND_GENERAL_AUTHENTICATE,
                                                      &command, response, &answer);
  if (result != TC_PIV_OK)
    return result;
  if (answer.sw != TC_PIV_SW_SUCCESS_VALUE)
    return tc_piv_link_fail(link, response, answer.sw, TC_PIV_CARD_STATUS);
  static const uint8_t tags[] = {TC_PIV_TEMPLATE_RESPONSE};
  TC_bytes value = {NULL, 0};
  if (tc_piv_template_read(answer.data, tags, 1, &value) != TC_TLV_OK || !value.length)
    return tc_piv_link_fail(link, response, 0, TC_PIV_INVALID);
  *signature = value;
  return TC_PIV_OK;
}

TC_PIV_result TC_PIV_key_prove(TC_PIV_link* link, const TC_PIV_key_proof_request* request,
                               TC_random_source random, const TC_X509_signature_provider* provider,
                               TC_PIV_key_proof_workspace* workspace, TC_work_budget* work)
{
  if (!arguments_valid(link, request, random, provider, workspace, work))
    return TC_PIV_ARGUMENT;
  TC_PIV_result result = proof_allowed(link, request);
  if (result != TC_PIV_OK)
    return result;
  if (!provider->verify_digest)
    return TC_PIV_UNSUPPORTED;
  const TC_X509_public_key* key = &request->certificate->public_key;
  TC_PIV_key_parameters parameters;
  TC_bytes challenge = {NULL, 0}, signature = {NULL, 0};
  memset(&parameters, 0, sizeof parameters);
  result = TC_PIV_key_parameters_select(request->certificate, &request->policy, &parameters);
  if (result == TC_PIV_OK)
    result = challenge_result(TC_key_challenge_prepare(key, &parameters.challenge, random,
                                                       &workspace->challenge, work, &challenge));
  if (result == TC_PIV_OK)
    result = sign_challenge(link, parameters.algorithm, request->key_reference, challenge,
                            workspace, &signature);
  if (result == TC_PIV_OK)
    result = challenge_result(
        TC_key_challenge_verify(key, signature, provider, &workspace->challenge, work));
  TC_secure_zero(workspace, sizeof *workspace);
  return result;
}
#endif

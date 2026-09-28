/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/rsa.h>
#if TC_ENABLE_RSA
#define TC_MP_WORD_BITS TC_RSA_WORD_BITS
#include "rsa_internal.h"
#include "rsa_pss_internal.h"
#include "rsa_private_internal.h"
#include "rsa_crt_internal.h"
#include "rsa_oaep_internal.h"
#include "rsa_keygen_internal.h"
#include "pki_storage_internal.h"

#define TC_RSA_KEYGEN_MARKER UINT32_C(0x524b4731)

size_t TC_RSA_verify_workspace_words(size_t bits)
{
  if (bits != 1024 && bits != 2048 && bits != 3072) return 0;
  return TC_RSA_VERIFY_WORKSPACE_WORDS(bits);
}

size_t TC_RSA_validate_workspace_words(size_t bits)
{
  if (bits != 1024 && bits != 2048 && bits != 3072) return 0;
  return TC_RSA_VALIDATE_WORKSPACE_WORDS(bits);
}

size_t TC_RSA_crt_workspace_words(size_t bits)
{
  if (bits != 1024 && bits != 2048 && bits != 3072) return 0;
  return TC_RSA_CRT_WORKSPACE_WORDS(bits);
}

size_t TC_RSA_sign_workspace_words(size_t bits)
{
  if (bits != 1024 && bits != 2048 && bits != 3072) return 0;
  return TC_RSA_SIGN_WORKSPACE_WORDS(bits);
}

size_t TC_RSA_decrypt_workspace_words(size_t bits)
{
  if (bits != 1024 && bits != 2048 && bits != 3072) return 0;
  return TC_RSA_DECRYPT_WORKSPACE_WORDS(bits);
}

size_t TC_RSA_encrypt_workspace_words(size_t bits)
{
  return TC_RSA_verify_workspace_words(bits);
}

size_t TC_RSA_keygen_workspace_words(size_t bits)
{
  if (bits != 1024 && bits != 2048 && bits != 3072) return 0;
  return TC_RSA_KEYGEN_WORKSPACE_WORDS(bits);
}

static int tc_rsa_keygen_output_check(const TC_RSA_keygen_state* state,
    const TC_RSA_keygen_output* output, const TC_RSA_workspace* workspace,
    size_t bits)
{
  const size_t modulus_length = bits / 8, prime_length = bits / 16;
  const TC_buffer buffers[] = {
    output->modulus,output->exponent,output->d,output->p,output->q
  };
  const size_t needed[] = {modulus_length,3,modulus_length,prime_length,prime_length};
  if (!workspace->words || (uintptr_t)workspace->words % sizeof(TC_RSA_word) ||
      workspace->capacity > SIZE_MAX / sizeof *workspace->words ||
      workspace->capacity < TC_RSA_KEYGEN_WORKSPACE_WORDS(bits)) return 0;
  for (size_t i = 0; i < sizeof buffers / sizeof *buffers; ++i) {
    if (!buffers[i].data || buffers[i].capacity < needed[i]) return 0;
    if (!tc_internal_ranges_disjoint(buffers[i].data,buffers[i].capacity,
          state,sizeof *state) ||
        !tc_internal_ranges_disjoint(buffers[i].data,buffers[i].capacity,
          output,sizeof *output) ||
        !tc_internal_ranges_disjoint(buffers[i].data,buffers[i].capacity,
          workspace,sizeof *workspace) ||
        !tc_internal_ranges_disjoint(buffers[i].data,buffers[i].capacity,
          workspace->words,workspace->capacity * sizeof *workspace->words)) return 0;
    for (size_t j = 0; j < i; ++j)
      if (!tc_internal_ranges_disjoint(buffers[i].data,buffers[i].capacity,
            buffers[j].data,buffers[j].capacity)) return 0;
  }
  return tc_internal_ranges_disjoint(state,sizeof *state,workspace,sizeof *workspace) &&
      tc_internal_ranges_disjoint(state,sizeof *state,workspace->words,
        workspace->capacity * sizeof *workspace->words);
}

TC_RSA_result TC_RSA_keygen_init(TC_RSA_keygen_state* state, size_t bits,
    const TC_RSA_keygen_output* output, TC_RSA_keygen_limits limits,
    const TC_RSA_workspace* workspace)
{
  if (!state || !output || !workspace) return TC_RSA_ARGUMENT;
  if (state->marker == TC_RSA_KEYGEN_MARKER) return TC_RSA_ARGUMENT;
  if (!TC_RSA_keygen_workspace_words(bits)) return TC_RSA_UNSUPPORTED;
  if (!limits.candidate_attempts || !limits.random_requests) return TC_RSA_LIMIT;
  const size_t length = bits / 8, prime_length = length / 2;
  if (workspace->capacity < TC_RSA_KEYGEN_WORKSPACE_WORDS(bits) ||
      output->modulus.capacity < length || output->exponent.capacity < 3 ||
      output->d.capacity < length || output->p.capacity < prime_length ||
      output->q.capacity < prime_length) return TC_RSA_LIMIT;
  if (!tc_rsa_keygen_output_check(state,output,workspace,bits)) return TC_RSA_ARGUMENT;
  TC_RSA_keygen_state initialized;
  memset(&initialized,0,sizeof initialized);
  initialized.workspace = *workspace;
  initialized.output = *output;
  initialized.marker = TC_RSA_KEYGEN_MARKER;
  initialized.bits = (uint32_t)bits;
  initialized.candidate_limit = limits.candidate_attempts;
  initialized.random_limit = limits.random_requests;
  initialized.phase = TC_RSA_KEYGEN_P_NEW;
  TC_secure_zero(workspace->words,
      TC_RSA_KEYGEN_WORKSPACE_WORDS(bits) * sizeof *workspace->words);
  *state = initialized;
  return TC_RSA_OK;
}

void TC_RSA_keygen_clear(TC_RSA_keygen_state* state)
{
  if (!state) return;
  if (state->marker == TC_RSA_KEYGEN_MARKER &&
      TC_RSA_keygen_workspace_words(state->bits) && state->workspace.words &&
      state->workspace.capacity >= TC_RSA_KEYGEN_WORKSPACE_WORDS(state->bits))
    TC_secure_zero(state->workspace.words,
        TC_RSA_KEYGEN_WORKSPACE_WORDS(state->bits) * sizeof *state->workspace.words);
  TC_secure_zero(state,sizeof *state);
}

static TC_RSA_result tc_rsa_keygen_stop(TC_RSA_keygen_state* state,
    TC_RSA_result result)
{
  TC_RSA_keygen_clear(state);
  return result;
}

static TC_RSA_result tc_rsa_keygen_random(TC_RSA_keygen_state* state,
    TC_random_fn random, void* context, uint8_t* output, size_t length)
{
  if (state->random_requests == state->random_limit)
    return TC_RSA_LIMIT;
  ++state->random_requests;
  return random(context,output,length) == TC_OK ? TC_RSA_OK : TC_RSA_ERROR;
}

static TC_RSA_result tc_rsa_keygen_step(TC_RSA_keygen_state* state,
    TC_random_fn random, void* random_context,
    TC_RSA_cancel_fn cancel, void* cancel_context, uint32_t* work)
{
  if (!work) return TC_RSA_ARGUMENT;
  uint32_t max_work = *work;
#define TC_RSA_KEYGEN_RETURN(value) do { \
    TC_RSA_result tc_rsa_keygen_result = (value); \
    *work = max_work; \
    return tc_rsa_keygen_result; \
  } while (0)
  if (!state || !random || state->marker != TC_RSA_KEYGEN_MARKER ||
      !TC_RSA_keygen_workspace_words(state->bits))
    TC_RSA_KEYGEN_RETURN(TC_RSA_ARGUMENT);
  if (state->phase < TC_RSA_KEYGEN_P_NEW || state->phase > TC_RSA_KEYGEN_Q_ROUND)
    TC_RSA_KEYGEN_RETURN(tc_rsa_keygen_stop(state,TC_RSA_ARGUMENT));
  const size_t length = state->bits / 8, prime_length = length / 2;
  const size_t n = length / sizeof(TC_RSA_word), h = n / 2;
  const size_t required = TC_RSA_KEYGEN_WORKSPACE_WORDS(state->bits);
  if (!state->workspace.words || state->workspace.capacity < required)
    TC_RSA_KEYGEN_RETURN(tc_rsa_keygen_stop(state,TC_RSA_ARGUMENT));
  uint8_t* p = (uint8_t*)state->workspace.words;
  uint8_t* q = p + prime_length;
  TC_RSA_word* scratch = state->workspace.words + n;
  const uint32_t candidate_work = (uint32_t)prime_length + 54;
  const uint32_t setup_work = UINT32_C(24) * (uint32_t)prime_length + 3;
  const uint32_t round_work = UINT32_C(24) * (uint32_t)prime_length + 2;
  for (;;) {
    if (cancel && cancel(cancel_context))
      TC_RSA_KEYGEN_RETURN(tc_rsa_keygen_stop(state,TC_RSA_CANCELLED));
    if (state->phase == TC_RSA_KEYGEN_P_NEW || state->phase == TC_RSA_KEYGEN_Q_NEW) {
      if (state->candidates == state->candidate_limit)
        TC_RSA_KEYGEN_RETURN(tc_rsa_keygen_stop(state,TC_RSA_LIMIT));
      if (max_work < candidate_work) TC_RSA_KEYGEN_RETURN(TC_RSA_IN_PROGRESS);
      max_work -= candidate_work; ++state->candidates;
      uint8_t* candidate = state->phase == TC_RSA_KEYGEN_P_NEW ? p : q;
      TC_RSA_result status = tc_rsa_keygen_random(state,random,random_context,
          candidate,prime_length);
      if (status != TC_RSA_OK)
        TC_RSA_KEYGEN_RETURN(tc_rsa_keygen_stop(state,status));
      candidate[0] |= 0xc0u;
      candidate[prime_length - 1] |= 1u;
      if (!tc_rsa_keygen_candidate_filter(candidate,prime_length)) continue;
      if (state->phase == TC_RSA_KEYGEN_Q_NEW &&
          !tc_rsa_keygen_far_apart(p,q,prime_length,scratch)) continue;
      state->phase = state->phase == TC_RSA_KEYGEN_P_NEW ?
          TC_RSA_KEYGEN_P_PREPARE : TC_RSA_KEYGEN_Q_PREPARE;
    }
    if (state->phase == TC_RSA_KEYGEN_P_PREPARE ||
        state->phase == TC_RSA_KEYGEN_Q_PREPARE) {
      if (max_work < setup_work) TC_RSA_KEYGEN_RETURN(TC_RSA_IN_PROGRESS);
      max_work -= setup_work;
      const uint8_t* candidate = state->phase == TC_RSA_KEYGEN_P_PREPARE ? p : q;
      tc_mp_from_be(scratch,candidate,prime_length);
      state->twos = (uint16_t)tc_mp_miller_rabin_prepare(scratch,h,scratch + 2 * h);
      state->rounds = 0;
      state->phase = state->phase == TC_RSA_KEYGEN_P_PREPARE ?
          TC_RSA_KEYGEN_P_ROUND : TC_RSA_KEYGEN_Q_ROUND;
    }
    if (state->phase == TC_RSA_KEYGEN_P_ROUND || state->phase == TC_RSA_KEYGEN_Q_ROUND) {
      if (max_work < round_work) TC_RSA_KEYGEN_RETURN(TC_RSA_IN_PROGRESS);
      max_work -= round_work;
      TC_RSA_word* base = scratch + h;
      TC_RSA_word* temporary = scratch + 8 * h;
      TC_RSA_result status = tc_rsa_keygen_random(state,random,random_context,
          (uint8_t*)temporary,prime_length);
      if (status != TC_RSA_OK)
        TC_RSA_KEYGEN_RETURN(tc_rsa_keygen_stop(state,status));
      const uint8_t* candidate_bytes = state->phase == TC_RSA_KEYGEN_P_ROUND ? p : q;
      unsigned seen = 0;
      uint8_t* bytes = (uint8_t*)temporary;
      for (size_t i = 0; i < prime_length; ++i) {
        unsigned mask = candidate_bytes[i];
        mask |= mask >> 1; mask |= mask >> 2; mask |= mask >> 4;
        mask |= 0u - seen;
        bytes[i] &= (uint8_t)mask;
        seen |= (unsigned)(candidate_bytes[i] != 0);
      }
      tc_mp_from_be(base,bytes,prime_length);
      TC_RSA_word* last = scratch + 7 * h;
      memcpy(last,scratch,prime_length); --last[0];
      const TC_RSA_word below_last = tc_mp_subtract(temporary,base,last,h);
      TC_RSA_word above_one = (TC_RSA_word)(base[0] & ~1u);
      for (size_t i = 1; i < h; ++i) above_one |= base[i];
      if (!below_last || !above_one) continue;
      if (!tc_mp_miller_rabin_round(scratch,base,h,state->twos,scratch + 2 * h)) {
        state->phase = state->phase == TC_RSA_KEYGEN_P_ROUND ?
            TC_RSA_KEYGEN_P_NEW : TC_RSA_KEYGEN_Q_NEW;
        state->rounds = 0;
        TC_secure_zero(scratch,(6 * n + 2) * sizeof *scratch);
        continue;
      }
      if (++state->rounds != TC_RSA_VALIDATION_ROUNDS) continue;
      if (state->phase == TC_RSA_KEYGEN_P_ROUND) {
        state->phase = TC_RSA_KEYGEN_Q_NEW;
        state->rounds = 0;
        TC_secure_zero(scratch,(6 * n + 2) * sizeof *scratch);
        continue;
      }
      TC_RSA_word *modulus_words, *d_words;
      tc_rsa_keygen_derive(p,q,prime_length,scratch,&modulus_words,&d_words);
      tc_mp_to_be(state->output.modulus.data,modulus_words,length);
      state->output.exponent.data[0] = 1;
      state->output.exponent.data[1] = 0;
      state->output.exponent.data[2] = 1;
      tc_mp_to_be(state->output.d.data,d_words,length);
      memcpy(state->output.p.data,p,prime_length);
      memcpy(state->output.q.data,q,prime_length);
      TC_RSA_keygen_clear(state);
      TC_RSA_KEYGEN_RETURN(TC_RSA_OK);
    }
  }
#undef TC_RSA_KEYGEN_RETURN
}

TC_RSA_result TC_RSA_keygen_step(TC_RSA_keygen_state* state,
    TC_random_source random, TC_RSA_cancel_fn cancel, void* cancel_context,
    TC_work_budget* work)
{
  if (!work) return TC_RSA_ARGUMENT;
  uint32_t available = work->remaining;
  TC_RSA_result result = tc_rsa_keygen_step(state,random.fill,random.context,
      cancel,cancel_context,&available);
  work->remaining = available;
  return result;
}

TC_RSA_result TC_RSA_encode_v15_digest(const TC_RSA_v15_options* options,
    TC_bytes digest, TC_buffer encoded, TC_work_budget* work)
{
  tc_hash_info info;
  if (!options || !work || !digest.data || !encoded.data ||
      !tc_internal_ranges_disjoint(digest.data,digest.length,encoded.data,encoded.capacity) ||
      !tc_internal_ranges_disjoint(options,sizeof *options,encoded.data,encoded.capacity) ||
      !tc_internal_ranges_disjoint(work,sizeof *work,encoded.data,encoded.capacity))
    return TC_RSA_ARGUMENT;
  if (!tc_hash_info_get(options->hash,&info)) return TC_RSA_UNSUPPORTED;
  if (digest.length != info.digest_length) return TC_RSA_ARGUMENT;
  if (encoded.capacity != 128 && encoded.capacity != 256 && encoded.capacity != 384)
    return TC_RSA_UNSUPPORTED;
  if (work->remaining < encoded.capacity) return TC_RSA_LIMIT;
  work->remaining -= (uint32_t)encoded.capacity;
  return tc_rsa_v15_encode(encoded.data,encoded.capacity,info.digest_info.data,
      info.digest_info.length,digest.data,digest.length) == TC_OK ? TC_RSA_OK : TC_RSA_ARGUMENT;
}

TC_RSA_result TC_RSA_encode_pss_digest(const TC_RSA_pss_options* options,
    TC_bytes digest, TC_bytes salt, TC_buffer encoded, TC_work_budget* work)
{
  if (!options || !work || !digest.data || (salt.length && !salt.data) || !encoded.data)
    return TC_RSA_ARGUMENT;
  const TC_bytes inputs[] = {{(const uint8_t*)options,sizeof *options},digest,salt};
  if (!tc_internal_ranges_disjoint(work,sizeof *work,encoded.data,encoded.capacity))
    return TC_RSA_ARGUMENT;
  for (size_t i = 0; i < sizeof inputs / sizeof *inputs; ++i) {
    if (!tc_internal_ranges_disjoint(inputs[i].data,inputs[i].length,encoded.data,encoded.capacity) ||
        !tc_internal_ranges_disjoint(inputs[i].data,inputs[i].length,work,sizeof *work))
      return TC_RSA_ARGUMENT;
  }
  if (salt.length != options->salt_length) return TC_RSA_ARGUMENT;
  if (encoded.capacity != 128 && encoded.capacity != 256 && encoded.capacity != 384)
    return TC_RSA_UNSUPPORTED;
  tc_hash_info info;
  size_t remaining = work->remaining;
  TC_RSA_result result = tc_rsa_pss_prepare(encoded.capacity,encoded.capacity * 8 - 1,
      options->hash,options->mgf_hash,digest.length,salt.length,&remaining,&info);
  if (result != TC_RSA_OK) return result;
  tc_hash_workspace workspace;
  uint8_t block[64];
  remaining = work->remaining;
  result = tc_rsa_pss_encode(encoded.data,encoded.capacity,encoded.capacity * 8 - 1,
      options->hash,options->mgf_hash,digest,salt,block,&workspace,&remaining);
  work->remaining = (uint32_t)remaining;
  if (result != TC_RSA_OK) TC_secure_zero(encoded.data,encoded.capacity);
  TC_secure_zero(block,sizeof block);
  TC_secure_zero(&workspace,sizeof workspace);
  return result;
}

static TC_RSA_result tc_rsa_workspace_inputs(const TC_RSA_workspace* workspace,
    const TC_bytes* inputs, size_t count)
{
  TC_bytes writes;
  TC_TLV_result checked;
  size_t checks = count;
  if ((uintptr_t)workspace->words % sizeof(TC_RSA_word)) return TC_RSA_ARGUMENT;
  checked = tc_pki_storage_span(workspace->words,workspace->capacity,sizeof *workspace->words,&writes);
  if (checked != TC_TLV_OK) return TC_RSA_ARGUMENT;
  for (size_t i = 0; i < count; ++i)
    if (tc_pki_storage_input(&writes,1,inputs[i],&checks) != TC_TLV_OK) return TC_RSA_ARGUMENT;
  return TC_RSA_OK;
}

/* Output and scratch are separate from each other and every borrowed input. */
static TC_RSA_result tc_rsa_output_inputs(const TC_RSA_workspace* workspace,
    const TC_bytes* inputs, size_t count, TC_bytes output)
{
  TC_RSA_result checked = tc_rsa_workspace_inputs(workspace,inputs,count);
  if (checked != TC_RSA_OK) return checked;
  checked = tc_rsa_workspace_inputs(workspace,&output,1);
  if (checked != TC_RSA_OK) return checked;
  size_t checks = count;
  for (size_t i = 0; i < count; ++i)
    if (tc_pki_storage_input(&output,1,inputs[i],&checks) != TC_TLV_OK)
      return TC_RSA_ARGUMENT;
  return TC_RSA_OK;
}

static TC_RSA_result tc_rsa_control_inputs(const TC_RSA_workspace* workspace,
    TC_bytes output, const void* first, size_t first_size,
    const void* second, size_t second_size)
{
  if (!workspace || !first || !second) return TC_RSA_ARGUMENT;
  const TC_bytes controls[] = {
    {(const uint8_t*)first,first_size},{(const uint8_t*)second,second_size}
  };
  return tc_rsa_output_inputs(workspace,controls,2,output);
}

static TC_RSA_result tc_rsa_public_inputs(const TC_RSA_public_key* key,
    TC_bytes first, TC_bytes second, TC_bytes output, const TC_RSA_workspace* workspace)
{
  TC_bytes inputs[6];
  if (!key || !workspace) return TC_RSA_ARGUMENT;
  inputs[0] = (TC_bytes){(const uint8_t*)key,sizeof *key};
  inputs[1] = (TC_bytes){(const uint8_t*)workspace,sizeof *workspace};
  inputs[2] = (TC_bytes){key->modulus.data,key->modulus.length};
  inputs[3] = (TC_bytes){key->exponent.data,key->exponent.length};
  inputs[4] = (TC_bytes){first.data,first.length};
  inputs[5] = (TC_bytes){second.data,second.length};
  return tc_rsa_output_inputs(workspace,inputs,sizeof inputs / sizeof *inputs,output);
}

static TC_RSA_result tc_rsa_private_inputs(const TC_RSA_private_key* key,
    TC_bytes digest, TC_bytes output, const TC_RSA_workspace* workspace)
{
  if (!key || !workspace) return TC_RSA_ARGUMENT;
  const TC_bytes inputs[] = {
    {(const uint8_t*)key,sizeof *key},
    {(const uint8_t*)workspace,sizeof *workspace},
    {key->public_key.modulus.data,key->public_key.modulus.length},
    {key->public_key.exponent.data,key->public_key.exponent.length},
    {key->d.data,key->d.length},{key->p.data,key->p.length},{key->q.data,key->q.length},
    {digest.data,digest.length}
  };
  TC_RSA_result checked = tc_rsa_output_inputs(workspace,inputs,sizeof inputs / sizeof *inputs,output);
  if (checked != TC_RSA_OK) return checked;
  const size_t length = key->public_key.modulus.length;
  if (!key->d.length || !key->p.length || !key->q.length ||
      key->d.length > length || key->p.length > length || key->q.length > length)
    return TC_RSA_INVALID;
  return TC_RSA_OK;
}

TC_RSA_result TC_RSA_validate_crt(const TC_RSA_private_key* key, const TC_RSA_crt* crt,
    const TC_RSA_workspace* workspace, TC_work_budget* work)
{
  if (!crt || !work) return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_private_inputs(key,(TC_bytes){NULL,0},
      (TC_bytes){NULL,0},workspace);
  if (result != TC_RSA_OK) return result;
  result = tc_rsa_workspace_inputs(workspace,
      &(TC_bytes){(const uint8_t*)work,sizeof *work},1);
  if (result != TC_RSA_OK) return result;
  const TC_bytes inputs[] = {
    {(const uint8_t*)crt,sizeof *crt},
    {crt->dp.data,crt->dp.length},{crt->dq.data,crt->dq.length},
    {crt->q_inverse.data,crt->q_inverse.length}
  };
  result = tc_rsa_workspace_inputs(workspace,inputs,sizeof inputs / sizeof *inputs);
  if (result != TC_RSA_OK) return result;
  result = tc_rsa_public_key_check(key->public_key.modulus.data,key->public_key.modulus.length,
      key->public_key.exponent.data,key->public_key.exponent.length);
  if (result != TC_RSA_OK) return result;
  size_t available = work->remaining;
  result = tc_rsa_crt_consistent(key->public_key.modulus.length,key->d,key->p,key->q,
      crt->dp,crt->dq,crt->q_inverse,workspace->words,workspace->capacity,&available);
  work->remaining = (uint32_t)available;
  return result;
}

TC_RSA_result TC_RSA_derive_crt(const TC_RSA_private_key* key,
    const TC_RSA_crt_output* output, const TC_RSA_workspace* workspace,
    TC_work_budget* work)
{
  if (!output || !work) return TC_RSA_ARGUMENT;
  TC_RSA_result status = tc_rsa_private_inputs(key,(TC_bytes){NULL,0},
      (TC_bytes){NULL,0},workspace);
  if (status != TC_RSA_OK) return status;
  status = tc_rsa_workspace_inputs(workspace,
      &(TC_bytes){(const uint8_t*)work,sizeof *work},1);
  if (status != TC_RSA_OK) return status;
  status = tc_rsa_public_key_check(key->public_key.modulus.data,
      key->public_key.modulus.length,key->public_key.exponent.data,
      key->public_key.exponent.length);
  if (status != TC_RSA_OK) return status;
  const size_t length = key->public_key.modulus.length, prime_length = length / 2;
  const TC_buffer buffers[] = {output->dp,output->dq,output->q_inverse};
  const TC_bytes metadata = {(const uint8_t*)output,sizeof *output};
  for (size_t i = 0; i < sizeof buffers / sizeof *buffers; ++i) {
    if (!buffers[i].data) return TC_RSA_ARGUMENT;
    if (buffers[i].capacity < prime_length) return TC_RSA_LIMIT;
    status = tc_rsa_private_inputs(key,(TC_bytes){NULL,0},
        (TC_bytes){buffers[i].data,buffers[i].capacity},workspace);
    if (status != TC_RSA_OK) return status;
    status = tc_rsa_output_inputs(workspace,&metadata,1,
        (TC_bytes){buffers[i].data,buffers[i].capacity});
    if (status != TC_RSA_OK) return status;
    if (!tc_internal_ranges_disjoint(buffers[i].data,buffers[i].capacity,
          work,sizeof *work)) return TC_RSA_ARGUMENT;
    for (size_t j = 0; j < i; ++j)
      if (!tc_internal_ranges_disjoint(buffers[i].data,buffers[i].capacity,
            buffers[j].data,buffers[j].capacity)) return TC_RSA_ARGUMENT;
  }
  tc_mp_word *dp, *dq, *inverse;
  size_t available = work->remaining;
  status = tc_rsa_crt_derive(length,key->d,key->p,key->q,workspace->words,
      workspace->capacity,&available,&dp,&dq,&inverse);
  work->remaining = (uint32_t)available;
  if (status == TC_RSA_OK) {
    tc_mp_to_be(output->dp.data,dp,prime_length);
    tc_mp_to_be(output->dq.data,dq,prime_length);
    tc_mp_to_be(output->q_inverse.data,inverse,prime_length);
  }
  if (workspace->capacity >= TC_RSA_CRT_WORKSPACE_WORDS(length * 8))
    TC_secure_zero(workspace->words,TC_RSA_CRT_WORKSPACE_WORDS(length * 8) *
        sizeof *workspace->words);
  return status;
}

static TC_RSA_result tc_rsa_crt_inputs(const TC_RSA_private_key* key,
    const TC_RSA_crt* crt, TC_bytes output, const TC_RSA_workspace* workspace)
{
  if (!crt) return TC_RSA_ARGUMENT;
  const TC_bytes inputs[] = {
    {(const uint8_t*)crt,sizeof *crt},
    {crt->dp.data,crt->dp.length},{crt->dq.data,crt->dq.length},
    {crt->q_inverse.data,crt->q_inverse.length}
  };
  TC_RSA_result status = tc_rsa_output_inputs(workspace,inputs,
      sizeof inputs / sizeof *inputs,output);
  if (status != TC_RSA_OK) return status;
  const size_t prime_length = key->public_key.modulus.length / 2;
  for (size_t i = 1; i < sizeof inputs / sizeof *inputs; ++i) {
    if (!inputs[i].data) return TC_RSA_ARGUMENT;
    if (!inputs[i].length || inputs[i].length > 2 * prime_length) return TC_RSA_INVALID;
    size_t excess = inputs[i].length > prime_length ? inputs[i].length - prime_length : 0;
    for (size_t j = 0; j < excess; ++j)
      if (inputs[i].data[j]) return TC_RSA_INVALID;
  }
  return TC_RSA_OK;
}

/* Additional output metadata uses the same disjoint-range rules as bytes. */
static TC_RSA_result tc_rsa_decrypt_inputs(const TC_RSA_private_key* key,
    TC_bytes ciphertext, TC_bytes label, uint8_t* plaintext, size_t capacity,
    size_t* plaintext_length, const TC_RSA_workspace* workspace)
{
  if (!plaintext_length || (uintptr_t)plaintext_length % sizeof *plaintext_length)
    return TC_RSA_ARGUMENT;
  TC_RSA_result status = tc_rsa_private_inputs(key,ciphertext,
      (TC_bytes){plaintext,capacity},workspace);
  if (status != TC_RSA_OK) return status;
  TC_bytes scratch;
  if (tc_pki_storage_span(workspace->words,workspace->capacity,sizeof *workspace->words,
      &scratch) != TC_TLV_OK) return TC_RSA_ARGUMENT;
  const TC_bytes writes[] = {scratch,{plaintext,capacity},
      {(const uint8_t*)plaintext_length,sizeof *plaintext_length}};
  size_t checks = 3;
  if (tc_pki_storage_input(writes,3,(TC_bytes){label.data,label.length},&checks) != TC_TLV_OK)
    return TC_RSA_ARGUMENT;
  checks = 2;
  if (tc_pki_storage_input(writes,2,writes[2],&checks) != TC_TLV_OK) return TC_RSA_ARGUMENT;
  /* Treat the length object as another output when checking key and ciphertext. */
  return tc_rsa_private_inputs(key,ciphertext,writes[2],workspace);
}

static TC_RSA_result tc_rsa_validate_private_key(const TC_RSA_private_key* key,
    TC_random_fn random, void* random_context, size_t max_attempts,
    const TC_RSA_workspace* workspace, uint32_t* work)
{
  if (!random) return TC_RSA_ARGUMENT;
  TC_RSA_result checked = tc_rsa_private_inputs(key,(TC_bytes){NULL,0},
      (TC_bytes){NULL,0},workspace);
  if (checked != TC_RSA_OK) return checked;
  const size_t length = key->public_key.modulus.length;
  return tc_rsa_private_magnitudes_check(key->public_key.modulus.data,length,
      key->public_key.exponent.data,key->public_key.exponent.length,
      key->d,key->p,key->q,TC_RSA_VALIDATION_ROUNDS,random,random_context,
      max_attempts,workspace->words,workspace->capacity,work);
}

TC_RSA_result TC_RSA_validate_private_key(const TC_RSA_private_key* key,
    const TC_RSA_workspace* workspace, TC_RSA_execution* execution)
{
  if (!workspace || !execution) return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_workspace_inputs(workspace,
      &(TC_bytes){(const uint8_t*)execution,sizeof *execution},1);
  if (result != TC_RSA_OK) return result;
  result = tc_rsa_validate_private_key(key,execution->random.fill,
      execution->random.context,execution->random_attempts,workspace,
      &execution->work.remaining);
  return result;
}

static TC_RSA_result tc_rsa_sign_inputs(const TC_RSA_private_key* key,
    TC_bytes digest, uint8_t* signature, size_t signature_length,
    TC_random_fn random, size_t max_attempts, const TC_RSA_workspace* workspace)
{
  if (!random) return TC_RSA_ARGUMENT;
  TC_RSA_result status = tc_rsa_private_inputs(key,digest,
      (TC_bytes){signature,signature_length},workspace);
  if (status != TC_RSA_OK) return status;
  status = tc_rsa_public_key_check(key->public_key.modulus.data,key->public_key.modulus.length,
      key->public_key.exponent.data,key->public_key.exponent.length);
  if (status != TC_RSA_OK) return status;
  if (signature_length != key->public_key.modulus.length) return TC_RSA_INVALID;
  if (!max_attempts || workspace->capacity < 14 * (signature_length / sizeof(TC_RSA_word)))
    return TC_RSA_LIMIT;
  return TC_RSA_OK;
}

static TC_RSA_result tc_rsa_sign_encoded(const TC_RSA_private_key* key,
    const TC_RSA_crt* crt, uint8_t* signature,
    TC_random_fn random, void* random_context, size_t max_attempts,
    const TC_RSA_workspace* workspace, size_t* work)
{
  const size_t length = key->public_key.modulus.length, n = length / sizeof(TC_RSA_word);
  if (crt) return tc_rsa_crt_private_operation(key->public_key.modulus.data,length,
      key->public_key.exponent.data,key->public_key.exponent.length,key->p,key->q,crt,
      (const uint8_t*)(workspace->words + 13 * n),signature,random,random_context,
      max_attempts,workspace->words,13 * n,work);
  return tc_rsa_private_operation_magnitude(key->public_key.modulus.data,length,
      key->public_key.exponent.data,key->public_key.exponent.length,key->d,
      (const uint8_t*)(workspace->words + 13 * n),signature,random,random_context,
      max_attempts,workspace->words,13 * n,work);
}

static TC_RSA_result tc_rsa_sign_v15_digest(const TC_RSA_private_key* key,
    const TC_RSA_crt* crt,
    TC_hash_algorithm hash, TC_bytes digest, uint8_t* signature, size_t signature_length,
    TC_random_fn random, void* random_context, size_t max_attempts,
    const TC_RSA_workspace* workspace, size_t* work)
{
  size_t max_work = *work;
  tc_hash_info info;
  TC_RSA_result status = tc_rsa_sign_inputs(key,digest,signature,signature_length,
      random,max_attempts,workspace);
  if (status != TC_RSA_OK) return status;
  if (crt) {
    status = tc_rsa_crt_inputs(key,crt,(TC_bytes){signature,signature_length},workspace);
    if (status != TC_RSA_OK) return status;
  }
  if (!tc_hash_info_get(hash,&info)) return TC_RSA_UNSUPPORTED;
  if (!digest.data || digest.length != info.digest_length) return TC_RSA_ARGUMENT;
  const size_t length = key->public_key.modulus.length;
  if (!tc_rsa_v15_size(length,info.digest_info.length,digest.length)) return TC_RSA_INVALID;
  const size_t n = length / sizeof(TC_RSA_word), required = 14 * n;
  if (max_work < length) return TC_RSA_LIMIT;
  max_work -= length;
  uint8_t* encoded = (uint8_t*)(workspace->words + 13 * n);
  if (tc_rsa_v15_encode(encoded,length,info.digest_info.data,info.digest_info.length,
      digest.data,digest.length) != TC_OK) status = TC_RSA_ARGUMENT;
  else status = tc_rsa_sign_encoded(key,crt,signature,random,random_context,max_attempts,workspace,&max_work);
  TC_secure_zero(workspace->words,required * sizeof(TC_RSA_word));
  *work = max_work;
  return status;
}

TC_RSA_result TC_RSA_sign_v15_digest(const TC_RSA_private_key* key,
    const TC_RSA_v15_options* options, TC_bytes digest,
    const TC_RSA_workspace* workspace, TC_buffer signature,
    TC_RSA_execution* execution)
{
  if (!key || !options || !execution) return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_control_inputs(workspace,
      (TC_bytes){signature.data,signature.capacity},options,sizeof *options,
      execution,sizeof *execution);
  if (result != TC_RSA_OK) return result;
  size_t work = execution->work.remaining;
  result = tc_rsa_sign_v15_digest(key,key->crt,options->hash,digest,
      signature.data,signature.capacity,execution->random.fill,
      execution->random.context,execution->random_attempts,workspace,&work);
  execution->work.remaining = (uint32_t)work;
  return result;
}

static TC_RSA_result tc_rsa_sign_pss_digest(const TC_RSA_private_key* key,
    const TC_RSA_crt* crt,
    TC_hash_algorithm hash, TC_hash_algorithm mgf_hash, size_t salt_length,
    TC_bytes digest, uint8_t* signature, size_t signature_length,
    TC_random_fn random, void* random_context, size_t max_attempts,
    const TC_RSA_workspace* workspace, size_t* work)
{
  size_t max_work = *work;
  tc_hash_info info;
  size_t validation_work = SIZE_MAX;
  TC_RSA_result status = tc_rsa_sign_inputs(key,digest,signature,signature_length,
      random,max_attempts,workspace);
  if (status != TC_RSA_OK) return status;
  if (crt) {
    status = tc_rsa_crt_inputs(key,crt,(TC_bytes){signature,signature_length},workspace);
    if (status != TC_RSA_OK) return status;
  }
  const size_t length = key->public_key.modulus.length, n = length / sizeof(TC_RSA_word);
  status = tc_rsa_pss_prepare(length,length * 8 - 1,hash,mgf_hash,digest.length,
      salt_length,&validation_work,&info);
  if (status != TC_RSA_OK) return status;
  if (!digest.data) return TC_RSA_ARGUMENT;
  if (max_work < SIZE_MAX - validation_work + (salt_length != 0)) return TC_RSA_LIMIT;
  tc_hash_workspace hash_workspace;
  uint8_t block[64];
  uint8_t* salt = (uint8_t*)workspace->words;
  uint8_t* encoded = (uint8_t*)(workspace->words + 13 * n);
  if (salt_length) {
    --max_work;
    if (random(random_context,salt,salt_length) != TC_OK) {
      status = TC_RSA_ERROR; goto cleanup;
    }
  }
  status = tc_rsa_pss_encode(encoded,length,length * 8 - 1,hash,mgf_hash,digest,
      (TC_bytes){salt,salt_length},block,&hash_workspace,&max_work);
  if (status == TC_RSA_OK)
    status = tc_rsa_sign_encoded(key,crt,signature,random,random_context,max_attempts,workspace,&max_work);
cleanup:
  TC_secure_zero(workspace->words,14 * n * sizeof(TC_RSA_word));
  TC_secure_zero(block,sizeof block);
  TC_secure_zero(&hash_workspace,sizeof hash_workspace);
  *work = max_work;
  return status;
}

TC_RSA_result TC_RSA_sign_pss_digest(const TC_RSA_private_key* key,
    const TC_RSA_pss_options* options, TC_bytes digest,
    const TC_RSA_workspace* workspace, TC_buffer signature,
    TC_RSA_execution* execution)
{
  if (!key || !options || !execution) return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_control_inputs(workspace,
      (TC_bytes){signature.data,signature.capacity},options,sizeof *options,
      execution,sizeof *execution);
  if (result != TC_RSA_OK) return result;
  size_t work = execution->work.remaining;
  result = tc_rsa_sign_pss_digest(key,key->crt,options->hash,
      options->mgf_hash,options->salt_length,digest,signature.data,
      signature.capacity,execution->random.fill,execution->random.context,
      execution->random_attempts,workspace,&work);
  execution->work.remaining = (uint32_t)work;
  return result;
}

static TC_RSA_result tc_rsa_encrypt_oaep(const TC_RSA_public_key* key,
    TC_hash_algorithm hash, TC_hash_algorithm mgf_hash, TC_bytes label,
    TC_bytes plaintext, uint8_t* ciphertext, size_t ciphertext_length,
    TC_random_fn random, void* random_context,
    const TC_RSA_workspace* workspace, size_t* work)
{
  size_t max_work = *work;
  if (!random || !ciphertext) return TC_RSA_ARGUMENT;
  TC_RSA_result status = tc_rsa_public_inputs(key,plaintext,label,
      (TC_bytes){ciphertext,ciphertext_length},workspace);
  if (status != TC_RSA_OK) return status;
  const size_t length = key->modulus.length;
  status = tc_rsa_public_key_check(key->modulus.data,length,key->exponent.data,key->exponent.length);
  if (status != TC_RSA_OK) return status;
  if (ciphertext_length != length) return TC_RSA_INVALID;
  tc_hash_info info;
  size_t validation_work = SIZE_MAX;
  status = tc_rsa_oaep_prepare(length,hash,mgf_hash,label,&validation_work,&info);
  if (status != TC_RSA_OK) return status;
  if (plaintext.length > length - 2 * info.digest_length - 2) return TC_RSA_INVALID;
  const size_t n = length / sizeof(TC_RSA_word), arithmetic_words = 8 * n + 2;
  const size_t required = arithmetic_words + n;
  if (workspace->capacity < required || max_work <= SIZE_MAX - validation_work)
    return TC_RSA_LIMIT;
  tc_hash_workspace hash_workspace;
  uint8_t block[64];
  /* Seed storage is reused by the modular operation after OAEP encoding. */
  uint8_t* seed = (uint8_t*)workspace->words;
  uint8_t* encoded = (uint8_t*)(workspace->words + arithmetic_words);
  --max_work;
  if (random(random_context,seed,info.digest_length) != TC_OK) status = TC_RSA_ERROR;
  else {
    status = tc_rsa_oaep_encode(encoded,length,hash,mgf_hash,label,plaintext,
        (TC_bytes){seed,info.digest_length},block,&hash_workspace,&max_work);
    if (status == TC_RSA_OK)
      status = tc_rsa_public_operation(key->modulus.data,length,key->exponent.data,
          key->exponent.length,encoded,ciphertext,workspace->words,arithmetic_words,&max_work);
  }
  TC_secure_zero(workspace->words,required * sizeof(TC_RSA_word));
  TC_secure_zero(block,sizeof block);
  TC_secure_zero(&hash_workspace,sizeof hash_workspace);
  *work = max_work;
  return status;
}

TC_RSA_result TC_RSA_encrypt_oaep(const TC_RSA_public_key* key,
    const TC_RSA_oaep_options* options, TC_bytes plaintext,
    const TC_RSA_workspace* workspace, TC_buffer ciphertext,
    TC_RSA_execution* execution)
{
  if (!options || !execution) return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_control_inputs(workspace,
      (TC_bytes){ciphertext.data,ciphertext.capacity},options,sizeof *options,
      execution,sizeof *execution);
  if (result != TC_RSA_OK) return result;
  size_t work = execution->work.remaining;
  result = tc_rsa_encrypt_oaep(key,options->hash,
      options->mgf_hash,options->label,plaintext,ciphertext.data,
      ciphertext.capacity,execution->random.fill,execution->random.context,
      workspace,&work);
  execution->work.remaining = (uint32_t)work;
  return result;
}

static TC_RSA_result tc_rsa_decrypt_oaep(const TC_RSA_private_key* key,
    const TC_RSA_crt* crt,
    TC_hash_algorithm hash, TC_hash_algorithm mgf_hash, TC_bytes label,
    TC_bytes ciphertext, uint8_t* plaintext, size_t plaintext_capacity,
    size_t* plaintext_length, TC_random_fn random, void* random_context,
    size_t max_attempts, const TC_RSA_workspace* workspace, size_t* work)
{
  size_t max_work = *work;
  tc_hash_info info;
  size_t validation_work = SIZE_MAX;
  if (!random) return TC_RSA_ARGUMENT;
  TC_RSA_result status = tc_rsa_decrypt_inputs(key,ciphertext,label,plaintext,
      plaintext_capacity,plaintext_length,workspace);
  if (status != TC_RSA_OK) return status;
  if (crt) {
    status = tc_rsa_crt_inputs(key,crt,(TC_bytes){plaintext,plaintext_capacity},workspace);
    if (status != TC_RSA_OK) return status;
  }
  const size_t length = key->public_key.modulus.length;
  status = tc_rsa_public_key_check(key->public_key.modulus.data,length,
      key->public_key.exponent.data,key->public_key.exponent.length);
  if (status != TC_RSA_OK) return status;
  if (ciphertext.length != length) return TC_RSA_INVALID;
  status = tc_rsa_oaep_prepare(length,hash,mgf_hash,label,&validation_work,&info);
  if (status != TC_RSA_OK) return status;
  const size_t n = length / sizeof(TC_RSA_word), required = 14 * n;
  if (!max_attempts || workspace->capacity < required || max_work < SIZE_MAX - validation_work)
    return TC_RSA_LIMIT;
  tc_hash_workspace hash_workspace;
  uint8_t block[64];
  uint8_t* encoded = (uint8_t*)(workspace->words + 13 * n);
  TC_bytes message = {0};
  if (crt) status = tc_rsa_crt_private_operation(key->public_key.modulus.data,length,
      key->public_key.exponent.data,key->public_key.exponent.length,key->p,key->q,crt,
      ciphertext.data,encoded,random,random_context,max_attempts,
      workspace->words,13 * n,&max_work);
  else status = tc_rsa_private_operation_magnitude(key->public_key.modulus.data,length,
      key->public_key.exponent.data,key->public_key.exponent.length,key->d,
      ciphertext.data,encoded,random,random_context,max_attempts,
      workspace->words,13 * n,&max_work);
  if (status == TC_RSA_OK)
    status = tc_rsa_oaep_decode(encoded,length,hash,mgf_hash,label,block,
        &hash_workspace,&max_work,&message);
  if (status == TC_RSA_OK) {
    if (message.length > plaintext_capacity) status = TC_RSA_LIMIT;
    else {
      if (message.length) memcpy(plaintext,message.data,message.length);
      *plaintext_length = message.length;
    }
  }
  TC_secure_zero(workspace->words,required * sizeof(TC_RSA_word));
  TC_secure_zero(block,sizeof block);
  TC_secure_zero(&hash_workspace,sizeof hash_workspace);
  *work = max_work;
  return status;
}

TC_RSA_result TC_RSA_decrypt_oaep(const TC_RSA_private_key* key,
    const TC_RSA_oaep_options* options, TC_bytes ciphertext,
    const TC_RSA_workspace* workspace, TC_buffer plaintext,
    size_t* plaintext_length, TC_RSA_execution* execution)
{
  if (!key || !options || !execution) return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_control_inputs(workspace,
      (TC_bytes){plaintext.data,plaintext.capacity},options,sizeof *options,
      execution,sizeof *execution);
  if (result != TC_RSA_OK) return result;
  if (!plaintext_length ||
      !tc_internal_ranges_disjoint(options,sizeof *options,plaintext_length,
        sizeof *plaintext_length) ||
      !tc_internal_ranges_disjoint(execution,sizeof *execution,plaintext_length,
        sizeof *plaintext_length)) return TC_RSA_ARGUMENT;
  size_t work = execution->work.remaining;
  result = tc_rsa_decrypt_oaep(key,key->crt,options->hash,
      options->mgf_hash,options->label,ciphertext,plaintext.data,
      plaintext.capacity,plaintext_length,execution->random.fill,
      execution->random.context,execution->random_attempts,workspace,&work);
  execution->work.remaining = (uint32_t)work;
  return result;
}

TC_RSA_result TC_RSA_verify_v15_digest(const TC_RSA_public_key* key,
    const TC_RSA_v15_options* options, TC_bytes digest, TC_bytes signature,
    const TC_RSA_workspace* workspace, TC_work_budget* work)
{
  if (!options || !work) return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_control_inputs(workspace,(TC_bytes){NULL,0},
      options,sizeof *options,work,sizeof *work);
  if (result != TC_RSA_OK) return result;
  result = tc_rsa_public_inputs(key,digest,signature,(TC_bytes){NULL,0},workspace);
  if (result != TC_RSA_OK) return result;
  size_t available = work->remaining;
  result = tc_rsa_verify_v15(key->modulus.data,key->modulus.length,
      key->exponent.data,key->exponent.length,signature.data,signature.length,
      options->hash,digest.data,digest.length,workspace->words,workspace->capacity,&available);
  work->remaining = (uint32_t)available;
  return result;
}

TC_RSA_result TC_RSA_verify_pss_digest(const TC_RSA_public_key* key,
    const TC_RSA_pss_options* options, TC_bytes digest, TC_bytes signature,
    const TC_RSA_workspace* workspace, TC_work_budget* work)
{
  tc_hash_workspace hash_workspace;
  uint8_t block[64];
  uint8_t* encoded;
  size_t length, words, needed;
  size_t validation_work = SIZE_MAX;
  tc_hash_info info;
  if (!options || !work) return TC_RSA_ARGUMENT;
  TC_RSA_result result = tc_rsa_control_inputs(workspace,(TC_bytes){NULL,0},
      options,sizeof *options,work,sizeof *work);
  if (result != TC_RSA_OK) return result;
  size_t max_work = work->remaining;
  result = tc_rsa_public_inputs(key,digest,signature,(TC_bytes){NULL,0},workspace);
  if (result != TC_RSA_OK) return result;
  length = key->modulus.length;
  if (length != 128 && length != 256 && length != 384) return TC_RSA_INVALID;
  if (signature.length != length) return TC_RSA_INVALID;
  if (!digest.data) return TC_RSA_ARGUMENT;
  result = tc_rsa_pss_prepare(length,length * 8 - 1,options->hash,
      options->mgf_hash,digest.length,options->salt_length,&validation_work,&info);
  if (result != TC_RSA_OK) return result;
  words = length / sizeof(TC_RSA_word);
  needed = 9 * words + 2;
  if (workspace->capacity < needed) return TC_RSA_LIMIT;
  encoded = (uint8_t*)(workspace->words + 8 * words + 2);
  result = tc_rsa_public_operation(key->modulus.data,length,key->exponent.data,
      key->exponent.length,signature.data,encoded,workspace->words,8 * words + 2,&max_work);
  if (result == TC_RSA_OK)
    result = tc_rsa_pss_check(encoded,length,length * 8 - 1,options->hash,
        options->mgf_hash,digest,options->salt_length,block,&hash_workspace,&max_work);
  TC_secure_zero(workspace->words,needed * sizeof(TC_RSA_word));
  TC_secure_zero(block,sizeof block);
  TC_secure_zero(&hash_workspace,sizeof hash_workspace);
  work->remaining = (uint32_t)max_work;
  return result;
}
#endif

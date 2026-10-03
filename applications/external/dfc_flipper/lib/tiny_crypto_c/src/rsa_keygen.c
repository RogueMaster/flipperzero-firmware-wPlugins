/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Incremental RSA key generation (FIPS 186-5 appendices A.1.1 and A.1.3). */
#include <tiny_crypto/rsa.h>
#if TC_ENABLE_RSA
#define TC_MP_WORD_BITS TC_RSA_WORD_BITS
#include "rsa_private_internal.h"

enum {
  TC_RSA_KEYGEN_EMPTY = 0,
  TC_RSA_KEYGEN_P_NEW,
  TC_RSA_KEYGEN_P_PREPARE,
  TC_RSA_KEYGEN_P_ROUND,
  TC_RSA_KEYGEN_Q_NEW,
  TC_RSA_KEYGEN_Q_PREPARE,
  TC_RSA_KEYGEN_Q_ROUND,
  TC_RSA_KEYGEN_DERIVE
};

/* Cheap public-candidate filtering avoids almost all expensive strong tests.
 * The residues use constant-time reduction because an accepted candidate
 * becomes a secret prime. A rejected candidate is discarded. */
static int tc_rsa_keygen_candidate_filter(const uint8_t* candidate, size_t length)
{
  static const uint8_t primes[] = {
      3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,  47,  53,  59,  61,  67,
      71,  73,  79,  83,  89,  97,  101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157,
      163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239, 241, 251};
  for (size_t i = 0; i < sizeof primes; ++i)
    if (tc_mp_mod_u32_be(candidate, length, primes[i]) == 0)
      return 0;
  /* FIPS 186-5 A.1.1 2(a): p - 1 is coprime to the prime e, so p mod e != 1. */
  return tc_mp_mod_u32_be(candidate, length, TC_RSA_KEYGEN_PUBLIC_EXPONENT) != 1;
}

/* value mod divisor for a public 1 < divisor < 2^31, in time independent of
 * value. */
static uint32_t tc_rsa_keygen_mod_u64(uint64_t value, uint32_t divisor)
{
  uint32_t remainder = 0;
  for (unsigned bit = 64; bit; --bit)
    /* Only the remainder is needed. The quotient bit is discarded. */
    (void)tc_mp_mod_u32_step(&remainder, (unsigned)(value >> (bit - 1)), divisor);
  return remainder;
}

/* value^-1 mod 65537 for 0 < value < 65537. 65537 is prime, so a fixed
 * square-and-multiply chain computes value^(65537-2). */
static uint32_t tc_rsa_keygen_inverse_65537(uint32_t value)
{
  uint32_t result = 1;
  for (unsigned bit = 0; bit < 16; ++bit) {
    result = tc_rsa_keygen_mod_u64((uint64_t)result * result, TC_RSA_KEYGEN_PUBLIC_EXPONENT);
    result = tc_rsa_keygen_mod_u64((uint64_t)result * value, TC_RSA_KEYGEN_PUBLIC_EXPONENT);
  }
  return result;
}

/* Derive n and d = e^-1 mod LCM(p-1, q-1) from retained prime bytes
 * (FIPS 186-5 A.1.1 3). scratch has 6n+2 limbs, n = 2 * prime_length / limb
 * size. Every step runs in time that depends only on the key size. Returns 0
 * when d <= 2^(nlen/2); the caller then generates new primes. Results remain
 * in scratch until publication. */
static int tc_rsa_keygen_derive(const uint8_t* p_bytes, const uint8_t* q_bytes, size_t prime_length,
                                tc_mp_word* scratch, tc_mp_word** modulus_out, tc_mp_word** d_out)
{
  const size_t h = prime_length / sizeof(tc_mp_word), n = 2 * h;
  const size_t extra = (16 + TC_MP_WORD_BITS - 1) / TC_MP_WORD_BITS;
  tc_mp_word* p = scratch;            /* h limbs */
  tc_mp_word* q = p + h;              /* h limbs */
  tc_mp_word* modulus = q + h;        /* n limbs: phi, then n */
  tc_mp_word* gcd = modulus + n;      /* n limbs */
  tc_mp_word* remainder = gcd + n;    /* n limbs, gcd scratch */
  tc_mp_word* lambda = remainder + n; /* n limbs, gcd scratch */
  tc_mp_word* d = lambda + n;         /* n + extra limbs */
  tc_mp_from_be(p, p_bytes, prime_length);
  tc_mp_from_be(q, q_bytes, prime_length);
  --p[0];
  --q[0];                           /* Both primes are odd, so the decrement needs no borrow. */
  tc_mp_multiply(modulus, p, q, h); /* phi = (p-1)(q-1) */
  memset(gcd, 0, n * sizeof *gcd);
  tc_mp_gcd(gcd, p, q, h, remainder);
  /* LCM(p-1, q-1) = phi / gcd(p-1, q-1). The division is exact. */
  tc_mp_divide_words(lambda, remainder, modulus, n, gcd, n, d);
  ++p[0];
  ++q[0];
  tc_mp_multiply(modulus, p, q, h);
  /* e d = 1 + lambda k with k = -lambda^-1 mod e, so d = (lambda k + 1) / e. */
  const uint32_t residue = tc_mp_divide_u32(NULL, lambda, n, TC_RSA_KEYGEN_PUBLIC_EXPONENT);
  const uint32_t multiplier = TC_RSA_KEYGEN_PUBLIC_EXPONENT - tc_rsa_keygen_inverse_65537(residue);
  uint64_t carry = 1;
  for (size_t i = 0; i < n; ++i) {
    carry += (uint64_t)lambda[i] * multiplier;
    d[i] = (tc_mp_word)carry;
    carry >>= TC_MP_WORD_BITS;
  }
  for (size_t i = 0; i < extra; ++i) {
    d[n + i] = (tc_mp_word)carry;
    carry >>= TC_MP_WORD_BITS;
  }
  tc_mp_divide_u32(d, d, n + extra, TC_RSA_KEYGEN_PUBLIC_EXPONENT);
  /* d > 2^(nlen/2) exactly when d - (2^(nlen/2) + 1) does not borrow. */
  memset(gcd, 0, n * sizeof *gcd);
  gcd[0] = 1;
  gcd[h] = 1;
  const tc_mp_word small = tc_mp_subtract(remainder, d, gcd, n);
  *modulus_out = modulus;
  *d_out = d;
  return small == 0;
}

#define TC_RSA_KEYGEN_MARKER UINT32_C(0x524b4731)

/* Argument check: present, aligned and describable storage, with the state,
 * output metadata, workspace and output buffers pairwise disjoint. Sizes are
 * checked separately so a short buffer returns LIMIT. */
static int tc_rsa_keygen_storage_check(const TC_RSA_keygen_state* state,
                                       const TC_RSA_keygen_output* output,
                                       const TC_RSA_workspace* workspace)
{
  const TC_buffer buffers[] = {output->modulus, output->exponent, output->d, output->p, output->q};
  if (!workspace->words || (uintptr_t)workspace->words % sizeof(TC_RSA_word) ||
      workspace->capacity > SIZE_MAX / sizeof *workspace->words)
    return 0;
  for (size_t i = 0; i < sizeof buffers / sizeof *buffers; ++i) {
    if (!buffers[i].data)
      return 0;
    if (!tc_internal_ranges_disjoint(buffers[i].data, buffers[i].capacity, state, sizeof *state) ||
        !tc_internal_ranges_disjoint(buffers[i].data, buffers[i].capacity, output,
                                     sizeof *output) ||
        !tc_internal_ranges_disjoint(buffers[i].data, buffers[i].capacity, workspace,
                                     sizeof *workspace) ||
        !tc_internal_ranges_disjoint(buffers[i].data, buffers[i].capacity, workspace->words,
                                     workspace->capacity * sizeof *workspace->words))
      return 0;
    for (size_t j = 0; j < i; ++j)
      if (!tc_internal_ranges_disjoint(buffers[i].data, buffers[i].capacity, buffers[j].data,
                                       buffers[j].capacity))
        return 0;
  }
  const size_t words_size = workspace->capacity * sizeof *workspace->words;
  return tc_internal_ranges_disjoint(state, sizeof *state, workspace, sizeof *workspace) &&
         tc_internal_ranges_disjoint(state, sizeof *state, workspace->words, words_size) &&
         tc_internal_ranges_disjoint(state, sizeof *state, output, sizeof *output) &&
         tc_internal_ranges_disjoint(output, sizeof *output, workspace, sizeof *workspace) &&
         tc_internal_ranges_disjoint(output, sizeof *output, workspace->words, words_size) &&
         tc_internal_ranges_disjoint(workspace, sizeof *workspace, workspace->words, words_size);
}

TC_RSA_result TC_RSA_keygen_init(TC_RSA_keygen_state* state, size_t bits,
                                 const TC_RSA_keygen_output* output, TC_RSA_keygen_limits limits,
                                 const TC_RSA_workspace* workspace)
{
  if (!state || !output || !workspace)
    return TC_RSA_ARGUMENT;
  if (state->marker == TC_RSA_KEYGEN_MARKER ||
      !tc_rsa_keygen_storage_check(state, output, workspace))
    return TC_RSA_ARGUMENT;
  if (!TC_RSA_workspace_words(TC_RSA_OPERATION_KEYGEN, bits))
    return TC_RSA_UNSUPPORTED;
  if (!limits.candidate_attempts || !limits.random_requests)
    return TC_RSA_LIMIT;
  const size_t length = bits / 8, prime_length = length / 2;
  if (workspace->capacity < TC_RSA_KEYGEN_WORKSPACE_WORDS(bits) ||
      output->modulus.capacity < length || output->exponent.capacity < 3 ||
      output->d.capacity < length || output->p.capacity < prime_length ||
      output->q.capacity < prime_length)
    return TC_RSA_LIMIT;
  TC_RSA_keygen_state initialized;
  memset(&initialized, 0, sizeof initialized);
  initialized.workspace = *workspace;
  initialized.output = *output;
  initialized.marker = TC_RSA_KEYGEN_MARKER;
  initialized.bits = (uint32_t)bits;
  initialized.candidate_limit = limits.candidate_attempts;
  initialized.random_limit = limits.random_requests;
  initialized.phase = TC_RSA_KEYGEN_P_NEW;
  TC_secure_zero(workspace->words, TC_RSA_KEYGEN_WORKSPACE_WORDS(bits) * sizeof *workspace->words);
  *state = initialized;
  return TC_RSA_OK;
}

/* The state's key size. TC_RSA_keygen_init stores a supported size, which
 * fits size_t. A larger value maps to 0, which no operation supports. */
static size_t tc_rsa_keygen_bits(const TC_RSA_keygen_state* state)
{
#if SIZE_MAX < UINT32_MAX
  if (state->bits > SIZE_MAX)
    return 0;
#endif
  return (size_t)state->bits;
}

void TC_RSA_keygen_clear(TC_RSA_keygen_state* state)
{
  if (!state)
    return;
  const size_t bits = tc_rsa_keygen_bits(state);
  if (state->marker == TC_RSA_KEYGEN_MARKER &&
      TC_RSA_workspace_words(TC_RSA_OPERATION_KEYGEN, bits) && state->workspace.words &&
      state->workspace.capacity >= TC_RSA_KEYGEN_WORKSPACE_WORDS(bits))
    TC_secure_zero(state->workspace.words,
                   TC_RSA_KEYGEN_WORKSPACE_WORDS(bits) * sizeof *state->workspace.words);
  TC_secure_zero(state, sizeof *state);
}

static TC_RSA_result tc_rsa_keygen_stop(TC_RSA_keygen_state* state, TC_RSA_result result)
{
  TC_RSA_keygen_clear(state);
  return result;
}

static TC_RSA_result tc_rsa_keygen_random(TC_RSA_keygen_state* state, TC_random_fn random,
                                          void* context, uint8_t* output, size_t length)
{
  if (state->random_requests == state->random_limit)
    return TC_RSA_LIMIT;
  ++state->random_requests;
  return random(context, output, length) == TC_OK ? TC_RSA_OK : TC_RSA_ERROR;
}

static TC_RSA_result tc_rsa_keygen_step(TC_RSA_keygen_state* state, TC_random_fn random,
                                        void* random_context, TC_RSA_cancel_fn cancel,
                                        void* cancel_context, uint32_t* work)
{
  if (!work)
    return TC_RSA_ARGUMENT;
  uint32_t max_work = *work;
#define TC_RSA_KEYGEN_RETURN(value)                                                                \
  do {                                                                                             \
    TC_RSA_result tc_rsa_keygen_result = (value);                                                  \
    *work = max_work;                                                                              \
    return tc_rsa_keygen_result;                                                                   \
  } while (0)
  if (!state || !random || state->marker != TC_RSA_KEYGEN_MARKER)
    TC_RSA_KEYGEN_RETURN(TC_RSA_ARGUMENT);
  const size_t bits = tc_rsa_keygen_bits(state);
  if (!TC_RSA_workspace_words(TC_RSA_OPERATION_KEYGEN, bits))
    TC_RSA_KEYGEN_RETURN(TC_RSA_ARGUMENT);
  if (state->phase < TC_RSA_KEYGEN_P_NEW || state->phase > TC_RSA_KEYGEN_DERIVE)
    TC_RSA_KEYGEN_RETURN(tc_rsa_keygen_stop(state, TC_RSA_ARGUMENT));
  const size_t length = bits / 8, prime_length = length / 2;
  const size_t n = length / sizeof(TC_RSA_word), h = n / 2;
  const size_t required = TC_RSA_KEYGEN_WORKSPACE_WORDS(bits);
  if (!state->workspace.words || state->workspace.capacity < required)
    TC_RSA_KEYGEN_RETURN(tc_rsa_keygen_stop(state, TC_RSA_ARGUMENT));
  uint8_t* p = (uint8_t*)state->workspace.words;
  uint8_t* q = p + prime_length;
  TC_RSA_word* scratch = state->workspace.words + n;
  const uint32_t candidate_work = (uint32_t)prime_length + 54;
  const uint32_t setup_work = UINT32_C(24) * (uint32_t)prime_length + 3;
  const uint32_t round_work = UINT32_C(24) * (uint32_t)prime_length + 2;
  for (;;) {
    if (cancel && cancel(cancel_context))
      TC_RSA_KEYGEN_RETURN(tc_rsa_keygen_stop(state, TC_RSA_CANCELLED));
    if (state->phase == TC_RSA_KEYGEN_P_NEW || state->phase == TC_RSA_KEYGEN_Q_NEW) {
      if (state->candidates == state->candidate_limit)
        TC_RSA_KEYGEN_RETURN(tc_rsa_keygen_stop(state, TC_RSA_LIMIT));
      if (max_work < candidate_work)
        TC_RSA_KEYGEN_RETURN(TC_RSA_IN_PROGRESS);
      max_work -= candidate_work;
      ++state->candidates;
      uint8_t* candidate = state->phase == TC_RSA_KEYGEN_P_NEW ? p : q;
      TC_RSA_result status =
          tc_rsa_keygen_random(state, random, random_context, candidate, prime_length);
      if (status != TC_RSA_OK)
        TC_RSA_KEYGEN_RETURN(tc_rsa_keygen_stop(state, status));
      candidate[0] |= 0xc0u;
      candidate[prime_length - 1] |= 1u;
      if (!tc_rsa_keygen_candidate_filter(candidate, prime_length))
        continue;
      if (state->phase == TC_RSA_KEYGEN_Q_NEW) {
        tc_mp_from_be(scratch, p, prime_length);
        tc_mp_from_be(scratch + h, q, prime_length);
        const int far_apart = tc_rsa_factors_far_apart(scratch, scratch + h, h, scratch + 2 * h);
        TC_secure_zero(scratch, 5 * h * sizeof *scratch);
        if (!far_apart)
          continue;
      }
      state->phase =
          state->phase == TC_RSA_KEYGEN_P_NEW ? TC_RSA_KEYGEN_P_PREPARE : TC_RSA_KEYGEN_Q_PREPARE;
    }
    if (state->phase == TC_RSA_KEYGEN_P_PREPARE || state->phase == TC_RSA_KEYGEN_Q_PREPARE) {
      if (max_work < setup_work)
        TC_RSA_KEYGEN_RETURN(TC_RSA_IN_PROGRESS);
      max_work -= setup_work;
      const uint8_t* candidate = state->phase == TC_RSA_KEYGEN_P_PREPARE ? p : q;
      tc_mp_from_be(scratch, candidate, prime_length);
      state->twos = (uint16_t)tc_mp_miller_rabin_prepare(scratch, h, scratch + 2 * h);
      state->rounds = 0;
      state->phase =
          state->phase == TC_RSA_KEYGEN_P_PREPARE ? TC_RSA_KEYGEN_P_ROUND : TC_RSA_KEYGEN_Q_ROUND;
    }
    if (state->phase == TC_RSA_KEYGEN_P_ROUND || state->phase == TC_RSA_KEYGEN_Q_ROUND) {
      if (max_work < round_work)
        TC_RSA_KEYGEN_RETURN(TC_RSA_IN_PROGRESS);
      max_work -= round_work;
      TC_RSA_word* base = scratch + h;
      TC_RSA_word* temporary = scratch + 8 * h;
      TC_RSA_result status =
          tc_rsa_keygen_random(state, random, random_context, (uint8_t*)temporary, prime_length);
      if (status != TC_RSA_OK)
        TC_RSA_KEYGEN_RETURN(tc_rsa_keygen_stop(state, status));
      const uint8_t* candidate_bytes = state->phase == TC_RSA_KEYGEN_P_ROUND ? p : q;
      if (!tc_rsa_witness_sample(base, (uint8_t*)temporary,
                                 (TC_bytes){candidate_bytes, prime_length}, prime_length, scratch,
                                 h, scratch + 7 * h))
        continue;
      if (!tc_mp_miller_rabin_round(scratch, base, h, state->twos, scratch + 2 * h)) {
        state->phase =
            state->phase == TC_RSA_KEYGEN_P_ROUND ? TC_RSA_KEYGEN_P_NEW : TC_RSA_KEYGEN_Q_NEW;
        state->rounds = 0;
        TC_secure_zero(scratch, (6 * n + 2) * sizeof *scratch);
        continue;
      }
      if (++state->rounds != TC_RSA_VALIDATION_ROUNDS)
        continue;
      if (state->phase == TC_RSA_KEYGEN_P_ROUND) {
        state->phase = TC_RSA_KEYGEN_Q_NEW;
        state->rounds = 0;
        TC_secure_zero(scratch, (6 * n + 2) * sizeof *scratch);
        continue;
      }
      state->phase = TC_RSA_KEYGEN_DERIVE;
    }
    if (state->phase == TC_RSA_KEYGEN_DERIVE) {
      /* Deriving n and d costs one setup step of work. */
      if (max_work < setup_work)
        TC_RSA_KEYGEN_RETURN(TC_RSA_IN_PROGRESS);
      max_work -= setup_work;
      TC_RSA_word *modulus_words, *d_words;
      const int d_large =
          tc_rsa_keygen_derive(p, q, prime_length, scratch, &modulus_words, &d_words);
      if (!d_large) {
        /* FIPS 186-5 A.1.1 3: d <= 2^(nlen/2) requires new primes. */
        state->phase = TC_RSA_KEYGEN_P_NEW;
        state->rounds = 0;
        TC_secure_zero(scratch, (6 * n + 2) * sizeof *scratch);
        continue;
      }
      tc_mp_to_be(state->output.modulus.data, modulus_words, length);
      state->output.exponent.data[0] = 1;
      state->output.exponent.data[1] = 0;
      state->output.exponent.data[2] = 1;
      tc_mp_to_be(state->output.d.data, d_words, length);
      memcpy(state->output.p.data, p, prime_length);
      memcpy(state->output.q.data, q, prime_length);
      TC_RSA_keygen_clear(state);
      TC_RSA_KEYGEN_RETURN(TC_RSA_OK);
    }
  }
#undef TC_RSA_KEYGEN_RETURN
}

TC_RSA_result TC_RSA_keygen_step(TC_RSA_keygen_state* state, TC_random_source random,
                                 TC_RSA_cancel_fn cancel, void* cancel_context,
                                 TC_work_budget* work)
{
  if (!work)
    return TC_RSA_ARGUMENT;
  uint32_t available = work->remaining;
  TC_RSA_result result =
      tc_rsa_keygen_step(state, random.fill, random.context, cancel, cancel_context, &available);
  work->remaining = available;
  return result;
}
#endif

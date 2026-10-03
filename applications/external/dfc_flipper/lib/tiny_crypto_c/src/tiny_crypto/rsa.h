/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* RSA public and private-key operations: PKCS #1 v1.5 and PSS signatures,
 * OAEP, raw operations, key validation, CRT derivation and stepwise key
 * generation.
 * Standards: RFC 8017, FIPS 186-5 appendices A.1 and C.
 * Configuration: TC_ENABLE_RSA, TC_RSA_ENABLE_1024/2048/3072/4096 and
 * TC_RSA_SMALL. RSA-1024 is a legacy size and is disabled by default.
 * Limitations: two-prime keys of 1024, 2048, 3072 or 4096 bits.
 * Contracts: docs/api.md, including its TC_work_budget units.
 * Guide: docs/rsa.md. */
#ifndef TINY_CRYPTO_RSA_H_
#define TINY_CRYPTO_RSA_H_
#include <tiny_crypto/common.h>
#if TC_RSA_SMALL || defined(__AVR__)
typedef uint8_t TC_RSA_word;
#define TC_RSA_WORD_BITS 8
#else
typedef uint32_t TC_RSA_word;
#define TC_RSA_WORD_BITS 32
#endif

/* Individually enabled moduli are 1024, 2048, 3072 and 4096 bits. Size caller
 * storage that holds a modulus-length value, such as an encoded message or a
 * signature, from TC_RSA_MAX_MODULUS_BYTES. */
#define TC_RSA_MAX_MODULUS_BITS 4096u
#define TC_RSA_MAX_MODULUS_BYTES (TC_RSA_MAX_MODULUS_BITS / 8u)

/* Results. Every RSA function checks its arguments once, in this order, and
 * reports the first problem it finds:
 *
 *   TC_RSA_ARGUMENT     NULL pointers, overlapping or misaligned storage, and
 *                       a digest whose length differs from its known hash.
 *   TC_RSA_INVALID      a malformed key or CRT value, out-of-range scheme
 *                       parameters, then received data: a signature,
 *                       ciphertext or raw input of the wrong length.
 *   TC_RSA_LIMIT        a caller output buffer shorter than required, then
 *                       too little workspace, RNG attempts or work.
 *   TC_RSA_UNSUPPORTED  a modulus size, hash or option the build does not
 *                       implement.
 *
 * The arithmetic finds a representative at or above the modulus and returns
 * TC_RSA_INVALID after the limit checks pass. Output buffers larger than
 * required are accepted, and exactly the documented length is written.
 * UNSUPPORTED and LIMIT never report success. Argument errors and limits
 * found before arithmetic leave every output, the workspace and the work
 * budget unchanged. */
typedef TC_result TC_RSA_result;
#define TC_RSA_OK TC_RESULT_OK
#define TC_RSA_INVALID TC_RESULT_INVALID
#define TC_RSA_LIMIT TC_RESULT_LIMIT
#define TC_RSA_ARGUMENT TC_RESULT_ARGUMENT
#define TC_RSA_UNSUPPORTED TC_RESULT_UNSUPPORTED
#define TC_RSA_ERROR TC_RESULT_ERROR
#define TC_RSA_IN_PROGRESS TC_RESULT_IN_PROGRESS
#define TC_RSA_CANCELLED TC_RESULT_CANCELLED
typedef struct {
  TC_bytes modulus, exponent;
} TC_RSA_public_key;
typedef struct {
  TC_bytes dp, dq, q_inverse;
} TC_RSA_crt;
typedef struct {
  TC_RSA_public_key public_key;
  TC_bytes d, p, q;
  /* Optional validated CRT values. NULL selects full-width exponentiation. */
  const TC_RSA_crt* crt;
} TC_RSA_private_key;
typedef struct {
  TC_buffer dp, dq, q_inverse;
} TC_RSA_crt_output;
typedef struct {
  TC_RSA_word* words;
  size_t capacity;
} TC_RSA_workspace;
/* Optional setup for repeated verification with an unchanged borrowed key.
 * Initialize with TC_RSA_prepare_public_key and clear before releasing the key. */
typedef struct {
  TC_RSA_public_key key;
  TC_RSA_workspace r2;
  uint32_t marker;
} TC_RSA_prepared_public_key;
typedef int (*TC_RSA_cancel_fn)(void* user);
typedef struct {
  TC_buffer modulus, exponent, d, p, q;
} TC_RSA_keygen_output;
typedef struct {
  uint32_t candidate_attempts;
  uint32_t random_requests;
} TC_RSA_keygen_limits;
typedef TC_execution TC_RSA_execution;
typedef struct {
  TC_hash_algorithm hash;
} TC_RSA_v15_options;
typedef struct {
  TC_hash_algorithm hash, mgf_hash;
  size_t salt_length;
} TC_RSA_pss_options;
typedef struct {
  TC_hash_algorithm hash, mgf_hash;
  TC_bytes label;
} TC_RSA_oaep_options;
/* Public exponent range accepted by private-key validation. The zero value
 * applies FIPS 186-5. ANY_ODD admits keys outside FIPS 186-5, such as test
 * vectors with e = 3. */
typedef enum { TC_RSA_EXPONENT_FIPS = 0, TC_RSA_EXPONENT_ANY_ODD } TC_RSA_exponent_policy;

/* Caller-owned resumable state. Zero-initialize before the first init and
 * access it only through the key-generation functions below. */
typedef struct {
  TC_RSA_workspace workspace;
  TC_RSA_keygen_output output;
  uint32_t marker, bits, candidate_limit, random_limit;
  uint32_t candidates, random_requests;
  uint16_t phase, rounds, twos;
  uint16_t reserved;
} TC_RSA_keygen_state;

/* Workspace limbs per operation for a supported, constant key size in bits,
 * for static arrays. TC_RSA_workspace_words gives the same values at run
 * time. Verification uses nine limb arrays and two carry words. */
#define TC_RSA_VERIFY_WORKSPACE_WORDS(bits) (9u * ((bits) / TC_RSA_WORD_BITS) + 2u)
#define TC_RSA_VALIDATE_WORKSPACE_WORDS(bits) (12u * ((bits) / TC_RSA_WORD_BITS) + 2u)
#define TC_RSA_CRT_WORKSPACE_WORDS(bits) (8u * ((bits) / TC_RSA_WORD_BITS))
#define TC_RSA_SIGN_WORKSPACE_WORDS(bits) (14u * ((bits) / TC_RSA_WORD_BITS))
#define TC_RSA_DECRYPT_WORKSPACE_WORDS(bits) (14u * ((bits) / TC_RSA_WORD_BITS))
#define TC_RSA_ENCRYPT_WORKSPACE_WORDS(bits) TC_RSA_VERIFY_WORKSPACE_WORDS(bits)
#define TC_RSA_RAW_PUBLIC_WORKSPACE_WORDS(bits) (8u * ((bits) / TC_RSA_WORD_BITS) + 2u)
#define TC_RSA_RAW_PRIVATE_WORKSPACE_WORDS(bits) (13u * ((bits) / TC_RSA_WORD_BITS))
#define TC_RSA_VALIDATION_ROUNDS 65u
/* Work that TC_RSA_validate_private_key can consume for a bits-bit key with
 * attempts RNG requests allowed per factor: the component checks, then one
 * Miller-Rabin setup, TC_RSA_VALIDATION_ROUNDS rounds and every request for
 * each half-width factor. */
#define TC_RSA_VALIDATE_WORK(bits, attempts)                                                       \
  (48u * ((bits) / 8u) + 2u +                                                                      \
   2u * ((24u * ((bits) / 16u) + 3u) + TC_RSA_VALIDATION_ROUNDS * (24u * ((bits) / 16u) + 1u) +    \
         (attempts)))
#define TC_RSA_KEYGEN_PUBLIC_EXPONENT 65537u
#define TC_RSA_KEYGEN_WORKSPACE_WORDS(bits) (7u * ((bits) / TC_RSA_WORD_BITS) + 2u)
#define TC_RSA_KEYGEN_STEP_WORK(bits) (24u * ((bits) / 16u) + 3u)

#ifdef __cplusplus
extern "C" {
#endif
/* Operations with distinct workspace requirements. */
typedef enum {
  TC_RSA_OPERATION_VERIFY,      /* PKCS #1 v1.5 and PSS verification */
  TC_RSA_OPERATION_ENCRYPT,     /* OAEP encryption */
  TC_RSA_OPERATION_RAW_PUBLIC,  /* TC_RSA_raw_public */
  TC_RSA_OPERATION_VALIDATE,    /* TC_RSA_validate_private_key */
  TC_RSA_OPERATION_CRT,         /* TC_RSA_validate_crt and TC_RSA_derive_crt */
  TC_RSA_OPERATION_SIGN,        /* PKCS #1 v1.5 and PSS signing */
  TC_RSA_OPERATION_DECRYPT,     /* OAEP decryption */
  TC_RSA_OPERATION_RAW_PRIVATE, /* TC_RSA_raw_private */
  TC_RSA_OPERATION_KEYGEN       /* TC_RSA_keygen_init */
} TC_RSA_operation;
/* Shared conventions for the functions below. modulus_bytes is k, the key's
 * modulus length. Keys use unsigned, minimal big-endian magnitudes without a
 * DER sign octet. A modulus other than 1024, 2048, 3072 or 4096 bits returns
 * UNSUPPORTED. An even modulus, a modulus without its top bit, or an
 * exponent that is even, below 3 or at least the modulus returns INVALID.
 * Workspaces are aligned TC_RSA_word arrays sized with TC_RSA_workspace_words
 * or the *_WORKSPACE_WORDS macros, exclusive to one call and disjoint from
 * every input, output and metadata object. Used scratch is wiped before
 * return. Checked overlaps return ARGUMENT before any write. Digests passed
 * with a known hash must have its length, otherwise the result is ARGUMENT.
 * Outputs change only on TC_RSA_OK, except where a function says otherwise.
 * Private-key operations blind the input, need an RNG in execution that
 * fills each request with unpredictable bytes and keeps its context disjoint
 * from the other arguments, and verify the result with the public exponent
 * before release. A failed RNG request or a failed release check returns
 * TC_RSA_ERROR. */

#if TC_ENABLE_RSA
/* Workspace limbs for operation at a key size in bits. Zero for an
 * unsupported key size or an unknown operation. Charges no work. */
size_t TC_RSA_workspace_words(TC_RSA_operation operation, size_t bits);

/* Prepare a borrowed public key for repeated v1.5 or PSS verification by
 * caching R^2 mod n. cache needs k / sizeof(TC_RSA_word) limbs and scratch
 * twice that. Scratch is wiped on return. Keep the cache and the borrowed key
 * bytes unchanged and alive until TC_RSA_prepared_public_key_clear. setup,
 * key, cache, scratch and work must be disjoint.
 *
 * TC_RSA_ARGUMENT     NULL or misaligned storage, or overlap.
 * TC_RSA_UNSUPPORTED  unsupported modulus size.
 * TC_RSA_INVALID      malformed key.
 * TC_RSA_LIMIT        cache, scratch or work below the required size.
 *
 * setup changes only on OK. Work: 16*k + 1. */
TC_RSA_result TC_RSA_prepare_public_key(TC_RSA_prepared_public_key* setup,
                                        const TC_RSA_public_key* key, const TC_RSA_workspace* cache,
                                        const TC_RSA_workspace* workspace, TC_work_budget* work);
/* Wipe the setup. The caller-owned R^2 cache contains only public data and is
 * left untouched. Accepts NULL and an uninitialized setup. Charges no work. */
void TC_RSA_prepared_public_key_clear(TC_RSA_prepared_public_key* setup);

/* Generate a two-prime RSA key with e = 65537 and d = e^-1 mod LCM(p-1, q-1)
 * under FIPS 186-5 appendices A.1.1 and A.1.3, with probable primes from 65
 * Miller-Rabin rounds each (appendix B.3.1). Output capacities must be at
 * least bits/8 for modulus and d, bits/16 for p and q, and three bytes for
 * e. Scratch needs TC_RSA_KEYGEN_WORKSPACE_WORDS(bits) limbs. Output buffers
 * remain unchanged until a complete key is published. Scratch, state,
 * outputs, their metadata and the workspace descriptor must be mutually
 * disjoint. Zero-initialize state before the first init.
 *
 * init:
 * TC_RSA_ARGUMENT     NULL pointer or buffer, misaligned scratch, overlap or
 *                     an active state.
 * TC_RSA_UNSUPPORTED  another key size.
 * TC_RSA_LIMIT        a zero limit, or a buffer or scratch shorter than
 *                     required.
 * Failures leave state, outputs and scratch unchanged. Charges no work.
 *
 * step performs at most work->remaining units and returns
 * TC_RSA_IN_PROGRESS when more work is needed, with work reduced by the
 * units completed. TC_RSA_KEYGEN_STEP_WORK(bits) lets every pending unit
 * make progress. limits.candidate_attempts bounds prime candidates and
 * limits.random_requests every RNG request across all steps. The RNG must
 * fill each request completely. Callback contexts must be separate from
 * state, scratch and outputs.
 *
 * TC_RSA_OK           the key is published to the outputs and state is
 *                     cleared.
 * TC_RSA_IN_PROGRESS  call step again.
 * TC_RSA_ARGUMENT     NULL work, state or random.fill, or a state without an
 *                     active generation. A corrupted state is also cleared.
 * TC_RSA_LIMIT        candidate or RNG limit exhausted.
 * TC_RSA_CANCELLED    cancel returned nonzero.
 * TC_RSA_ERROR        an RNG request failed.
 *
 * LIMIT, CANCELLED and ERROR clear state and wipe retained candidates.
 * clear wipes state and its scratch. It accepts NULL. Call it whenever
 * abandoning an in-progress generation. */
TC_RSA_result TC_RSA_keygen_init(TC_RSA_keygen_state* state, size_t bits,
                                 const TC_RSA_keygen_output* output, TC_RSA_keygen_limits limits,
                                 const TC_RSA_workspace* workspace);
TC_RSA_result TC_RSA_keygen_step(TC_RSA_keygen_state* state, TC_random_source random,
                                 TC_RSA_cancel_fn cancel, void* cancel_context,
                                 TC_work_budget* work);
void TC_RSA_keygen_clear(TC_RSA_keygen_state* state);

/* Apply RSAEP/RSAVP1 (RFC 8017 section 5.1.1, 5.2.2) or RSADP/RSASP1
 * (sections 5.1.2, 5.2.1) to one already formatted, fixed-width
 * representative. No padding, hashing or encoding is provided. Callers select
 * and validate their protocol's encoding. input has length k and must be
 * less than the modulus. output.capacity is at least k, and exactly k bytes
 * are written, only on TC_RSA_OK.
 *
 * TC_RSA_ARGUMENT     NULL or misaligned storage, NULL random.fill for the
 *                     private operation, or overlap.
 * TC_RSA_UNSUPPORTED  unsupported modulus size.
 * TC_RSA_INVALID      malformed key, input of another length, a private
 *                     exponent empty or longer than k, or, after the LIMIT
 *                     checks, an input at or above the modulus.
 * TC_RSA_LIMIT        short output, scratch or work, and for the private
 *                     operation zero random_attempts or every blinding
 *                     attempt rejected.
 * TC_RSA_ERROR        private operation only: RNG or release-check failure.
 *
 * The private operation takes a private exponent of 1 to k bytes and runs
 * full width. Work: TC_RSA_public_work(key) for the public operation, and
 * TC_RSA_private_work with a key whose crt is NULL for the private
 * operation. */
TC_RSA_result TC_RSA_raw_public(const TC_RSA_public_key* key, TC_bytes input, TC_buffer output,
                                const TC_RSA_workspace* workspace, TC_work_budget* work);
TC_RSA_result TC_RSA_raw_private(const TC_RSA_public_key* key, TC_bytes private_exponent,
                                 TC_bytes input, TC_buffer output,
                                 const TC_RSA_workspace* workspace, TC_RSA_execution* execution);

/* Return 1 when bits names a supported modulus size, otherwise 0. */
int TC_RSA_modulus_supported(size_t bits);
#endif

/* Work contracts. TC_work_budget counts public work units: modular
 * operations, encoded and masked bytes, hash invocations and RNG requests. It
 * does not measure time. The functions below return the exact work of one
 * successful call, or 0 for a NULL argument, an unknown or disabled hash, an
 * unsupported size or key, parameters the operation rejects, or a cost
 * above UINT32_MAX. They charge no work. Operations add them as follows:
 *
 *   TC_RSA_raw_public           TC_RSA_public_work
 *   TC_RSA_verify_v15_digest    TC_RSA_public_work + TC_RSA_encode_v15_work
 *   TC_RSA_verify_pss_digest    TC_RSA_public_work + TC_RSA_encode_pss_work
 *   TC_RSA_verify_*_prepared    TC_RSA_prepared_public_work in place of
 *                               TC_RSA_public_work
 *   TC_RSA_encrypt_oaep         1 + TC_RSA_oaep_work + TC_RSA_public_work
 *   TC_RSA_raw_private          TC_RSA_private_work
 *   TC_RSA_sign_v15_digest      TC_RSA_private_work + TC_RSA_encode_v15_work
 *   TC_RSA_sign_pss_digest      TC_RSA_private_work + TC_RSA_encode_pss_work
 *                               + 1 when salt_length is nonzero
 *   TC_RSA_decrypt_oaep         TC_RSA_private_work + TC_RSA_oaep_work
 *
 * Each private-key operation checks its cost for every allowed blinding
 * attempt before arithmetic or any RNG request. A smaller budget returns
 * TC_RSA_LIMIT and leaves outputs, work and the RNG untouched. A rejected
 * blinding factor costs one more attempt, so budget TC_RSA_private_work with
 * execution.random_attempts to cover every attempt. Verification consumes
 * work up to the point where it detects an invalid signature. */

#if TC_ENABLE_RSA
/* EMSA-PKCS1-v1_5 and EMSA-PSS encoding for a modulus of modulus_bytes. The
 * same cost covers PSS encoding and PSS verification. */
uint32_t TC_RSA_encode_v15_work(const TC_RSA_v15_options* options, size_t modulus_bytes);
uint32_t TC_RSA_encode_pss_work(const TC_RSA_pss_options* options, size_t modulus_bytes);
/* EME-OAEP encoding or decoding with both MGF1 masks: modulus_bytes +
 * label length + 1, then D + ceil(D/G)*(H + 5) and H + ceil(H/G)*(D + 5)
 * for message-hash length H, MGF-hash length G and D = modulus_bytes - H - 1. */
uint32_t TC_RSA_oaep_work(const TC_RSA_oaep_options* options, size_t modulus_bytes);
/* One public operation: 16*modulus_bytes + 16*exponent_bytes + 4. */
uint32_t TC_RSA_public_work(const TC_RSA_public_key* key);
/* One public operation with the setup's cached R^2: 16*exponent_bytes + 4.
 * Preparing the setup costs 16*modulus_bytes + 1. Returns 0 for a setup that
 * TC_RSA_prepare_public_key did not initialize. */
uint32_t TC_RSA_prepared_public_work(const TC_RSA_prepared_public_key* setup);
/* One private operation with at most attempts blinding requests, attempts
 * from 1. Full width costs 32*modulus_bytes + 32*exponent_bytes + 8, and a
 * key with key->crt set costs 48*modulus_bytes + 32*exponent_bytes + 12.
 * Each attempt adds 16*modulus_bytes + 1. Only key->public_key and whether
 * key->crt is NULL are read, so a raw private operation can pass a key that
 * holds only its public key. */
uint32_t TC_RSA_private_work(const TC_RSA_private_key* key, size_t attempts);

/* Encode a precomputed digest with EMSA-PKCS1-v1_5 (RFC 8017 section 9.2)
 * for a card or hardware provider that performs the private operation.
 * encoded.capacity is the modulus size, 128, 256, 384 or 512 bytes, and the
 * whole buffer is written. No hashing or key operation is performed. encoded
 * and work are disjoint from the options, the digest and each other.
 *
 * TC_RSA_ARGUMENT     NULL pointer or span data, overlap, or a digest of
 *                     another length than the hash.
 * TC_RSA_UNSUPPORTED  unknown hash, or another encoded.capacity.
 * TC_RSA_LIMIT        work below encoded.capacity.
 *
 * All failures preserve encoded and work. Work: TC_RSA_encode_v15_work. */
TC_RSA_result TC_RSA_encode_v15_digest(const TC_RSA_v15_options* options, TC_bytes digest,
                                       TC_buffer encoded, TC_work_budget* work);

/* Encode a digest and caller-supplied salt with EMSA-PSS (RFC 8017 section
 * 9.1.1). encoded.capacity is the modulus size, 128, 256, 384 or 512 bytes,
 * and emBits is one less than that size in bits. Generate salt with a
 * cryptographic RNG. Inputs may share storage. encoded and work are disjoint
 * from every input and each other.
 *
 * TC_RSA_ARGUMENT     NULL pointer or span data, overlap, salt.length other
 *                     than options->salt_length, or a digest of another
 *                     length than the hash.
 * TC_RSA_UNSUPPORTED  another encoded.capacity, or a hash or MGF hash that is
 *                     unknown or disabled.
 * TC_RSA_INVALID      a salt too long for the modulus.
 * TC_RSA_LIMIT        work below TC_RSA_encode_pss_work.
 * TC_RSA_ERROR        a hash failure during encoding. It wipes encoded and
 *                     consumes work.
 *
 * Other failures preserve encoded and work. Work: TC_RSA_encode_pss_work. */
TC_RSA_result TC_RSA_encode_pss_digest(const TC_RSA_pss_options* options, TC_bytes digest,
                                       TC_bytes salt, TC_buffer encoded, TC_work_budget* work);

/* RSAES-OAEP encryption (RFC 8017 section 7.1.1) with explicit message and
 * MGF hashes, both enabled. An empty label is {NULL, 0}. ciphertext.capacity
 * is at least k, and exactly k bytes are written, only on TC_RSA_OK. The RNG
 * supplies one hash-sized seed.
 *
 * TC_RSA_ARGUMENT     NULL or misaligned storage, NULL random.fill, or
 *                     overlap of an output, scratch or execution with an
 *                     input or with each other.
 * TC_RSA_UNSUPPORTED  unsupported modulus size, or a hash that is unknown or
 *                     disabled.
 * TC_RSA_INVALID      malformed key, or a message longer than k - 2*hLen - 2
 *                     (step 1.b).
 * TC_RSA_LIMIT        short ciphertext, scratch or work.
 * TC_RSA_ERROR        the seed request failed.
 *
 * Work: 1 + TC_RSA_oaep_work + TC_RSA_public_work, checked in full before the
 * seed request. */
TC_RSA_result TC_RSA_encrypt_oaep(const TC_RSA_public_key* key, const TC_RSA_oaep_options* options,
                                  TC_bytes plaintext, TC_buffer ciphertext,
                                  const TC_RSA_workspace* workspace, TC_RSA_execution* execution);

/* RSAES-OAEP decryption (RFC 8017 section 7.1.2) with a validated, unchanged
 * private key and explicit hashes, both enabled. Label bytes are borrowed,
 * and {NULL, 0} selects an empty label. plaintext_length is an aligned
 * size_t. plaintext.capacity must be at least k - 2*hLen - 2, so the status
 * reveals nothing about the padding (note after step 4). Plaintext and its
 * length change only on TC_RSA_OK, and exactly the message length is
 * written. Output bytes, length, scratch and RNG state are separate from
 * each other, inputs and metadata.
 *
 * TC_RSA_ARGUMENT     NULL or misaligned storage, NULL random.fill, or
 *                     overlap.
 * TC_RSA_UNSUPPORTED  unsupported modulus size, or a hash that is unknown or
 *                     disabled.
 * TC_RSA_INVALID      malformed key or CRT values, ciphertext of another
 *                     length than k, or, after decryption, a ciphertext at
 *                     or above the modulus, invalid padding or a wrong label.
 * TC_RSA_LIMIT        short plaintext capacity, zero random_attempts, or
 *                     short scratch or work, before decryption, without an
 *                     RNG request or work charge.
 * TC_RSA_ERROR        RNG or release-check failure.
 *
 * Work: TC_RSA_private_work + TC_RSA_oaep_work. */
TC_RSA_result TC_RSA_decrypt_oaep(const TC_RSA_private_key* key, const TC_RSA_oaep_options* options,
                                  TC_bytes ciphertext, TC_buffer plaintext,
                                  size_t* plaintext_length, const TC_RSA_workspace* workspace,
                                  TC_RSA_execution* execution);

/* Sign a precomputed digest with RSASSA-PKCS1-v1_5 (RFC 8017 section 8.2.1)
 * using a private key validated with TC_RSA_validate_private_key and kept
 * unchanged afterward. key->crt, when set, holds values checked with
 * TC_RSA_validate_crt. signature.capacity is at least k, and exactly k bytes
 * are written, only on TC_RSA_OK. RNG requests provide blinding factors, and
 * execution.random_attempts bounds rejected factors. No hash implementation
 * is required.
 *
 * TC_RSA_ARGUMENT     NULL or misaligned storage, NULL random.fill, overlap,
 *                     or a digest of another length than the hash.
 * TC_RSA_UNSUPPORTED  unsupported modulus size or unknown hash.
 * TC_RSA_INVALID      malformed key or CRT values, such as a factor wider
 *                     than k/2 bytes.
 * TC_RSA_LIMIT        short signature, scratch, zero random_attempts, work,
 *                     or every blinding attempt rejected.
 * TC_RSA_ERROR        RNG or release-check failure.
 *
 * Work: TC_RSA_private_work + TC_RSA_encode_v15_work. */
TC_RSA_result TC_RSA_sign_v15_digest(const TC_RSA_private_key* key,
                                     const TC_RSA_v15_options* options, TC_bytes digest,
                                     TC_buffer signature, const TC_RSA_workspace* workspace,
                                     TC_RSA_execution* execution);

/* RSASSA-PSS signing (RFC 8017 section 8.1.1) with explicit message and MGF
 * hashes, both enabled, and salt length. The RNG supplies the salt, then
 * blinding bytes. A zero-length salt skips its RNG request. Output,
 * workspace, key and status rules match v1.5 signing. A salt too long for
 * the modulus returns INVALID, and a disabled hash returns UNSUPPORTED.
 * Work: TC_RSA_private_work + TC_RSA_encode_pss_work, plus 1 for a salt
 * request. */
TC_RSA_result TC_RSA_sign_pss_digest(const TC_RSA_private_key* key,
                                     const TC_RSA_pss_options* options, TC_bytes digest,
                                     TC_buffer signature, const TC_RSA_workspace* workspace,
                                     TC_RSA_execution* execution);

/* Validate two-prime RSA components at 1024, 2048, 3072 or 4096 bits. Public
 * components use minimal unsigned encodings. d, p and q are nonempty
 * unsigned magnitudes, at most the modulus length, and p and q fit k/2
 * bytes after optional leading zero bytes. All key bytes are borrowed and
 * must remain stable throughout the call. key->crt is ignored here. The
 * call checks the component equations and runs TC_RSA_VALIDATION_ROUNDS
 * Miller-Rabin rounds per factor (FIPS 186-5 appendix B.3.1).
 * execution.random must provide independent cryptographically secure bytes.
 * execution.random_attempts bounds total RNG requests per factor and must be
 * at least TC_RSA_VALIDATION_ROUNDS. RNG state must be separate from key
 * bytes, metadata and workspace. Scratch needs
 * TC_RSA_VALIDATE_WORKSPACE_WORDS limbs.
 *
 * The FIPS 186-5 appendix A.1.1 criteria are checked in addition to the
 * component equations: sqrt(2) 2^(nlen/2 - 1) <= p, q; |p - q| >
 * 2^(nlen/2 - 100); 2^(nlen/2) < d < LCM(p - 1, q - 1); and e d = 1 mod
 * LCM(p - 1, q - 1). exponent_policy selects the public exponent range:
 * TC_RSA_EXPONENT_FIPS requires TC_RSA_exponent_in_fips_range, and
 * TC_RSA_EXPONENT_ANY_ODD accepts any odd 3 <= e < n. Every other criterion
 * applies under both policies.
 *
 * TC_RSA_ARGUMENT     an unknown policy, NULL or misaligned storage, NULL
 *                     random.fill, or overlap.
 * TC_RSA_UNSUPPORTED  unsupported modulus size.
 * TC_RSA_INVALID      malformed components, or a failed criterion or
 *                     primality round.
 * TC_RSA_LIMIT        short scratch, random_attempts below the round count,
 *                     or a work budget below the full cost.
 * TC_RSA_ERROR        an RNG request failed.
 *
 * Work: at most TC_RSA_VALIDATE_WORK(bits, random_attempts). That full cost
 * is checked before any arithmetic or RNG request. A smaller budget, or a
 * cost above UINT32_MAX, returns TC_RSA_LIMIT with work, workspace and the
 * RNG unchanged. Key-strength and application acceptance policies belong to
 * the caller. */
TC_RSA_result TC_RSA_validate_private_key(const TC_RSA_private_key* key,
                                          TC_RSA_exponent_policy exponent_policy,
                                          const TC_RSA_workspace* workspace,
                                          TC_RSA_execution* execution);
/* 1 when a big-endian magnitude is odd and 2^16 < e < 2^256 (FIPS 186-5
 * A.1.1), otherwise 0. Leading zero octets are ignored. Charges no work. */
int TC_RSA_exponent_in_fips_range(TC_bytes exponent);

/* Check CRT components against an already validated, unchanged private key
 * (RFC 8017 section 3.2): dP = d mod (p - 1), dQ = d mod (q - 1) and
 * qInv = q^-1 mod p. Magnitudes are nonempty, at most k bytes, and fit k/2
 * bytes after optional leading zero bytes. Scratch must be separate from all
 * key bytes and metadata and needs TC_RSA_CRT_WORKSPACE_WORDS limbs.
 *
 * TC_RSA_ARGUMENT     NULL or misaligned storage, or overlap.
 * TC_RSA_UNSUPPORTED  unsupported modulus size.
 * TC_RSA_INVALID      malformed key or CRT magnitudes, or a value that
 *                     disagrees with the key.
 * TC_RSA_LIMIT        short scratch or work, before arithmetic.
 *
 * Used scratch is wiped. Preflight failures preserve it. Work: 32*k + 1. Key
 * validation remains a prerequisite. */
TC_RSA_result TC_RSA_validate_crt(const TC_RSA_private_key* key, const TC_RSA_crt* crt,
                                  const TC_RSA_workspace* workspace, TC_work_budget* work);

/* Derive fixed-width dP, dQ and qInv (RFC 8017 section 3.2) from an already
 * validated private key. Each output needs k/2 capacity, and exactly k/2
 * bytes are written to each. Outputs change together only on success and
 * must be mutually disjoint from the key, metadata and scratch. Scratch
 * needs TC_RSA_CRT_WORKSPACE_WORDS limbs.
 *
 * TC_RSA_ARGUMENT     NULL or misaligned storage, or overlap.
 * TC_RSA_UNSUPPORTED  unsupported modulus size.
 * TC_RSA_INVALID      malformed key, or factors without the required
 *                     inverses.
 * TC_RSA_LIMIT        a short output, scratch or work, before arithmetic.
 *
 * Used scratch is wiped. Work: 48*k + 3. */
TC_RSA_result TC_RSA_derive_crt(const TC_RSA_private_key* key, const TC_RSA_crt_output* output,
                                const TC_RSA_workspace* workspace, TC_work_budget* work);

/* Verify a precomputed SHA-1, SHA-224, SHA-256, SHA-384 or SHA-512 digest
 * with RSASSA-PKCS1-v1_5 (RFC 8017 section 8.2.2): RSAVP1 and an exact
 * comparison of the whole encoded message. All bytes are borrowed for this
 * call, and inputs may share storage. Scratch needs
 * TC_RSA_VERIFY_WORKSPACE_WORDS limbs and is wiped after verification. No
 * hash implementation is required for this prehashed API.
 *
 * TC_RSA_OK           the signature matches.
 * TC_RSA_ARGUMENT     NULL or misaligned storage, overlap with workspace or
 *                     work, or a digest of another length than the hash.
 * TC_RSA_UNSUPPORTED  unsupported modulus size or unknown hash.
 * TC_RSA_INVALID      malformed key, a signature of another length than k,
 *                     or, after the work charge, a representative at or
 *                     above the modulus or a mismatched encoding.
 * TC_RSA_LIMIT        short scratch or work, before arithmetic.
 *
 * Work: TC_RSA_public_work + TC_RSA_encode_v15_work. Acceptance policy
 * belongs to the caller. Certificate validation and trust are separate
 * steps. */
TC_RSA_result TC_RSA_verify_v15_digest(const TC_RSA_public_key* key,
                                       const TC_RSA_v15_options* options, TC_bytes digest,
                                       TC_bytes signature, const TC_RSA_workspace* workspace,
                                       TC_work_budget* work);
/* Verify with an initialized setup whose borrowed key and cache storage stay
 * unchanged. Verification workspace and all inputs must be separate from the
 * setup and its cache. A setup that TC_RSA_prepare_public_key did not
 * initialize returns TC_RSA_ARGUMENT. Other results match the one-shot form.
 * Work: TC_RSA_prepared_public_work + TC_RSA_encode_v15_work. */
TC_RSA_result TC_RSA_verify_v15_prepared(const TC_RSA_prepared_public_key* setup,
                                         const TC_RSA_v15_options* options, TC_bytes digest,
                                         TC_bytes signature, const TC_RSA_workspace* workspace,
                                         TC_work_budget* work);

/* RSASSA-PSS verification (RFC 8017 section 8.1.2) with explicit message and
 * MGF hashes, both enabled, and the expected salt length. Salt detection is
 * outside this API. Key, workspace and status rules match v1.5
 * verification. A disabled hash returns UNSUPPORTED, and a salt too long for
 * the modulus returns INVALID. Additional stack storage holds one hash
 * context and a 64-byte digest buffer. Work: TC_RSA_public_work +
 * TC_RSA_encode_pss_work. */
TC_RSA_result TC_RSA_verify_pss_digest(const TC_RSA_public_key* key,
                                       const TC_RSA_pss_options* options, TC_bytes digest,
                                       TC_bytes signature, const TC_RSA_workspace* workspace,
                                       TC_work_budget* work);
/* PSS verification with a prepared setup, under the rules of
 * TC_RSA_verify_v15_prepared. Work: TC_RSA_prepared_public_work +
 * TC_RSA_encode_pss_work. */
TC_RSA_result TC_RSA_verify_pss_prepared(const TC_RSA_prepared_public_key* setup,
                                         const TC_RSA_pss_options* options, TC_bytes digest,
                                         TC_bytes signature, const TC_RSA_workspace* workspace,
                                         TC_work_budget* work);
#endif
#ifdef __cplusplus
}
#endif
#endif

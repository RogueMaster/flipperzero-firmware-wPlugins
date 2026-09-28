/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
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

typedef enum {
  TC_RSA_OK, TC_RSA_INVALID, TC_RSA_LIMIT, TC_RSA_ARGUMENT, TC_RSA_UNSUPPORTED,
  TC_RSA_ERROR, /* Random-source or private-operation verification failure. */
  TC_RSA_IN_PROGRESS, TC_RSA_CANCELLED
} TC_RSA_result;
typedef struct { TC_bytes modulus, exponent; } TC_RSA_public_key;
typedef struct { TC_bytes dp, dq, q_inverse; } TC_RSA_crt;
typedef struct {
  TC_RSA_public_key public_key;
  TC_bytes d, p, q;
  /* Optional validated CRT values. NULL selects full-width exponentiation. */
  const TC_RSA_crt* crt;
} TC_RSA_private_key;
typedef struct { TC_buffer dp, dq, q_inverse; } TC_RSA_crt_output;
typedef struct { TC_RSA_word* words; size_t capacity; } TC_RSA_workspace;
typedef int (*TC_RSA_cancel_fn)(void* user);
typedef struct {
  TC_buffer modulus, exponent, d, p, q;
} TC_RSA_keygen_output;
typedef struct {
  uint32_t candidate_attempts;
  uint32_t random_requests;
} TC_RSA_keygen_limits;
typedef struct {
  TC_random_source random;
  size_t random_attempts;
  /* Reduced by work completed on success and failure. */
  TC_work_budget work;
} TC_RSA_execution;
typedef struct { TC_hash_algorithm hash; } TC_RSA_v15_options;
typedef struct {
  TC_hash_algorithm hash, mgf_hash;
  size_t salt_length;
} TC_RSA_pss_options;
typedef struct {
  TC_hash_algorithm hash, mgf_hash;
  TC_bytes label;
} TC_RSA_oaep_options;
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

/* Static verification storage for a supported, constant key size in bits.
 * Nine limb arrays and two carry words. Use the function below for runtime sizes. */
#define TC_RSA_VERIFY_WORKSPACE_WORDS(bits) (9u * ((bits) / TC_RSA_WORD_BITS) + 2u)
#define TC_RSA_VALIDATE_WORKSPACE_WORDS(bits) (12u * ((bits) / TC_RSA_WORD_BITS) + 2u)
#define TC_RSA_CRT_WORKSPACE_WORDS(bits) (8u * ((bits) / TC_RSA_WORD_BITS))
#define TC_RSA_SIGN_WORKSPACE_WORDS(bits) (14u * ((bits) / TC_RSA_WORD_BITS))
#define TC_RSA_DECRYPT_WORKSPACE_WORDS(bits) (14u * ((bits) / TC_RSA_WORD_BITS))
#define TC_RSA_ENCRYPT_WORKSPACE_WORDS(bits) TC_RSA_VERIFY_WORKSPACE_WORDS(bits)
#define TC_RSA_VALIDATION_ROUNDS 65u
#define TC_RSA_KEYGEN_PUBLIC_EXPONENT 65537u
#define TC_RSA_KEYGEN_WORKSPACE_WORDS(bits) \
  (7u * ((bits) / TC_RSA_WORD_BITS) + 2u)
#define TC_RSA_KEYGEN_STEP_WORK(bits) (24u * ((bits) / 16u) + 3u)

#ifdef __cplusplus
extern "C" {
#endif
/* Number of workspace limbs for verification; zero for unsupported key sizes. */
size_t TC_RSA_verify_workspace_words(size_t bits);
size_t TC_RSA_validate_workspace_words(size_t bits);
size_t TC_RSA_crt_workspace_words(size_t bits);
size_t TC_RSA_sign_workspace_words(size_t bits);
size_t TC_RSA_decrypt_workspace_words(size_t bits);
size_t TC_RSA_encrypt_workspace_words(size_t bits);
size_t TC_RSA_keygen_workspace_words(size_t bits);

/* Generate a two-prime RSA key with e=65537. Output capacities must be at
 * least bits/8 for modulus and d, bits/16 for p and q, and three bytes for e.
 * Output buffers remain unchanged until a complete key is published. Scratch,
 * state, outputs and their metadata must be mutually disjoint.
 *
 * Each step performs at most work->remaining units and returns
 * TC_RSA_IN_PROGRESS when more work is needed. The configured limits bound
 * candidate generation and every RNG request across all steps. Cancellation and terminal failures wipe
 * retained candidates. TC_RSA_KEYGEN_STEP_WORK(bits) lets every pending unit
 * make progress. The RNG must fill each request completely. Callback contexts
 * must be separate from state, scratch and outputs. Call clear after success or
 * whenever abandoning an in-progress operation. */
TC_RSA_result TC_RSA_keygen_init(TC_RSA_keygen_state* state, size_t bits,
    const TC_RSA_keygen_output* output, TC_RSA_keygen_limits limits,
    const TC_RSA_workspace* workspace);
TC_RSA_result TC_RSA_keygen_step(TC_RSA_keygen_state* state,
    TC_random_source random, TC_RSA_cancel_fn cancel, void* cancel_context,
    TC_work_budget* work);
void TC_RSA_keygen_clear(TC_RSA_keygen_state* state);

/* Encode a precomputed SHA digest using EMSA-PKCS1-v1_5 (RFC 8017 section 9.2).
 * Used when a card or hardware provider performs the RSA private operation.
 * encoded.capacity is the modulus size: 128, 256 or 384 bytes. No hashing or key
 * operation is performed. Input and output must be disjoint. All failures
 * preserve output; the work budget must cover encoded.capacity bytes. */
TC_RSA_result TC_RSA_encode_v15_digest(const TC_RSA_v15_options* options, TC_bytes digest,
    TC_buffer encoded, TC_work_budget* work);

/* Encode a digest and caller-supplied salt using EMSA-PSS (RFC 8017 section 9.1.1).
 * encoded.capacity is the modulus size: 128, 256 or 384 bytes; emBits is one
 * less than that size in bits. salt.length must equal options->salt_length.
 * Generate salt with a cryptographic RNG. Inputs may share storage; encoded
 * and work are disjoint from every input and each other. Preflight failures
 * preserve output and work. Processing failures wipe output and consume work. */
TC_RSA_result TC_RSA_encode_pss_digest(const TC_RSA_pss_options* options, TC_bytes digest,
    TC_bytes salt, TC_buffer encoded, TC_work_budget* work);

/* OAEP encryption with explicit message and MGF hashes; both must be enabled.
 * Message length is at most modulus_bytes - 2*hash_bytes - 2. An empty label
 * is {NULL,0}. Ciphertext length equals the modulus length and changes only
 * on TC_RSA_OK. The RNG supplies one hash-sized seed. Keep output, scratch and
 * RNG state separate from inputs and metadata. Used scratch is wiped. */
TC_RSA_result TC_RSA_encrypt_oaep(const TC_RSA_public_key* key,
    const TC_RSA_oaep_options* options, TC_bytes plaintext,
    const TC_RSA_workspace* workspace, TC_buffer ciphertext,
    TC_RSA_execution* execution);

/* OAEP decryption with a validated, unchanged private key and explicit hashes.
 * Both hashes must be enabled. Ciphertext has the modulus length. Label bytes
 * are borrowed; {NULL,0} selects an empty label. Plaintext and its length change
 * only on TC_RSA_OK. Short output storage returns TC_RSA_LIMIT after decoding.
 * Output bytes, length, scratch and RNG state are separate from each other,
 * inputs and metadata. Used scratch is wiped on return. */
TC_RSA_result TC_RSA_decrypt_oaep(const TC_RSA_private_key* key,
    const TC_RSA_oaep_options* options, TC_bytes ciphertext,
    const TC_RSA_workspace* workspace, TC_buffer plaintext,
    size_t* plaintext_length, TC_RSA_execution* execution);

/* Sign a precomputed SHA digest with PKCS#1 v1.5 using a validated private key.
 * Validate the components before use and keep them unchanged afterward.
 * Signature length equals the modulus length. All input, output, metadata,
 * scratch and RNG state are disjoint. RNG requests provide blinding factors;
 * execution.random_attempts bounds rejected factors. Output changes only on
 * TC_RSA_OK. Used scratch is wiped. Hash implementations are optional. */
TC_RSA_result TC_RSA_sign_v15_digest(const TC_RSA_private_key* key,
    const TC_RSA_v15_options* options, TC_bytes digest,
    const TC_RSA_workspace* workspace, TC_buffer signature,
    TC_RSA_execution* execution);

/* PSS signing uses explicit message/MGF hashes and salt length. Both hashes
 * must be enabled. The RNG supplies salt and blinding bytes; zero-length salt
 * skips its RNG request. Workspace and key rules match v1.5 signing. */
TC_RSA_result TC_RSA_sign_pss_digest(const TC_RSA_private_key* key,
    const TC_RSA_pss_options* options, TC_bytes digest,
    const TC_RSA_workspace* workspace, TC_buffer signature,
    TC_RSA_execution* execution);

/* Validate two-prime RSA components at 1024, 2048 or 3072 bits. Public components
 * use minimal unsigned encodings. d, p and q are nonempty unsigned magnitudes,
 * at most the modulus length; leading zero bytes are accepted.
 * All key bytes are borrowed and must remain stable throughout the call.
 * Checks component equations and runs 65 Miller-Rabin rounds per factor.
 * execution.random must provide independent cryptographically secure bytes.
 * execution.random_attempts bounds total RNG requests per factor and must be
 * at least 65. RNG state must be separate from key bytes, metadata and
 * workspace. Scratch is wiped after use.
 * The work budget is 32-bit on every target, including targets with 16-bit size_t.
 * Key-strength and application acceptance policies belong to the caller. */
TC_RSA_result TC_RSA_validate_private_key(const TC_RSA_private_key* key,
    const TC_RSA_workspace* workspace, TC_RSA_execution* execution);

/* Check CRT components against an already validated, unchanged private key.
 * Magnitudes are nonempty, at most the modulus length, with optional leading
 * zero bytes. Scratch must be separate from all key bytes and metadata.
 * Used scratch is wiped; preflight failures preserve it. A sufficient work
 * budget is 32*modulus_bytes+1. Key validation remains a prerequisite. */
TC_RSA_result TC_RSA_validate_crt(const TC_RSA_private_key* key, const TC_RSA_crt* crt,
    const TC_RSA_workspace* workspace, TC_work_budget* work);

/* Derive fixed-width dP, dQ and qInv from an already validated private key.
 * Each output needs modulus_bytes/2 capacity. Outputs change together only on
 * success and must be mutually disjoint from the key, metadata and scratch.
 * Used scratch is wiped. A sufficient work budget is 48*modulus_bytes+3. */
TC_RSA_result TC_RSA_derive_crt(const TC_RSA_private_key* key,
    const TC_RSA_crt_output* output, const TC_RSA_workspace* workspace,
    TC_work_budget* work);

/* Verify a precomputed SHA-1/224/256/384/512 digest with PKCS#1 v1.5.
 * Modulus and exponent are unsigned, minimal big-endian encodings, without DER
 * sign padding. Modulus size is exactly 1024, 2048 or 3072 bits. Signature size
 * must equal the modulus size in bytes. Acceptance policy belongs to the caller.
 *
 * All bytes are borrowed for this call. Workspace must be aligned, separate from
 * inputs and metadata, and exclusive to the operation. Scratch is wiped after
 * verification. No hash implementation is required for this prehashed API.
 * Work is a public bound on modular operations and encoding comparisons, not
 * elapsed time. Unsupported hashes, invalid signatures and exhausted limits are
 * distinct results. This does not validate a certificate or establish trust. */
TC_RSA_result TC_RSA_verify_v15_digest(const TC_RSA_public_key* key,
    const TC_RSA_v15_options* options, TC_bytes digest, TC_bytes signature,
    const TC_RSA_workspace* workspace, TC_work_budget* work);

/* PSS uses explicit message/MGF hashes and salt length, with the same key and
 * workspace rules. Both hashes must be enabled. No automatic salt detection.
 * Additional stack storage holds one hash context and a 64-byte digest buffer. */
TC_RSA_result TC_RSA_verify_pss_digest(const TC_RSA_public_key* key,
    const TC_RSA_pss_options* options, TC_bytes digest, TC_bytes signature,
    const TC_RSA_workspace* workspace, TC_work_budget* work);
#ifdef __cplusplus
}
#endif
#endif

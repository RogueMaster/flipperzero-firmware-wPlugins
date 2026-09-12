#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    CipherKeyNone,
    CipherKeyNumberSingle,
    CipherKeyNumberDouble,
    CipherKeyText,
} CipherKeyKind;

typedef enum {
    CipherCategoryCipher,
    CipherCategoryHash,
    CipherCategoryEncoder,
} CipherCategory;

typedef struct {
    bool ok;
    char* text;
} CipherResult;

void cipher_result_free(CipherResult* result);

typedef CipherResult (
    *CipherTransformFn)(const char* input, int32_t key_a, int32_t key_b, const char* text_key);

typedef struct {
    const char* name;
    const char* file_key;
    const char* learn_text;

    CipherCategory category;
    CipherKeyKind key_kind;

    CipherTransformFn encode;
    CipherTransformFn decode;

    int32_t key_a_min, key_a_max;
    int32_t key_b_min, key_b_max;

    const char* key_a_prompt;
    const char* key_b_prompt;

    uint8_t
        text_key_exact_len; // Exact length required for CipherKeyText, or 0 for unrestricted length
} CipherDef;

extern const CipherDef kCiphers[];
extern const size_t kCipherCount;

const CipherDef* cipher_registry_find_by_name(const char* name);

// Runs encode or decode as appropriate
CipherResult cipher_registry_run(
    const CipherDef* def,
    bool is_decrypt,
    const char* input,
    int32_t key_a,
    int32_t key_b,
    const char* text_key);

// Writes <file_key>.txt or <file_key>_decrypt.txt into out. out must be at least 40 bytes.
void cipher_registry_build_filename(
    const CipherDef* def,
    bool is_decrypt,
    char* out,
    size_t out_size);

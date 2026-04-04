#pragma once
#include "../flippass_build_config.h"
#include "kdbx_includes.h"

typedef struct {
    uint16_t version_minor;
    uint16_t version_major;
    uint8_t encryption_algorithm_uuid[16];
    uint32_t compression_algorithm;
    uint8_t master_seed[32];
    uint8_t encryption_iv[16]; // ChaCha20 uses 12, AES uses 16. We'll use 16 for simplicity.
    uint8_t encryption_iv_size;
    // For now, we'll just store the raw KDF parameters.
    uint8_t* kdf_parameters;
    size_t kdf_parameters_size;
} KDBXHeader;

typedef struct KDBXParser KDBXParser;
typedef bool (*KDBXParserOutputCallback)(const uint8_t* data, size_t data_size, void* context);
typedef void (*KDBXParserKdfProgressCallback)(uint64_t current_round, uint64_t total_rounds, void* context);

KDBXParser* kdbx_parser_alloc(void);
void kdbx_parser_free(KDBXParser* parser);
/** Reset the parser state, close any open stream, and clear cached header data. */
void kdbx_parser_reset(KDBXParser* parser);
/**
 * Open a database file and parse its KDBX header.
 *
 * The payload stream remains open on success so the caller can derive the key
 * and decrypt the payload with the same parser instance.
 */
bool kdbx_parser_process_file(KDBXParser* parser, const char* file_path);
const KDBXHeader* kdbx_parser_get_header(const KDBXParser* parser);
const char* kdbx_parser_get_last_error(const KDBXParser* parser);
void kdbx_parser_set_kdf_progress_callback(
    KDBXParser* parser,
    KDBXParserKdfProgressCallback callback,
    void* context);
bool kdbx_parser_derive_key(
    const KDBXParser* parser,
    const char* password,
    uint8_t* cipher_key,
    size_t cipher_key_size,
    uint8_t* hmac_key,
    size_t hmac_key_size);

bool kdbx_parser_stream_payload(
    KDBXParser* parser,
    const uint8_t* cipher_key,
    size_t cipher_key_size,
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    KDBXParserOutputCallback callback,
    void* context);
bool kdbx_parser_stream_outer_payload(
    KDBXParser* parser,
    const uint8_t* cipher_key,
    size_t cipher_key_size,
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    KDBXParserOutputCallback callback,
    void* context);

uint8_t* kdbx_parser_decrypt_payload(
    KDBXParser* parser,
    const uint8_t* cipher_key,
    size_t cipher_key_size,
    const uint8_t* hmac_key,
    size_t hmac_key_size,
    size_t* payload_size);

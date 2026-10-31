#pragma once

#include <stdint.h>

#define KDBX_SIGNATURE_1     0x9AA2D903
#define KDBX_SIGNATURE_2     0xB54BFB67
/* Second signature of KeePass 1.x (.kdb) files; the first one is shared. */
#define KDBX_SIGNATURE_2_KDB 0xB54BFB65

/* KDBX 4 follows the outer header with its unkeyed SHA-256, then its HMAC-SHA-256. */
#define KDBX_HEADER_SHA256_SIZE 32U

typedef enum {
    KDBX_HEADER_FIELD_ID_END = 0,
    KDBX_HEADER_FIELD_ID_ENCRYPTION_ALGORITHM = 2,
    KDBX_HEADER_FIELD_ID_COMPRESSION_ALGORITHM = 3,
    KDBX_HEADER_FIELD_ID_MASTER_SEED = 4,
    KDBX_HEADER_FIELD_ID_ENCRYPTION_IV = 7,
    KDBX_HEADER_FIELD_ID_KDF_PARAMETERS = 11,
    KDBX_HEADER_FIELD_ID_PUBLIC_CUSTOM_DATA = 12,
} KDBXHeaderFieldID;

typedef enum {
    KDBX_COMPRESSION_NONE = 0,
    KDBX_COMPRESSION_GZIP = 1,
} KDBXCompressionAlgorithm;

static const uint8_t KDBX_UUID_AES256[] =
    {0x31, 0xC1, 0xF2, 0xE6, 0xBF, 0x71, 0x43, 0x50, 0xBE, 0x58, 0x05, 0x21, 0x6A, 0xFC, 0x5A, 0xFF};
static const uint8_t KDBX_UUID_CHACHA20[] =
    {0xD6, 0x03, 0x8A, 0x2B, 0x8B, 0x6F, 0x4C, 0xB5, 0xA5, 0x24, 0x33, 0x9A, 0x31, 0xDB, 0xB5, 0x9A};

/* Messages shared by the unlock pipeline and the pre-password header check. */
#define KDBX_MSG_UNSUPPORTED_CIPHER "Only AES256 or ChaCha20 KDBX 4 databases are supported."
#define KDBX_MSG_UNSUPPORTED_COMPRESSION \
    "Only raw or GZip-compressed KDBX 4 payloads are supported."
#define KDBX_MSG_AES_IV_SIZE         "AES-encrypted databases must use a 16-byte IV."
#define KDBX_MSG_CHACHA20_NONCE_SIZE "ChaCha20-encrypted databases must use a 12-byte nonce."
#define KDBX_MSG_KDF_DICTIONARY_VERSION_FORMAT \
    "This database uses an unsupported KDF dictionary version (%04X)."
#define KDBX_ARGON2_UNSUPPORTED_MESSAGE \
    "Argon2 KDF is not viable on this device. This database cannot be opened."

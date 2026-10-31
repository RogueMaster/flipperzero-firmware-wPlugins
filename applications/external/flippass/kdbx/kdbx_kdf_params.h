#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* KDBX 4 KdfParameters (a VariantDictionary), shared by the unlock pipeline
 * (kdbx_parser.c) and the pre-password header check (kdbx_header_scan.c). Plain C
 * with no Furi dependency so the native test harness compiles it unchanged. */

typedef enum {
    KDBXKdfTypeUnknown = 0,
    KDBXKdfTypeAes,
    KDBXKdfTypeArgon2,
} KDBXKdfType;

typedef enum {
    KDBXKdfParseOk = 0,
    KDBXKdfParseMissing,
    KDBXKdfParseTruncatedBlock,
    KDBXKdfParseUnsupportedVersion,
    KDBXKdfParseTruncatedEntryHeader,
    KDBXKdfParseTruncatedEntryName,
    KDBXKdfParseTruncatedEntryValue,
    KDBXKdfParseInvalidUuid,
    KDBXKdfParseInvalidSalt,
    KDBXKdfParseInvalidRounds,
    KDBXKdfParseUnknownKdf,
    KDBXKdfParseArgon2,
    KDBXKdfParseMissingSalt,
    KDBXKdfParseInvalidAesParameters,
} KDBXKdfParseResult;

typedef struct {
    KDBXKdfType type;
    const uint8_t* salt; /* points into the parsed block */
    size_t salt_size;
    uint64_t rounds;
    uint16_t version; /* dictionary version, also set when it is unsupported */
} KDBXKdfParameters;

/** Parse and validate a KdfParameters block; only AES-KDF with a 32-byte salt and
 *  nonzero rounds succeeds. `out` is filled as far as parsing got. */
KDBXKdfParseResult
    kdbx_kdf_parameters_parse(const uint8_t* data, size_t data_size, KDBXKdfParameters* out);

/** Message for a parse failure; KDBXKdfParseUnsupportedVersion returns the format
 *  string KDBX_MSG_KDF_DICTIONARY_VERSION_FORMAT, which takes the version. */
const char* kdbx_kdf_parse_message(KDBXKdfParseResult result);

#ifdef __cplusplus
}
#endif

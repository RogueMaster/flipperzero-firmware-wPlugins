#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AMIIBO_TAG_SIZE 540
#define AMIIBO_KEY_SIZE 160

typedef struct {
    uint8_t hmac_key[16];
    char type_string[14];
    uint8_t rfu;
    uint8_t magic_size;
    uint8_t magic[16];
    uint8_t xor_pad[32];
} AmiiboMasterKey;

typedef struct {
    AmiiboMasterKey data;
    AmiiboMasterKey tag;
} AmiiboKeys;

typedef enum {
    AmiiboOk = 0,
    AmiiboBadKeys,
    AmiiboBadSignature,
} AmiiboResult;

bool amiibo_keys_from_bytes(AmiiboKeys* keys, const uint8_t* bytes, size_t size);

void amiibo_uid_from_tag(const uint8_t tag[AMIIBO_TAG_SIZE], uint8_t uid[7]);

void amiibo_apply_password(uint8_t tag[AMIIBO_TAG_SIZE]);

void amiibo_fill_blank_trailer(uint8_t tag[AMIIBO_TAG_SIZE]);

AmiiboResult
    amiibo_set_uid(uint8_t tag[AMIIBO_TAG_SIZE], const AmiiboKeys* keys, const uint8_t uid[7]);

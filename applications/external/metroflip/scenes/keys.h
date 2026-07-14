#ifndef KEYS_H
#define KEYS_H

#include "../metroflip_i.h"

typedef enum {
    CARD_TYPE_BIP,
    CARD_TYPE_METROMONEY,
    CARD_TYPE_CHARLIECARD,
    CARD_TYPE_SMARTRIDER,
    CARD_TYPE_TROIKA,
    CARD_TYPE_GOCARD,
    CARD_TYPE_TWO_CITIES,
    CARD_TYPE_RENFE_SUM10,
    CARD_TYPE_RENFE_REGULAR,
    CARD_TYPE_UNKNOWN
} CardType;

typedef struct {
    CardType type;
} CardInfo;

typedef struct {
    uint64_t a;
    uint64_t b;
} MfClassicKeyPair;

typedef struct {
    const MfClassicKeyPair* keys;
    uint32_t data_sector;
} TroikaCardConfig;

extern const MfClassicKeyPair troika_1k_keys[16];
extern const MfClassicKeyPair troika_4k_keys[40];
extern const MfClassicKeyPair charliecard_1k_keys[16];
extern const MfClassicKeyPair bip_1k_keys[16];
extern const MfClassicKeyPair metromoney_1k_keys[16];
extern const uint8_t gocard_verify_data[1][14];
extern const uint8_t gocard_verify_data2[1][14];

#endif // KEYS_H

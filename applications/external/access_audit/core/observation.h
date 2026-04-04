#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    TechTypeUnknown = 0,
    TechTypeRfid125,
    TechTypeNfc13Mhz,
} TechType;

typedef enum {
    CardTypeUnknown = 0,
    /* 125 kHz RFID family */
    CardTypeEm4100Like,
    CardTypeHidProxLike, /* HID H10301 26-bit */
    CardTypeHidGeneric, /* HID generic / extended generic */
    CardTypeIndala, /* Indala 26-bit */
    CardTypeRfid125, /* other 125 kHz protocols */
    /* MIFARE Classic family */
    CardTypeMifareClassic, /* generic fallback */
    CardTypeMifareClassic1K,
    CardTypeMifareClassic4K,
    CardTypeMifareClassicMini,
    /* MIFARE Ultralight / NTAG family */
    CardTypeMifareUltralight, /* generic fallback */
    CardTypeMifareUltralightC,
    CardTypeNtag203,
    CardTypeNtag213,
    CardTypeNtag215,
    CardTypeNtag216,
    CardTypeNtagI2C,
    /* MIFARE DESFire sub-types */
    CardTypeMifareDesfire, /* generic fallback */
    CardTypeMifareDesfireEV1, /* DES/3DES crypto */
    CardTypeMifareDesfireEV2, /* AES crypto */
    CardTypeMifareDesfireEV3, /* AES + enhanced security */
    CardTypeMifareDesfireLight, /* lightweight DESFire variant */
    /* MIFARE Plus security levels */
    CardTypeMifarePlus, /* generic fallback */
    CardTypeMifarePlusSL1, /* SL1 = Classic-compatible, no AES in use */
    CardTypeMifarePlusSL2, /* SL2 = AES crypto, Classic frame structure */
    CardTypeMifarePlusSL3, /* SL3 = AES crypto + ISO14443-4 */
    CardTypeIso14443A,
    CardTypeIso14443B,
    CardTypeIso15693,
    CardTypeHidIclass, /* TI ISO15693 card — potential iCLASS, unconfirmed */
    CardTypeHidIclassLegacy, /* iCLASS confirmed via ACTALL/IDENTIFY: DES/3DES, memory unknown */
    CardTypeHidIclassLegacy2k, /* iCLASS DES/3DES, 2 kilobit memory (most common) */
    CardTypeHidIclassLegacy16k, /* iCLASS DES/3DES, 16 kilobit memory */
    CardTypeHidIclassLegacy32k, /* iCLASS DES/3DES, 32 kilobit memory */
    CardTypeFelica, /* generic fallback — workflow type unknown */
    CardTypeFeliCaLite, /* FeliCa Lite — no mutual auth, used in transit/building */
    CardTypeSlix,
    CardTypeSt25tb,
} CardType;

typedef struct {
    TechType tech;
    CardType card_type;
    bool uid_present;
    bool user_memory_present; /* true when authenticated/protected application memory confirmed */
    bool metadata_complete; /* false when classification could not read all expected fields */
    bool sak_atqa_present; /* true when SAK and ATQA were captured (ISO14443-3A only) */
    bool default_keys_readable; /* true when sector 0 auth succeeded with a default key (Classic only) */
    size_t uid_len;
    uint8_t uid[10];
    uint8_t sak; /* ISO14443-3A Select Acknowledge byte */
    uint8_t atqa[2]; /* ISO14443-3A Answer To Request A */
} AccessObservation;

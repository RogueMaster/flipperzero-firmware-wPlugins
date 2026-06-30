#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
    MANUFACTURER_UNKNOWN = 0,
    MANUFACTURER_ALIEN,
    MANUFACTURER_IMPINJ,
    MANUFACTURER_NXP,
} Manufacturer;

typedef enum {
    TAG_TYPE_UNKNOWN = 0,
    TAG_TYPE_EPC_CLASS1_GEN2,
    TAG_TYPE_ISO18000_6C,
} TagType;

typedef enum {
    TAG_PROTOCOL_UNKNOWN = 0,
    TAG_PROTOCOL_GEN2,
    TAG_PROTOCOL_ISO18000_6C,
} TagProtocol;

typedef enum {
    TAG_MEMORY_UNKNOWN = 0,
    TAG_MEMORY_EPC,
    TAG_MEMORY_TID,
    TAG_MEMORY_USER,
} TagMemory;

typedef enum {
    TAG_LOCK_UNKNOWN = 0,
    TAG_LOCK_UNLOCKED,
    TAG_LOCK_LOCKED,
} TagLock;

typedef enum {
    TAG_KILL_UNKNOWN = 0,
    TAG_KILL_UNLOCKED,
    TAG_KILL_LOCKED,
} TagKill;

typedef enum {
    TAG_PASSWORD_UNKNOWN = 0,
    TAG_PASSWORD_UNLOCKED,
    TAG_PASSWORD_LOCKED,
} TagPassword;

typedef enum {
    TAG_EAS_UNKNOWN = 0,
    TAG_EAS_DISABLED,
    TAG_EAS_ENABLED,
} TagEAS;

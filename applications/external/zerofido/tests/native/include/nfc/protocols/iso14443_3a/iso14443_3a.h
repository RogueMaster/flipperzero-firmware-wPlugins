#pragma once

#include <stdint.h>

#define ISO14443_3A_FDT_LISTEN_FC 1236U

typedef enum {
    Iso14443_3aErrorNone,
    Iso14443_3aErrorNotPresent,
    Iso14443_3aErrorColResFailed,
    Iso14443_3aErrorBufferOverflow,
    Iso14443_3aErrorCommunication,
    Iso14443_3aErrorFieldOff,
    Iso14443_3aErrorWrongCrc,
    Iso14443_3aErrorTimeout,
} Iso14443_3aError;

typedef struct {
    uint8_t uid[10];
    uint8_t uid_len;
    uint8_t atqa[2];
    uint8_t sak;
} Iso14443_3aData;

void iso14443_3a_set_atqa(Iso14443_3aData *data, const uint8_t atqa[2]);
void iso14443_3a_set_sak(Iso14443_3aData *data, uint8_t sak);

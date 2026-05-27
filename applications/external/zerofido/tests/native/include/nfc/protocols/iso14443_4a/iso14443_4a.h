#pragma once

#include <stddef.h>
#include <stdint.h>

#include <lib/toolbox/simple_array.h>
#include "../iso14443_3a/iso14443_3a.h"

typedef enum {
    Iso14443_4aErrorNone = 0,
    Iso14443_4aErrorNotPresent = 1,
    Iso14443_4aErrorProtocol = 2,
    Iso14443_4aErrorTimeout = 3,
    Iso14443_4aErrorSendExtra = 4,
} Iso14443_4aError;

typedef struct {
    uint8_t tl;
    uint8_t t0;
    uint8_t ta_1;
    uint8_t tb_1;
    uint8_t tc_1;
    SimpleArray *t1_tk;
} Iso14443_4aAtsData;

typedef struct {
    Iso14443_3aData *iso14443_3a_data;
    Iso14443_4aAtsData ats_data;
} Iso14443_4aData;

Iso14443_4aData *iso14443_4a_alloc(void);
void iso14443_4a_free(Iso14443_4aData *data);
void iso14443_4a_reset(Iso14443_4aData *data);
void iso14443_4a_set_uid(Iso14443_4aData *data, const uint8_t *uid, size_t uid_len);
const uint8_t *iso14443_4a_get_uid(const Iso14443_4aData *data, size_t *uid_len);
Iso14443_3aData *iso14443_4a_get_base_data(Iso14443_4aData *data);

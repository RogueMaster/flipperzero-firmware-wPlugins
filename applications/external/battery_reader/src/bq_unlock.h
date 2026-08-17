#ifndef BQ_UNLOCK_H
#define BQ_UNLOCK_H

#include "sbs_commands.h"
#include "flipper_i2c.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UNLOCK_OK = 0,
    UNLOCK_ERROR,
    UNLOCK_ALREADY_UNSEALED,
    UNLOCK_NEED_KEY
} UnlockResult;

typedef enum {
    BQ40_PF_CLEAR_OK = 0,
    BQ40_PF_CLEAR_NOT_ACTIVE,
    BQ40_PF_CLEAR_READ_ERROR,
    BQ40_PF_CLEAR_UNSEAL_FAILED,
    BQ40_PF_CLEAR_WRITE_ERROR,
    BQ40_PF_CLEAR_STILL_ACTIVE,
    BQ40_PF_CLEAR_RESULT_UNKNOWN,
    BQ40_PF_CLEAR_UNSAFE_CELLS,
    BQ40_PF_CLEAR_ACCESS_STATE_REQUIRED,
    BQ40_PF_CLEAR_KEY_REJECTED,
} BQ40PfClearResult;

typedef struct {
    uint32_t operation_status;
    uint32_t pf_alert;
    uint32_t pf_status;
    uint8_t security_mode;
} BQ40Status;

typedef enum {
    BQ40_KEY_PRESET_DJI,
    BQ40_KEY_PRESET_TI_FACTORY,
} BQ40KeyPreset;

UnlockResult bq_unseal(BMSI2C* i2c, uint16_t key_w0, uint16_t key_w1);
UnlockResult bq_full_access(BMSI2C* i2c, uint16_t key_w0, uint16_t key_w1);
UnlockResult bq_get_seal_state(BMSI2C* i2c, SealState* state);
BQ40PfClearResult
    bq40_unseal_preset(BMSI2C* i2c, BQ40KeyPreset preset, BQ40Status* before, BQ40Status* after);
BQ40PfClearResult
    bq40_full_access(BMSI2C* i2c, BQ40KeyPreset preset, BQ40Status* before, BQ40Status* after);
BQ40PfClearResult bq40_seal(BMSI2C* i2c, BQ40Status* before, BQ40Status* after);
BQ40PfClearResult bq40_clear_pf(BMSI2C* i2c, BQ40Status* before, BQ40Status* after);
BQ40PfClearResult
    bq30_unseal_sha1(BMSI2C* i2c, const BMSData* data, BQ40Status* before, BQ40Status* after);
BQ40PfClearResult
    bq30_full_access_sha1(BMSI2C* i2c, const BMSData* data, BQ40Status* before, BQ40Status* after);
BQ40PfClearResult bq30_seal(BMSI2C* i2c, BQ40Status* before, BQ40Status* after);
BQ40PfClearResult bq30_service_command(
    BMSI2C* i2c,
    const BMSData* data,
    uint16_t command,
    BQ40Status* before,
    BQ40Status* after);

#endif

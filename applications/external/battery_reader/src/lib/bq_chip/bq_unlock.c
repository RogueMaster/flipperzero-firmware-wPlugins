#include "../../bq_unlock.h"
#include "../../flipper_i2c.h"
#include "../../sbs_commands.h"
#include "../../sha1_digest.h"
#include <furi.h>

#define TAG "BQUnlock"
#define BQ40_CMD_PF_ALERT 0x52
#define BQ40_CMD_PF_STATUS 0x53
#define BQ40_CMD_OPERATION_STATUS 0x54
#define BQ40_CMD_MANUFACTURER_BLOCK_ACCESS 0x44
#define BQ40_MAC_PF_RESET 0x0029
#define BQ40_MAC_SEAL 0x0030
#define BQ40_UNSEAL_KEY_WORD0 0x7EE0
#define BQ40_UNSEAL_KEY_WORD1 0xCCDF
#define BQ40_FULL_ACCESS_KEY_WORD0 0xBF17
#define BQ40_FULL_ACCESS_KEY_WORD1 0xE0BC
#define BQ40_TI_UNSEAL_KEY_WORD0 0x0414
#define BQ40_TI_UNSEAL_KEY_WORD1 0x3672
#define BQ40_TI_FULL_ACCESS_KEY_WORD0 0xFFFF
#define BQ40_TI_FULL_ACCESS_KEY_WORD1 0xFFFF
#define BQ40_OPERATION_STATUS_PF (1UL << 12)
#define BQ30_CMD_MANUFACTURER_INPUT 0x2F
#define BQ30_SUBCMD_UNSEAL 0x0031
#define BQ30_SUBCMD_FULL_ACCESS 0x0032
#define BQ30_SUBCMD_SEAL 0x0030
#define BQ30_SUBCMD_PF_STATUS 0x0053
#define BQ30_SUBCMD_OPERATION_STATUS 0x0054

static const uint8_t bq30_default_key[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
};

UnlockResult bq_get_seal_state(BMSI2C* i2c, SealState* state) {
    uint8_t buf[32];
    uint8_t len;
    
    I2CResult res = bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, 0x3E);
    if(res != I2C_OK) return UNLOCK_ERROR;
    
    res = bms_i2c_read_block(i2c, SBS_CMD_MANUFACTURER_ACCESS, buf, &len, 32);
    if(res != I2C_OK) return UNLOCK_ERROR;
    
    if(len >= 4 && buf[0] == 0x3E && buf[1] == 0x00) {
        uint16_t status = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
        
        if((status & 0x0003) == 0x0003) {
            *state = SEAL_FULL;
        } else if((status & 0x0003) == 0x0002) {
            *state = SEAL_UNSEALED;
        } else {
            *state = SEAL_SEALED;
        }
    } else {
        *state = SEAL_SEALED;
    }
    
    return UNLOCK_OK;
}

UnlockResult bq_unseal(BMSI2C* i2c, uint16_t key_w0, uint16_t key_w1) {
    I2CResult res;
    
    res = bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, key_w0);
    if(res != I2C_OK) return UNLOCK_ERROR;
    
    furi_delay_ms(10);
    
    res = bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, key_w1);
    if(res != I2C_OK) return UNLOCK_ERROR;
    
    return UNLOCK_OK;
}

UnlockResult bq_full_access(BMSI2C* i2c, uint16_t key_w0, uint16_t key_w1) {
    I2CResult res;
    
    res = bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, key_w0);
    if(res != I2C_OK) return UNLOCK_ERROR;
    
    furi_delay_ms(10);
    
    res = bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, key_w1);
    if(res != I2C_OK) return UNLOCK_ERROR;
    
    return UNLOCK_OK;
}

static I2CResult bq40_read_status_word(BMSI2C* i2c, uint8_t command, uint32_t* value) {
    uint8_t request[2] = {command, 0};
    I2CResult result = bms_i2c_write_block(
        i2c, BQ40_CMD_MANUFACTURER_BLOCK_ACCESS, request, sizeof(request));
    if(result != I2C_OK) return result;
    furi_delay_ms(5);

    uint8_t data[8];
    uint8_t length = 0;
    result = bms_i2c_read_block(
        i2c, BQ40_CMD_MANUFACTURER_BLOCK_ACCESS, data, &length, sizeof(data));
    if(result != I2C_OK) return result;
    if(length != 6 || data[0] != command || data[1] != 0) return I2C_ERROR_INVALID;

    *value = (uint32_t)data[2] | ((uint32_t)data[3] << 8) | ((uint32_t)data[4] << 16) |
             ((uint32_t)data[5] << 24);
    return I2C_OK;
}

static I2CResult bq40_read_status(BMSI2C* i2c, BQ40Status* status) {
    I2CResult result =
        bq40_read_status_word(i2c, BQ40_CMD_OPERATION_STATUS, &status->operation_status);
    if(result != I2C_OK) return result;
    status->security_mode = (status->operation_status >> 8) & 0x03;
    result = bq40_read_status_word(i2c, BQ40_CMD_PF_ALERT, &status->pf_alert);
    if(result != I2C_OK) return result;
    result = bq40_read_status_word(i2c, BQ40_CMD_PF_STATUS, &status->pf_status);
    if(result != I2C_OK) return result;

    FURI_LOG_I(
        TAG,
        "BQ40 status: SEC=%u Operation=0x%08lX PFAlert=0x%08lX PFStatus=0x%08lX",
        status->security_mode,
        status->operation_status,
        status->pf_alert,
        status->pf_status);
    return I2C_OK;
}

static I2CResult bq40_read_security_status(BMSI2C* i2c, BQ40Status* status) {
    I2CResult result =
        bq40_read_status_word(i2c, BQ40_CMD_OPERATION_STATUS, &status->operation_status);
    if(result != I2C_OK) return result;
    status->security_mode = (status->operation_status >> 8) & 0x03;
    FURI_LOG_I(
        TAG,
        "BQ40 security status: SEC=%u Operation=0x%08lX",
        status->security_mode,
        status->operation_status);
    return I2C_OK;
}

BQ40PfClearResult bq40_clear_pf(BMSI2C* i2c, BQ40Status* before, BQ40Status* after) {
    if(!i2c || !before || !after) return BQ40_PF_CLEAR_READ_ERROR;
    memset(before, 0, sizeof(BQ40Status));
    memset(after, 0, sizeof(BQ40Status));

    if(bq40_read_status(i2c, before) != I2C_OK) return BQ40_PF_CLEAR_READ_ERROR;
    if(before->pf_status == 0 && !(before->operation_status & BQ40_OPERATION_STATUS_PF)) {
        *after = *before;
        return BQ40_PF_CLEAR_NOT_ACTIVE;
    }

    if(before->security_mode != 2 && before->security_mode != 1) {
        *after = *before;
        return BQ40_PF_CLEAR_ACCESS_STATE_REQUIRED;
    }

    if(bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, BQ40_MAC_PF_RESET) != I2C_OK) {
        return BQ40_PF_CLEAR_WRITE_ERROR;
    }
    furi_delay_ms(350);

    if(bq40_read_status(i2c, after) != I2C_OK) return BQ40_PF_CLEAR_READ_ERROR;
    if(after->pf_status != 0 || (after->operation_status & BQ40_OPERATION_STATUS_PF)) {
        return BQ40_PF_CLEAR_STILL_ACTIVE;
    }
    return BQ40_PF_CLEAR_OK;
}

static BQ40PfClearResult bq40_send_security_key(
    BMSI2C* i2c,
    BQ40Status* before,
    BQ40Status* after,
    uint16_t key_word0,
    uint16_t key_word1,
    uint8_t required_mode,
    uint8_t target_mode) {
    if(!i2c || !before || !after) return BQ40_PF_CLEAR_READ_ERROR;
    memset(before, 0, sizeof(BQ40Status));
    memset(after, 0, sizeof(BQ40Status));

    if(bq40_read_status(i2c, before) != I2C_OK) return BQ40_PF_CLEAR_READ_ERROR;
    if(before->security_mode == target_mode ||
       (target_mode == 2 && before->security_mode == 1)) {
        *after = *before;
        return BQ40_PF_CLEAR_NOT_ACTIVE;
    }
    if(before->security_mode != required_mode) {
        *after = *before;
        return BQ40_PF_CLEAR_ACCESS_STATE_REQUIRED;
    }

    furi_delay_ms(350);
    I2CResult first_write =
        bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, key_word0);
    if(first_write != I2C_OK) {
        furi_delay_ms(350);
        *after = *before;
        I2CResult status_result = bq40_read_security_status(i2c, after);
        if(status_result == I2C_OK &&
           (after->security_mode == target_mode ||
            (target_mode == 2 && after->security_mode == 1))) {
            FURI_LOG_W(TAG, "First key write reported an error, but status confirms target SEC");
            return BQ40_PF_CLEAR_OK;
        }
        if(status_result != I2C_OK) return BQ40_PF_CLEAR_RESULT_UNKNOWN;
        return BQ40_PF_CLEAR_WRITE_ERROR;
    }
    I2CResult second_write =
        bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, key_word1);
    furi_delay_ms(350);

    *after = *before;
    I2CResult status_result = bq40_read_security_status(i2c, after);
    if(status_result != I2C_OK) {
        return second_write == I2C_OK ? BQ40_PF_CLEAR_READ_ERROR :
                                       BQ40_PF_CLEAR_RESULT_UNKNOWN;
    }
    if(after->security_mode == target_mode ||
       (target_mode == 2 && after->security_mode == 1)) {
        if(second_write != I2C_OK) {
            FURI_LOG_W(TAG, "Second key write reported an error, but status confirms target SEC");
        }
        return BQ40_PF_CLEAR_OK;
    }
    if(second_write != I2C_OK) return BQ40_PF_CLEAR_WRITE_ERROR;
    return BQ40_PF_CLEAR_KEY_REJECTED;
}

BQ40PfClearResult bq40_unseal_preset(
    BMSI2C* i2c,
    BQ40KeyPreset preset,
    BQ40Status* before,
    BQ40Status* after) {
    uint16_t word0 = preset == BQ40_KEY_PRESET_TI_FACTORY ? BQ40_TI_UNSEAL_KEY_WORD0 :
                                                           BQ40_UNSEAL_KEY_WORD0;
    uint16_t word1 = preset == BQ40_KEY_PRESET_TI_FACTORY ? BQ40_TI_UNSEAL_KEY_WORD1 :
                                                           BQ40_UNSEAL_KEY_WORD1;
    return bq40_send_security_key(
        i2c,
        before,
        after,
        word0,
        word1,
        3,
        2);
}

BQ40PfClearResult bq40_full_access(
    BMSI2C* i2c,
    BQ40KeyPreset preset,
    BQ40Status* before,
    BQ40Status* after) {
    uint16_t word0 = preset == BQ40_KEY_PRESET_TI_FACTORY ? BQ40_TI_FULL_ACCESS_KEY_WORD0 :
                                                           BQ40_FULL_ACCESS_KEY_WORD0;
    uint16_t word1 = preset == BQ40_KEY_PRESET_TI_FACTORY ? BQ40_TI_FULL_ACCESS_KEY_WORD1 :
                                                           BQ40_FULL_ACCESS_KEY_WORD1;
    return bq40_send_security_key(
        i2c,
        before,
        after,
        word0,
        word1,
        2,
        1);
}

BQ40PfClearResult bq40_seal(BMSI2C* i2c, BQ40Status* before, BQ40Status* after) {
    if(!i2c || !before || !after) return BQ40_PF_CLEAR_READ_ERROR;
    memset(before, 0, sizeof(BQ40Status));
    memset(after, 0, sizeof(BQ40Status));
    if(bq40_read_security_status(i2c, before) != I2C_OK) return BQ40_PF_CLEAR_READ_ERROR;
    if(before->security_mode == 3) {
        *after = *before;
        return BQ40_PF_CLEAR_NOT_ACTIVE;
    }
    if(before->security_mode != 1 && before->security_mode != 2) {
        *after = *before;
        return BQ40_PF_CLEAR_UNSEAL_FAILED;
    }

    I2CResult write_result =
        bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, BQ40_MAC_SEAL);
    *after = *before;
    for(uint8_t attempt = 0; attempt < 3; attempt++) {
        furi_delay_ms(100);
        if(bq40_read_security_status(i2c, after) == I2C_OK && after->security_mode == 3) {
            return BQ40_PF_CLEAR_OK;
        }
    }
    return write_result == I2C_OK ? BQ40_PF_CLEAR_UNSEAL_FAILED :
                                    BQ40_PF_CLEAR_WRITE_ERROR;
}

typedef enum {
    Bq30CellsSafe,
    Bq30CellsUnsafe,
    Bq30CellsReadError,
} Bq30CellCheck;

static Bq30CellCheck bq30_check_cells(BMSI2C* i2c, const BMSData* data) {
    if(!data || data->cell_count < 2 || data->cell_count > 4) return Bq30CellsUnsafe;

    static const uint8_t cell_commands[4] = {
        SBS_CMD_OPT_MFG_FUNCTION1,
        SBS_CMD_OPT_MFG_FUNCTION2,
        SBS_CMD_OPT_MFG_FUNCTION3,
        SBS_CMD_OPT_MFG_FUNCTION4,
    };
    uint16_t minimum = UINT16_MAX;
    uint16_t maximum = 0;
    for(uint8_t index = 0; index < data->cell_count; index++) {
        uint16_t voltage = 0;
        if(bms_i2c_read_word(i2c, cell_commands[index], &voltage) != I2C_OK) {
            return Bq30CellsReadError;
        }
        if(voltage < minimum) minimum = voltage;
        if(voltage > maximum) maximum = voltage;
    }
    return minimum >= 3600 && maximum <= 4400 && maximum - minimum <= 200 ?
               Bq30CellsSafe :
               Bq30CellsUnsafe;
}

static I2CResult bq30_read_status_word(BMSI2C* i2c, uint16_t subcommand, uint32_t* value) {
    I2CResult result =
        bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, subcommand);
    if(result != I2C_OK) return result;
    furi_delay_ms(5);

    uint8_t data[4];
    uint8_t length = 0;
    result = bms_i2c_read_block(
        i2c, SBS_CMD_MANUFACTURER_DATA, data, &length, sizeof(data));
    if(result != I2C_OK) return result;
    if(length != sizeof(data)) return I2C_ERROR_INVALID;

    *value = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
             ((uint32_t)data[3] << 24);
    return I2C_OK;
}

static I2CResult bq30_read_security_status(BMSI2C* i2c, BQ40Status* status) {
    I2CResult result = bq30_read_status_word(
        i2c, BQ30_SUBCMD_OPERATION_STATUS, &status->operation_status);
    if(result != I2C_OK) return result;

    status->security_mode = (status->operation_status >> 8) & 0x03;
    FURI_LOG_I(
        TAG,
        "BQ30 security status: SEC=%u Operation=0x%08lX",
        status->security_mode,
        status->operation_status);
    return I2C_OK;
}

static BQ40PfClearResult bq30_sha_access(
    BMSI2C* i2c,
    const BMSData* battery,
    uint16_t subcommand,
    uint8_t target_mode,
    BQ40Status* before,
    BQ40Status* after) {
    if(!i2c || !battery || !before || !after) return BQ40_PF_CLEAR_READ_ERROR;
    memset(before, 0, sizeof(BQ40Status));
    memset(after, 0, sizeof(BQ40Status));
    bms_i2c_set_pec(i2c, true);
    Bq30CellCheck cells = bq30_check_cells(i2c, battery);
    if(cells == Bq30CellsReadError) return BQ40_PF_CLEAR_READ_ERROR;
    if(cells != Bq30CellsSafe) return BQ40_PF_CLEAR_UNSAFE_CELLS;
    if(!sha1_digest_self_test()) {
        FURI_LOG_E(TAG, "Local SHA-1 self-test failed");
        return BQ40_PF_CLEAR_READ_ERROR;
    }

    if(bq30_read_security_status(i2c, before) != I2C_OK) return BQ40_PF_CLEAR_READ_ERROR;
    if(before->security_mode == target_mode ||
       (target_mode == 2 && before->security_mode == 1)) {
        *after = *before;
        return BQ40_PF_CLEAR_NOT_ACTIVE;
    }
    furi_delay_ms(350);

    I2CResult select_result =
        bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, subcommand);
    furi_delay_ms(5);

    uint8_t challenge[20];
    uint8_t challenge_length = 0;
    I2CResult challenge_result = bms_i2c_read_block(
        i2c,
        BQ30_CMD_MANUFACTURER_INPUT,
        challenge,
        &challenge_length,
        sizeof(challenge));
    if(challenge_result != I2C_OK || challenge_length != sizeof(challenge)) {
        return select_result == I2C_OK ? BQ40_PF_CLEAR_READ_ERROR :
                                        BQ40_PF_CLEAR_WRITE_ERROR;
    }

    uint8_t sha_input[36];
    memcpy(sha_input, bq30_default_key, sizeof(bq30_default_key));
    for(uint8_t index = 0; index < sizeof(challenge); index++) {
        sha_input[sizeof(bq30_default_key) + index] = challenge[sizeof(challenge) - 1 - index];
    }

    uint8_t hash1[20];
    uint8_t hash2[20];
    if(!sha1_digest_short(sha_input, sizeof(sha_input), hash1)) {
        return BQ40_PF_CLEAR_READ_ERROR;
    }
    memcpy(&sha_input[sizeof(bq30_default_key)], hash1, sizeof(hash1));
    if(!sha1_digest_short(sha_input, sizeof(sha_input), hash2)) {
        return BQ40_PF_CLEAR_READ_ERROR;
    }

    uint8_t response[20];
    for(uint8_t index = 0; index < sizeof(response); index++) {
        response[index] = hash2[sizeof(hash2) - 1 - index];
    }
    I2CResult response_result = bms_i2c_write_block(
        i2c, BQ30_CMD_MANUFACTURER_INPUT, response, sizeof(response));
    furi_delay_ms(350);

    *after = *before;
    I2CResult status_result = bq30_read_security_status(i2c, after);
    if(status_result != I2C_OK) {
        return response_result == I2C_OK ? BQ40_PF_CLEAR_READ_ERROR :
                                          BQ40_PF_CLEAR_RESULT_UNKNOWN;
    }
    if(after->security_mode == target_mode ||
       (target_mode == 2 && after->security_mode == 1)) {
        return BQ40_PF_CLEAR_OK;
    }
    return response_result == I2C_OK ? BQ40_PF_CLEAR_KEY_REJECTED :
                                      BQ40_PF_CLEAR_WRITE_ERROR;
}

BQ40PfClearResult bq30_unseal_sha1(
    BMSI2C* i2c,
    const BMSData* data,
    BQ40Status* before,
    BQ40Status* after) {
    return bq30_sha_access(i2c, data, BQ30_SUBCMD_UNSEAL, 2, before, after);
}

BQ40PfClearResult bq30_full_access_sha1(
    BMSI2C* i2c,
    const BMSData* data,
    BQ40Status* before,
    BQ40Status* after) {
    return bq30_sha_access(i2c, data, BQ30_SUBCMD_FULL_ACCESS, 1, before, after);
}

BQ40PfClearResult bq30_seal(BMSI2C* i2c, BQ40Status* before, BQ40Status* after) {
    if(!i2c || !before || !after) return BQ40_PF_CLEAR_READ_ERROR;
    memset(before, 0, sizeof(BQ40Status));
    memset(after, 0, sizeof(BQ40Status));
    bms_i2c_set_pec(i2c, true);
    if(bq30_read_security_status(i2c, before) != I2C_OK) return BQ40_PF_CLEAR_READ_ERROR;
    if(before->security_mode == 3) {
        *after = *before;
        return BQ40_PF_CLEAR_NOT_ACTIVE;
    }
    if(before->security_mode != 1 && before->security_mode != 2) {
        *after = *before;
        return BQ40_PF_CLEAR_UNSEAL_FAILED;
    }

    I2CResult write_result =
        bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, BQ30_SUBCMD_SEAL);
    *after = *before;
    for(uint8_t attempt = 0; attempt < 3; attempt++) {
        furi_delay_ms(100);
        if(bq30_read_security_status(i2c, after) == I2C_OK && after->security_mode == 3) {
            return BQ40_PF_CLEAR_OK;
        }
    }
    return write_result == I2C_OK ? BQ40_PF_CLEAR_UNSEAL_FAILED :
                                    BQ40_PF_CLEAR_WRITE_ERROR;
}

BQ40PfClearResult bq30_service_command(
    BMSI2C* i2c,
    const BMSData* data,
    uint16_t command,
    BQ40Status* before,
    BQ40Status* after) {
    if(!i2c || !data || !before || !after) return BQ40_PF_CLEAR_READ_ERROR;
    memset(before, 0, sizeof(BQ40Status));
    memset(after, 0, sizeof(BQ40Status));
    if(command != 0x0012 && command != 0x0028 && command != 0x0029 && command != 0x002A) {
        return BQ40_PF_CLEAR_WRITE_ERROR;
    }
    bms_i2c_set_pec(i2c, true);
    Bq30CellCheck cells = bq30_check_cells(i2c, data);
    if(cells == Bq30CellsReadError) return BQ40_PF_CLEAR_READ_ERROR;
    if(cells != Bq30CellsSafe) return BQ40_PF_CLEAR_UNSAFE_CELLS;
    if(bq30_read_security_status(i2c, before) != I2C_OK) return BQ40_PF_CLEAR_READ_ERROR;
    if(before->security_mode != 1) {
        *after = *before;
        return BQ40_PF_CLEAR_ACCESS_STATE_REQUIRED;
    }
    if(command == 0x0029 &&
       bq30_read_status_word(i2c, BQ30_SUBCMD_PF_STATUS, &before->pf_status) != I2C_OK) {
        return BQ40_PF_CLEAR_READ_ERROR;
    }
    if(command == 0x0029 && before->pf_status == 0) {
        *after = *before;
        return BQ40_PF_CLEAR_NOT_ACTIVE;
    }

    I2CResult write_result =
        bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, command);
    furi_delay_ms(command == 0x0012 ? 1000 : 350);
    *after = *before;
    I2CResult status_result = bq30_read_security_status(i2c, after);
    if(status_result != I2C_OK) {
        return write_result == I2C_OK ? BQ40_PF_CLEAR_RESULT_UNKNOWN :
                                       BQ40_PF_CLEAR_WRITE_ERROR;
    }
    if(command == 0x0029) {
        I2CResult pf_result =
            bq30_read_status_word(i2c, BQ30_SUBCMD_PF_STATUS, &after->pf_status);
        if(pf_result != I2C_OK) return BQ40_PF_CLEAR_RESULT_UNKNOWN;
        if(after->pf_status != 0) return BQ40_PF_CLEAR_STILL_ACTIVE;
    }
    return write_result == I2C_OK ? BQ40_PF_CLEAR_OK : BQ40_PF_CLEAR_WRITE_ERROR;
}

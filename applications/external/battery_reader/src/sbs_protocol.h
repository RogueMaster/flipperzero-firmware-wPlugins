#ifndef SBS_PROTOCOL_H
#define SBS_PROTOCOL_H

#include "sbs_commands.h"
#include "flipper_i2c.h"

extern BMSI2C g_bms_i2c;

I2CResult sbs_read_basic_info(BMSData* data);
I2CResult sbs_read_status(BMSData* data);
I2CResult sbs_read_readonly(uint8_t address, BMSData* data);
I2CResult sbs_read_bq40_extended(BMSData* data);
I2CResult sbs_read_bq30_extended(BMSData* data);
I2CResult sbs_detect_chip(BQChipType* chip_type, uint16_t* fw_version);

const char* bq_chip_name(BQChipType chip);
const char* seal_state_name(SealState state);

#endif

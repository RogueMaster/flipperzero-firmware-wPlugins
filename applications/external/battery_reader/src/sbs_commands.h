#ifndef SBS_COMMANDS_H
#define SBS_COMMANDS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SBS_CMD_MANUFACTURER_ACCESS = 0x00,
    SBS_CMD_REMAINING_CAPACITY_ALARM = 0x01,
    SBS_CMD_REMAINING_TIME_ALARM = 0x02,
    SBS_CMD_BATTERY_MODE = 0x03,
    SBS_CMD_AT_RATE = 0x04,
    SBS_CMD_AT_RATE_TO_FULL = 0x05,
    SBS_CMD_AT_RATE_TO_EMPTY = 0x06,
    SBS_CMD_AT_RATE_OK = 0x07,
    SBS_CMD_TEMPERATURE = 0x08,
    SBS_CMD_VOLTAGE = 0x09,
    SBS_CMD_CURRENT = 0x0A,
    SBS_CMD_AVERAGE_CURRENT = 0x0B,
    SBS_CMD_MAX_ERROR = 0x0C,
    SBS_CMD_RELATIVE_STATE_OF_CHARGE = 0x0D,
    SBS_CMD_ABSOLUTE_STATE_OF_CHARGE = 0x0E,
    SBS_CMD_REMAINING_CAPACITY = 0x0F,
    SBS_CMD_FULL_CHARGE_CAPACITY = 0x10,
    SBS_CMD_RUN_TIME_TO_EMPTY = 0x11,
    SBS_CMD_AVERAGE_TIME_TO_EMPTY = 0x12,
    SBS_CMD_AVERAGE_TIME_TO_FULL = 0x13,
    SBS_CMD_CHARGING_CURRENT = 0x14,
    SBS_CMD_CHARGING_VOLTAGE = 0x15,
    SBS_CMD_BATTERY_STATUS = 0x16,
    SBS_CMD_CYCLE_COUNT = 0x17,
    SBS_CMD_DESIGN_CAPACITY = 0x18,
    SBS_CMD_DESIGN_VOLTAGE = 0x19,
    SBS_CMD_SPECIFICATION_INFO = 0x1A,
    SBS_CMD_MANUFACTURE_DATE = 0x1B,
    SBS_CMD_SERIAL_NUMBER = 0x1C,
    SBS_CMD_MANUFACTURER_NAME = 0x20,
    SBS_CMD_DEVICE_NAME = 0x21,
    SBS_CMD_DEVICE_CHEMISTRY = 0x22,
    SBS_CMD_MANUFACTURER_DATA = 0x23,
    SBS_CMD_OPT_MFG_FUNCTION5 = 0x2F,
    SBS_CMD_OPT_MFG_FUNCTION4 = 0x3C,
    SBS_CMD_OPT_MFG_FUNCTION3 = 0x3D,
    SBS_CMD_OPT_MFG_FUNCTION2 = 0x3E,
    SBS_CMD_OPT_MFG_FUNCTION1 = 0x3F
} SBSCommand;

typedef enum {
    BQ_CHIP_UNKNOWN = 0,
    BQ_CHIP_BQ30Z55 = 0x010550,
    BQ_CHIP_BQ30Z554 = 0x010554,
    BQ_CHIP_BQ30Z50 = 0x010500,
    BQ_CHIP_BQ40Z307 = 0x014307,
    BQ_CHIP_BQ40Z50 = 0x014500,
    BQ_CHIP_BQ40Z60 = 0x014600,
    BQ_CHIP_BQ40Z80 = 0x014800
} BQChipType;

typedef struct {
    uint8_t i2c_address;
    BQChipType chip_type;
    char dji_serial[32];
    char manufacturer[32];
    char device_name[32];
    char device_chemistry[8];
    uint16_t serial_number;
    uint16_t manufacture_date;
    uint16_t design_capacity;
    uint16_t design_voltage;
    uint16_t full_charge_capacity;
    uint16_t voltage;
    int16_t current;
    int16_t average_current;
    uint16_t temperature;
    uint8_t relative_state_of_charge;
    uint8_t absolute_state_of_charge;
    uint16_t remaining_capacity;
    uint16_t cycle_count;
    uint8_t max_error;
    uint16_t battery_status;
    uint16_t spec_info;
    uint16_t firmware_version;
    uint16_t firmware_build;
    uint16_t cell_voltage[4];
    uint8_t cell_count;
    uint8_t seal_state;
} BMSData;

typedef enum {
    SEAL_FULL = 0,
    SEAL_UNSEALED = 1,
    SEAL_SEALED = 2,
    SEAL_UNKNOWN = 0xFF,
} SealState;

#define BS_FLAG_FULLY_DISCHARGED          (1 << 4)
#define BS_FLAG_FULLY_CHARGED             (1 << 5)
#define BS_FLAG_DISCHARGING               (1 << 6)
#define BS_FLAG_INITIALIZED               (1 << 7)
#define BS_FLAG_REMAINING_TIME_ALARM      (1 << 8)
#define BS_FLAG_REMAINING_CAPACITY_ALARM  (1 << 9)
#define BS_FLAG_TERMINATE_DISCHARGE_ALARM (1 << 11)
#define BS_FLAG_OVERTEMPERATURE_ALARM     (1 << 12)
#define BS_FLAG_TERMINATE_CHARGE_ALARM    (1 << 14)
#define BS_FLAG_OVER_CHARGED_ALARM        (1 << 15)

#endif

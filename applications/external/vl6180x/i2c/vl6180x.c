
#include "vl6180x.h"

#include <furi.h>
#include <furi_hal_i2c.h>

#define LOG_TAG "vl6180x_i2c_communication"

// See datasheet of VL6180X for details on the register address
// Note: Only providing the registers that are used
typedef enum {
    MODEL_ID = 0x0000,
    MODEL_REVISION_MAJOR = 0x0001,
    MODEL_REVISION_MINOR = 0x0002,
    MODULE_REVISION_MAJOR = 0x0003,
    MODULE_REVISION_MINOR = 0x0004,
    MODE_GPIO1 = 0x0011,
    HISTORY_CONTROL = 0x0012,
    INTERRUPT_CONFIG_GPIO = 0x0014,
    INTERRUPT_CLEAR = 0x0015,
    FRESH_OUT_OF_RESET = 0x0016,
    RANGE_CONFIGURATION = 0x0018,
    RANGE_INTERMAESREMENT_PERIOD = 0x001B,
    VHV_RECALIBRATE = 0x002E,
    VHV_REPEAT_RATE = 0x0031,
    ALS_INTERMAESREMENT_PERIOD = 0x003E,
    ANALOGUE_GAIN = 0x003F,
    ALS_INTEGRATION_PERIOD = 0x0040,
    RANGE_STATUS = 0x004D,
    INTERRUPT_STATUS = 0x004F,
    RANGE_VALUE = 0x0062,
    RANGE_RAW = 0x0064,
    AVERAGING_SAMPLE_PERIOD = 0x010A,

    // Private Registers note found in data sheet
    REGISTER_207 = 0x0207,
    REGISTER_208 = 0x0208,
    REGISTER_096 = 0x0096,
    REGISTER_097 = 0x0097,
    REGISTER_0E3 = 0x00E3,
    REGISTER_0E4 = 0x00E4,
    REGISTER_0E5 = 0x00E5,
    REGISTER_0E6 = 0x00E6,
    REGISTER_0E7 = 0x00E7,
    REGISTER_0F5 = 0x00F5,
    REGISTER_0D9 = 0x00D9,
    REGISTER_0DB = 0x00DB,
    REGISTER_0DC = 0x00DC,
    REGISTER_0DD = 0x00DD,
    REGISTER_09F = 0x009F,
    REGISTER_0A3 = 0x00A3,
    REGISTER_0B7 = 0x00B7,
    REGISTER_0BB = 0x00BB,
    REGISTER_0B2 = 0x00B2,
    REGISTER_0CA = 0x00CA,
    REGISTER_198 = 0x0198,
    REGISTER_1B0 = 0x01B0,
    REGISTER_1AD = 0x01AD,
    REGISTER_0FF = 0x00FF,
    REGISTER_100 = 0x0100,
    REGISTER_199 = 0x0199,
    REGISTER_1A6 = 0x01A6,
    REGISTER_1AC = 0x01AC,
    REGISTER_1A7 = 0x01A7,
    REGISTER_030 = 0x0030,
} VL6180X_Register;

typedef struct {
    VL6180X_Register vl6180x_register;
    uint8_t value;
} VL6180X_ConfigurationSetting;

// See https://github.com/adafruit/Adafruit_CircuitPython_VL6180X/blob/main/adafruit_vl6180x.py#L322 for settings
static VL6180X_ConfigurationSetting DEFAULT_SETTINGS[] = {
    {.vl6180x_register = REGISTER_207, .value = 0x01},
    {.vl6180x_register = REGISTER_208, .value = 0x01},
    {.vl6180x_register = REGISTER_096, .value = 0x00},
    {.vl6180x_register = REGISTER_097, .value = 0xFD},
    {.vl6180x_register = REGISTER_0E3, .value = 0x00},
    {.vl6180x_register = REGISTER_0E4, .value = 0x04},
    {.vl6180x_register = REGISTER_0E5, .value = 0x02},
    {.vl6180x_register = REGISTER_0E6, .value = 0x01},
    {.vl6180x_register = REGISTER_0E7, .value = 0x03},
    {.vl6180x_register = REGISTER_0F5, .value = 0x02},
    {.vl6180x_register = REGISTER_0D9, .value = 0x05},
    {.vl6180x_register = REGISTER_0DB, .value = 0xCE},
    {.vl6180x_register = REGISTER_0DC, .value = 0x03},
    {.vl6180x_register = REGISTER_0DD, .value = 0xF8},
    {.vl6180x_register = REGISTER_09F, .value = 0x00},
    {.vl6180x_register = REGISTER_0A3, .value = 0x3C},
    {.vl6180x_register = REGISTER_0B7, .value = 0x00},
    {.vl6180x_register = REGISTER_0BB, .value = 0x3C},
    {.vl6180x_register = REGISTER_0B2, .value = 0x09},
    {.vl6180x_register = REGISTER_0CA, .value = 0x09},
    {.vl6180x_register = REGISTER_198, .value = 0x01},
    {.vl6180x_register = REGISTER_1B0, .value = 0x17},
    {.vl6180x_register = REGISTER_1AD, .value = 0x00},
    {.vl6180x_register = REGISTER_0FF, .value = 0x05},
    {.vl6180x_register = REGISTER_100, .value = 0x05},
    {.vl6180x_register = REGISTER_199, .value = 0x05},
    {.vl6180x_register = REGISTER_1A6, .value = 0x1B},
    {.vl6180x_register = REGISTER_1AC, .value = 0x3E},
    {.vl6180x_register = REGISTER_1A7, .value = 0x1F},
    {.vl6180x_register = REGISTER_030, .value = 0x00},

    // Enables polling for "New Sample Ready" when measurement is complete
    {.vl6180x_register = MODE_GPIO1, .value = 0x10},
    // Set the averaging sample period
    {.vl6180x_register = AVERAGING_SAMPLE_PERIOD, .value = 0x30},
    // Set the light and dark gain
    {.vl6180x_register = ANALOGUE_GAIN, .value = 0x46},
    // Sets the # of range measurements after auto calibration of the system is performed
    {.vl6180x_register = VHV_REPEAT_RATE, .value = 0xFF},
    // Set ALS integration time to 100ms
    {.vl6180x_register = ALS_INTEGRATION_PERIOD, .value = 0x63},
    // Perform single temperature calibration
    {.vl6180x_register = VHV_RECALIBRATE, .value = 0x01},
    // Set default ranging period to 100ms
    {.vl6180x_register = RANGE_INTERMAESREMENT_PERIOD, .value = 0x09},
    // Set default ALS period 10 500ms
    {.vl6180x_register = ALS_INTERMAESREMENT_PERIOD, .value = 0x31},
    // Configure interrupt on new sample
    {.vl6180x_register = INTERRUPT_CONFIG_GPIO, .value = 0x24},
};

#define VL6180X_DEFAULT_SETTINGS_COUNT \
    (sizeof(DEFAULT_SETTINGS) / sizeof(VL6180X_ConfigurationSetting))

// Note: Assumes that the handle has already been acquired
static void write_register(uint8_t i2c_address, VL6180X_Register vl6180x_register, uint8_t data) {
    FURI_LOG_T(LOG_TAG, __func__);

    uint8_t command[] = {vl6180x_register >> 8, vl6180x_register & 0xFF, data};

    if(!furi_hal_i2c_tx(
           &furi_hal_i2c_handle_external, i2c_address, command, sizeof(command), 100)) {
        return;
    }
}

// Note: Assumes that the handle has already been acquired
static uint8_t read_register(uint8_t i2c_address, VL6180X_Register vl6180x_register) {
    FURI_LOG_T(LOG_TAG, __func__);

    uint8_t command[] = {vl6180x_register >> 8, vl6180x_register & 0xFF};

    uint8_t response;

    if(!furi_hal_i2c_trx(
           &furi_hal_i2c_handle_external,
           i2c_address,
           command,
           sizeof(command),
           &response,
           1,
           100)) {
        return VL6180X_FAILED_DISTANCE;
    }

    return response;
}

uint8_t find_vl6180x_address() {
    FURI_LOG_T(LOG_TAG, __func__);

    uint8_t address;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Search for a device
    for(address = 0; address != VL6180X_NO_DEVICE_FOUND_ADDRESS; address++) {
        if(furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_external, address, 5)) {
            uint8_t model_id = read_register(address, MODEL_ID);
            FURI_LOG_D(
                LOG_TAG, "Found I2C Device at address %02X with Model ID %02X", address, model_id);

            if(model_id != 0xB4) {
                FURI_LOG_D(LOG_TAG, "I2C Device is not a VL6180X");
                continue;
            }

            FURI_LOG_I(LOG_TAG, "Found VL6180X at address %02X", address);
            FURI_LOG_D(
                LOG_TAG,
                "Model = %02X.%02X, Module = %02X.%02X",
                read_register(address, MODEL_REVISION_MAJOR),
                read_register(address, MODEL_REVISION_MINOR),
                read_register(address, MODULE_REVISION_MAJOR),
                read_register(address, MODULE_REVISION_MINOR));
            break;
        }
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return address;
}

void configure_vl6180x(uint8_t vl6180x_address) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(vl6180x_address != VL6180X_NO_DEVICE_FOUND_ADDRESS);
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    FURI_LOG_I(LOG_TAG, "Configuring %d default settings", VL6180X_DEFAULT_SETTINGS_COUNT);

    for(uint8_t i = 0; i < VL6180X_DEFAULT_SETTINGS_COUNT; i++) {
        write_register(
            vl6180x_address, DEFAULT_SETTINGS[i].vl6180x_register, DEFAULT_SETTINGS[i].value);
    }

    // Clear fresh register
    write_register(vl6180x_address, FRESH_OUT_OF_RESET, 0x00);

    // Clear the continuous read bit in the event it was being used
    // This is to allow for starting the app multiple times without manually resetting the board
    write_register(vl6180x_address, RANGE_CONFIGURATION, 0x01);
    furi_delay_ms(100);

    // Enable history
    write_register(vl6180x_address, HISTORY_CONTROL, 0x01);
    // Clear Interrupt
    write_register(vl6180x_address, INTERRUPT_CLEAR, 0x07);
    // Set to continuous
    write_register(vl6180x_address, RANGE_CONFIGURATION, 0x03);

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
}

uint8_t read_vl6180x_range(uint8_t vl6180x_address) {
    FURI_LOG_T(LOG_TAG, __func__);
    furi_assert(vl6180x_address != VL6180X_NO_DEVICE_FOUND_ADDRESS);
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Wait for interrupt
    FURI_LOG_T(LOG_TAG, "Waiting for VL6180X to have a range value");
    while(true) {
        if((read_register(vl6180x_address, INTERRUPT_STATUS) & 0x04) != 0x00) {
            break;
        }
    }

    uint8_t distance = read_register(vl6180x_address, RANGE_VALUE);

    // Clear interrupt registers
    write_register(vl6180x_address, INTERRUPT_CLEAR, 0x07);

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return distance;
}

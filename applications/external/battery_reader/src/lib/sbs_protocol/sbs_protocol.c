#include "../../sbs_commands.h"
#include "../../flipper_i2c.h"
#include "../../sbs_protocol.h"
#include <string.h>
#include <stdio.h>
#include <furi.h>

#define TAG "SBSProtocol"
#define BQ40_CMD_MANUFACTURER_BLOCK_ACCESS 0x44
#define BQ40_SUBCMD_FIRMWARE_VERSION 0x0002
#define BQ40_SUBCMD_OPERATION_STATUS 0x0054
#define BQ30_CMD_MANUFACTURER_INPUT 0x2F
#define BQ30_SUBCMD_FIRMWARE_VERSION 0x0002
#define BQ30_SUBCMD_OPERATION_STATUS 0x0054
#define DJI_CMD_BATTERY_SERIAL 0xD8



BMSI2C g_bms_i2c;

static I2CResult read_string(BMSI2C* i2c, uint8_t cmd, char* dest, size_t max_len) {
    if(!dest || max_len == 0 || max_len > 32) return I2C_ERROR_INVALID;
    dest[0] = '\0';
    uint8_t buf[32];
    uint8_t len;
    
    I2CResult res = bms_i2c_read_block(i2c, cmd, buf, &len, max_len);
    if(res != I2C_OK) return res;
    
    if(len > max_len - 1) len = max_len - 1;
    memcpy(dest, buf, len);
    dest[len] = '\0';
    
    return I2C_OK;
}

static I2CResult read_word(BMSI2C* i2c, uint8_t cmd, uint16_t* value) {
    return bms_i2c_read_word(i2c, cmd, value);
}

static I2CResult read_byte(BMSI2C* i2c, uint8_t cmd, uint8_t* value) {
    uint16_t val16;
    I2CResult res = bms_i2c_read_word(i2c, cmd, &val16);
    *value = (uint8_t)val16;
    return res;
}

static I2CResult read_signed_word(BMSI2C* i2c, uint8_t cmd, int16_t* value) {
    uint16_t raw;
    I2CResult res = bms_i2c_read_word(i2c, cmd, &raw);
    if(res != I2C_OK) return res;
    
    if(raw & 0x8000) {
        *value = (int16_t)(raw - 0x10000);
    } else {
        *value = (int16_t)raw;
    }
    return I2C_OK;
}

static I2CResult read_bq40_mba(
    BMSI2C* i2c,
    uint16_t subcommand,
    uint8_t* data,
    uint8_t* length,
    uint8_t max_length) {
    uint8_t request[2] = {(uint8_t)subcommand, (uint8_t)(subcommand >> 8)};
    I2CResult result = bms_i2c_write_block(
        i2c, BQ40_CMD_MANUFACTURER_BLOCK_ACCESS, request, sizeof(request));
    if(result != I2C_OK) return result;
    furi_delay_ms(5);

    uint8_t response[32];
    uint8_t response_length = 0;
    result = bms_i2c_read_block(
        i2c,
        BQ40_CMD_MANUFACTURER_BLOCK_ACCESS,
        response,
        &response_length,
        sizeof(response));
    if(result != I2C_OK) return result;
    if(response_length < 2 || response[0] != request[0] || response[1] != request[1]) {
        return I2C_ERROR_INVALID;
    }

    uint8_t payload_length = response_length - 2;
    if(payload_length > max_length) return I2C_ERROR_INVALID;
    memcpy(data, &response[2], payload_length);
    *length = payload_length;
    return I2C_OK;
}

static I2CResult read_manufacturer_access(BMSI2C* i2c, uint8_t subcmd, uint8_t* buf, uint8_t* len) {
    FURI_LOG_D(TAG, "read_manufacturer_access subcmd=0x%02X", subcmd);
    
    I2CResult res = bms_i2c_write_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, subcmd);
    
    furi_delay_ms(50);
    
    uint16_t raw;
    res = bms_i2c_read_word(i2c, SBS_CMD_MANUFACTURER_ACCESS, &raw);
    if(res != I2C_OK) {
        FURI_LOG_E(TAG, "read_manufacturer_access: read failed res=%d", res);
        return res;
    }
    
    buf[0] = (uint8_t)(raw & 0xFF);
    buf[1] = (uint8_t)((raw >> 8) & 0xFF);
    *len = 2;
    
    FURI_LOG_D(TAG, "read_manufacturer_access: result=%d len=%d data=0x%04X", res, *len, raw);
    return res;
}

I2CResult sbs_read_basic_info(BMSData* data) {
    BMSI2C* i2c = &g_bms_i2c;
    
    I2CResult res;
    
    res = read_string(i2c, SBS_CMD_MANUFACTURER_NAME, data->manufacturer, sizeof(data->manufacturer));
    if(res != I2C_OK) FURI_LOG_W(TAG, "Optional ManufacturerName read failed: %d", res);
    
    res = read_string(i2c, SBS_CMD_DEVICE_NAME, data->device_name, sizeof(data->device_name));
    if(res != I2C_OK) FURI_LOG_W(TAG, "Optional DeviceName read failed: %d", res);
    
    res = read_string(i2c, SBS_CMD_DEVICE_CHEMISTRY, data->device_chemistry, sizeof(data->device_chemistry));
    if(res != I2C_OK) FURI_LOG_W(TAG, "Optional DeviceChemistry read failed: %d", res);
    
    res = read_word(i2c, SBS_CMD_SERIAL_NUMBER, &data->serial_number);
    if(res != I2C_OK) return res;
    
    res = read_word(i2c, SBS_CMD_MANUFACTURE_DATE, &data->manufacture_date);
    if(res != I2C_OK) return res;
    
    res = read_word(i2c, SBS_CMD_DESIGN_CAPACITY, &data->design_capacity);
    if(res != I2C_OK) return res;
    
    res = read_word(i2c, SBS_CMD_DESIGN_VOLTAGE, &data->design_voltage);
    if(res != I2C_OK) return res;
    
    res = read_word(i2c, SBS_CMD_FULL_CHARGE_CAPACITY, &data->full_charge_capacity);
    
    return res;
}

I2CResult sbs_read_status(BMSData* data) {
    BMSI2C* i2c = &g_bms_i2c;
    I2CResult res;
    
    res = read_word(i2c, SBS_CMD_VOLTAGE, &data->voltage);
    if(res != I2C_OK) return res;
    
    res = read_signed_word(i2c, SBS_CMD_CURRENT, &data->current);
    if(res != I2C_OK) return res;
    
    res = read_signed_word(i2c, SBS_CMD_AVERAGE_CURRENT, &data->average_current);
    if(res != I2C_OK) return res;
    
    res = read_word(i2c, SBS_CMD_TEMPERATURE, &data->temperature);
    if(res != I2C_OK) return res;
    
    res = read_byte(i2c, SBS_CMD_RELATIVE_STATE_OF_CHARGE, &data->relative_state_of_charge);
    if(res != I2C_OK) return res;
    
    res = read_byte(i2c, SBS_CMD_ABSOLUTE_STATE_OF_CHARGE, &data->absolute_state_of_charge);
    if(res != I2C_OK) return res;
    
    res = read_word(i2c, SBS_CMD_REMAINING_CAPACITY, &data->remaining_capacity);
    if(res != I2C_OK) return res;
    
    res = read_word(i2c, SBS_CMD_BATTERY_STATUS, &data->battery_status);
    if(res != I2C_OK) return res;
    
    res = read_word(i2c, SBS_CMD_CYCLE_COUNT, &data->cycle_count);
    if(res != I2C_OK) return res;
    
    res = read_byte(i2c, SBS_CMD_MAX_ERROR, &data->max_error);
    if(res != I2C_OK) return res;
    
    res = read_word(i2c, SBS_CMD_SPECIFICATION_INFO, &data->spec_info);
    
    return res;
}

I2CResult sbs_read_readonly(uint8_t address, BMSData* data) {
    if(!data || address < I2C_SCAN_FIRST_ADDRESS || address > I2C_SCAN_LAST_ADDRESS) {
        return I2C_ERROR_INVALID;
    }

    memset(data, 0, sizeof(BMSData));
    data->i2c_address = address;
    data->chip_type = BQ_CHIP_UNKNOWN;
    data->seal_state = SEAL_UNKNOWN;
    bms_i2c_init(&g_bms_i2c, address);

    I2CResult result = read_word(&g_bms_i2c, SBS_CMD_TEMPERATURE, &data->temperature);
    if(result != I2C_OK) return result;
    result = read_word(&g_bms_i2c, SBS_CMD_VOLTAGE, &data->voltage);
    if(result != I2C_OK) return result;
    result = read_signed_word(&g_bms_i2c, SBS_CMD_CURRENT, &data->current);
    if(result != I2C_OK) return result;
    uint16_t relative_state_of_charge;
    result = read_word(
        &g_bms_i2c, SBS_CMD_RELATIVE_STATE_OF_CHARGE, &relative_state_of_charge);
    if(result != I2C_OK) return result;
    if(relative_state_of_charge > UINT8_MAX) return I2C_ERROR_INVALID;
    data->relative_state_of_charge = (uint8_t)relative_state_of_charge;
    result = read_word(&g_bms_i2c, SBS_CMD_REMAINING_CAPACITY, &data->remaining_capacity);
    if(result != I2C_OK) return result;
    result = read_word(&g_bms_i2c, SBS_CMD_FULL_CHARGE_CAPACITY, &data->full_charge_capacity);
    if(result != I2C_OK) return result;
    result = read_word(&g_bms_i2c, SBS_CMD_BATTERY_STATUS, &data->battery_status);
    if(result != I2C_OK) return result;
    result = read_word(&g_bms_i2c, SBS_CMD_CYCLE_COUNT, &data->cycle_count);
    if(result != I2C_OK) return result;
    result = read_word(&g_bms_i2c, SBS_CMD_DESIGN_CAPACITY, &data->design_capacity);
    if(result != I2C_OK) return result;
    result = read_word(&g_bms_i2c, SBS_CMD_DESIGN_VOLTAGE, &data->design_voltage);
    if(result != I2C_OK) return result;
    result = read_word(&g_bms_i2c, SBS_CMD_SPECIFICATION_INFO, &data->spec_info);
    if(result != I2C_OK) return result;
    result = read_word(&g_bms_i2c, SBS_CMD_MANUFACTURE_DATE, &data->manufacture_date);
    if(result != I2C_OK) return result;
    result = read_word(&g_bms_i2c, SBS_CMD_SERIAL_NUMBER, &data->serial_number);
    if(result != I2C_OK) return result;
    result = read_string(
        &g_bms_i2c, SBS_CMD_MANUFACTURER_NAME, data->manufacturer, sizeof(data->manufacturer));
    if(result != I2C_OK) FURI_LOG_W(TAG, "Optional ManufacturerName read failed: %d", result);
    result = read_string(
        &g_bms_i2c, SBS_CMD_DEVICE_NAME, data->device_name, sizeof(data->device_name));
    if(result != I2C_OK) FURI_LOG_W(TAG, "Optional DeviceName read failed: %d", result);
    result = read_string(
        &g_bms_i2c,
        SBS_CMD_DEVICE_CHEMISTRY,
        data->device_chemistry,
        sizeof(data->device_chemistry));
    if(result != I2C_OK) FURI_LOG_W(TAG, "Optional DeviceChemistry read failed: %d", result);

    result = read_word(&g_bms_i2c, SBS_CMD_OPT_MFG_FUNCTION4, &data->cell_voltage[3]);
    if(result != I2C_OK) FURI_LOG_W(TAG, "Cell 4 voltage read failed: %d", result);
    result = read_word(&g_bms_i2c, SBS_CMD_OPT_MFG_FUNCTION3, &data->cell_voltage[2]);
    if(result != I2C_OK) FURI_LOG_W(TAG, "Cell 3 voltage read failed: %d", result);
    result = read_word(&g_bms_i2c, SBS_CMD_OPT_MFG_FUNCTION2, &data->cell_voltage[1]);
    if(result != I2C_OK) FURI_LOG_W(TAG, "Cell 2 voltage read failed: %d", result);
    result = read_word(&g_bms_i2c, SBS_CMD_OPT_MFG_FUNCTION1, &data->cell_voltage[0]);
    if(result != I2C_OK) FURI_LOG_W(TAG, "Cell 1 voltage read failed: %d", result);
    for(uint8_t index = 0; index < COUNT_OF(data->cell_voltage); index++) {
        if(data->cell_voltage[index] > 0) data->cell_count = index + 1;
    }
    uint8_t design_cell_count = (data->design_voltage + 1850U) / 3700U;
    if(design_cell_count >= 1 && design_cell_count <= COUNT_OF(data->cell_voltage) &&
       design_cell_count > data->cell_count) {
        data->cell_count = design_cell_count;
    }

    uint8_t sbs_version = (data->spec_info >> 4) & 0x0F;
    bms_i2c_set_pec(&g_bms_i2c, sbs_version >= 3);

    uint8_t dji_serial_length = 0;
    result = bms_i2c_read_block(
        &g_bms_i2c,
        DJI_CMD_BATTERY_SERIAL,
        (uint8_t*)data->dji_serial,
        &dji_serial_length,
        sizeof(data->dji_serial) - 1);
    if(result == I2C_OK) {
        data->dji_serial[dji_serial_length] = '\0';
    } else {
        FURI_LOG_W(TAG, "DJI serial read failed: %d", result);
    }

    if(data->temperature < 2000 || data->temperature > 4000 || data->voltage < 1000 ||
       data->voltage > 30000 || data->relative_state_of_charge > 100 ||
       data->remaining_capacity > data->full_charge_capacity * 2U) {
        FURI_LOG_W(TAG, "Device at 0x%02X returned invalid SBS values", address);
        return I2C_ERROR_INVALID;
    }

    FURI_LOG_I(
        TAG,
        "SBS 0x%02X %s %s: %u mV, %d mA, %u%%, %u/%u mAh, %u cycles",
        address,
        data->manufacturer,
        data->device_name,
        data->voltage,
        data->current,
        data->relative_state_of_charge,
        data->remaining_capacity,
        data->full_charge_capacity,
        data->cycle_count);
    return I2C_OK;
}

I2CResult sbs_read_bq40_extended(BMSData* data) {
    if(!data || data->i2c_address < I2C_SCAN_FIRST_ADDRESS ||
       data->i2c_address > I2C_SCAN_LAST_ADDRESS) {
        return I2C_ERROR_INVALID;
    }

    bms_i2c_init(&g_bms_i2c, data->i2c_address);
    uint8_t sbs_version = (data->spec_info >> 4) & 0x0F;
    bms_i2c_set_pec(&g_bms_i2c, sbs_version >= 3);

    uint8_t firmware_data[30];
    uint8_t firmware_length = 0;
    I2CResult result = read_bq40_mba(
        &g_bms_i2c,
        BQ40_SUBCMD_FIRMWARE_VERSION,
        firmware_data,
        &firmware_length,
        sizeof(firmware_data));
    if(result != I2C_OK || firmware_length < 6) return I2C_ERROR_INVALID;

    uint16_t device_number = ((uint16_t)firmware_data[0] << 8) | firmware_data[1];
    if(device_number != 0x4307) {
        FURI_LOG_W(TAG, "Unsupported BQ40 device number: 0x%04X", device_number);
        return I2C_ERROR_INVALID;
    }
    data->chip_type = BQ_CHIP_BQ40Z307;
    data->firmware_version = ((uint16_t)firmware_data[2] << 8) | firmware_data[3];
    data->firmware_build = ((uint16_t)firmware_data[4] << 8) | firmware_data[5];

    uint8_t operation_data[4];
    uint8_t operation_length = 0;
    result = read_bq40_mba(
        &g_bms_i2c,
        BQ40_SUBCMD_OPERATION_STATUS,
        operation_data,
        &operation_length,
        sizeof(operation_data));
    if(result != I2C_OK || operation_length != sizeof(operation_data)) return I2C_ERROR_INVALID;

    uint32_t operation_status = (uint32_t)operation_data[0] |
                                ((uint32_t)operation_data[1] << 8) |
                                ((uint32_t)operation_data[2] << 16) |
                                ((uint32_t)operation_data[3] << 24);
    uint8_t security_mode = (operation_status >> 8) & 0x03;
    data->seal_state = security_mode == 3 ? SEAL_SEALED :
                       security_mode == 2 ? SEAL_UNSEALED :
                       security_mode == 1 ? SEAL_FULL : SEAL_UNKNOWN;
    FURI_LOG_I(
        TAG,
        "BQ40 device=0x%04X firmware=%04X build=%u OperationStatus=0x%08lX SEC=%u",
        device_number,
        data->firmware_version,
        data->firmware_build,
        operation_status,
        security_mode);
    return I2C_OK;
}

I2CResult sbs_read_bq30_extended(BMSData* data) {
    if(!data || data->i2c_address < I2C_SCAN_FIRST_ADDRESS ||
       data->i2c_address > I2C_SCAN_LAST_ADDRESS) {
        return I2C_ERROR_INVALID;
    }

    bms_i2c_init(&g_bms_i2c, data->i2c_address);
    bms_i2c_set_pec(&g_bms_i2c, true);

    I2CResult result = bms_i2c_write_word(
        &g_bms_i2c, SBS_CMD_MANUFACTURER_ACCESS, BQ30_SUBCMD_FIRMWARE_VERSION);
    uint8_t firmware_data[16];
    uint8_t firmware_length = 0;
    if(result == I2C_OK) {
        result = bms_i2c_read_block(
            &g_bms_i2c,
            SBS_CMD_MANUFACTURER_DATA,
            firmware_data,
            &firmware_length,
            sizeof(firmware_data));
    }
    if(result != I2C_OK || (firmware_length != 11 && firmware_length != 13)) {
        // The upstream BQ30 tool uses this malformed sequence for sealed discovery.
        for(uint8_t attempt = 0; attempt < 3; attempt++) {
            bms_i2c_write_byte(&g_bms_i2c, 0x22, 0x3E);
            bms_i2c_write_byte(&g_bms_i2c, 0x20, 0x3E);
            bms_i2c_write_byte(&g_bms_i2c, 0x22, 0x3E);
            furi_delay_ms(350);
            firmware_length = 0;
            result = bms_i2c_read_block(
                &g_bms_i2c,
                BQ30_CMD_MANUFACTURER_INPUT,
                firmware_data,
                &firmware_length,
                sizeof(firmware_data));
            if(result == I2C_OK && (firmware_length == 11 || firmware_length == 13)) break;
            furi_delay_ms(350);
        }
    }
    if(result != I2C_OK || (firmware_length != 11 && firmware_length != 13)) {
        return I2C_ERROR_INVALID;
    }

    uint16_t device_number = ((uint16_t)firmware_data[0] << 8) | firmware_data[1];
    if(device_number != 0x0550 && device_number != 0x0500 && device_number != 0x0554) {
        FURI_LOG_W(TAG, "Unsupported BQ30 device number: 0x%04X", device_number);
        return I2C_ERROR_INVALID;
    }
    data->chip_type = device_number == 0x0500 ? BQ_CHIP_BQ30Z50 :
                      device_number == 0x0554 ? BQ_CHIP_BQ30Z554 : BQ_CHIP_BQ30Z55;
    data->firmware_version = ((uint16_t)firmware_data[2] << 8) | firmware_data[3];
    data->firmware_build = ((uint16_t)firmware_data[4] << 8) | firmware_data[5];

    result = bms_i2c_write_word(
        &g_bms_i2c, SBS_CMD_MANUFACTURER_ACCESS, BQ30_SUBCMD_OPERATION_STATUS);
    if(result != I2C_OK) return result;
    furi_delay_ms(5);

    uint8_t operation_data[4];
    uint8_t operation_length = 0;
    result = bms_i2c_read_block(
        &g_bms_i2c,
        SBS_CMD_MANUFACTURER_DATA,
        operation_data,
        &operation_length,
        sizeof(operation_data));
    if(result != I2C_OK || operation_length != sizeof(operation_data)) return I2C_ERROR_INVALID;

    uint32_t operation_status = (uint32_t)operation_data[0] |
                                ((uint32_t)operation_data[1] << 8) |
                                ((uint32_t)operation_data[2] << 16) |
                                ((uint32_t)operation_data[3] << 24);
    uint8_t security_mode = (operation_status >> 8) & 0x03;
    data->seal_state = security_mode == 3 ? SEAL_SEALED :
                       security_mode == 2 ? SEAL_UNSEALED :
                       security_mode == 1 ? SEAL_FULL : SEAL_UNKNOWN;
    FURI_LOG_I(
        TAG,
        "BQ30 device=0x%04X firmware=%04X build=%u OperationStatus=0x%08lX SEC=%u",
        device_number,
        data->firmware_version,
        data->firmware_build,
        operation_status,
        security_mode);
    return I2C_OK;
}

I2CResult sbs_detect_chip(BQChipType* chip_type, uint16_t* fw_version) {
    BMSI2C* i2c = &g_bms_i2c;
    uint8_t buf[32];
    uint8_t len;
    FURI_LOG_I(TAG, "Starting chip detection...");
    
    FURI_LOG_I(TAG, "Scanning I2C bus...");
    for(uint8_t addr = 0x08; addr <= 0x77; addr++) {
        furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
        bool ready = furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_external, addr << 1, 5);
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        if(ready) {
            FURI_LOG_I(TAG, "Found device at 0x%02X", addr);
        }
    }
    
    uint8_t test_addrs[] = {0x0B, 0x16, 0x0A, 0x12};
    bool found = false;
    for(uint8_t i = 0; i < sizeof(test_addrs); i++) {
        i2c->dev_addr = test_addrs[i];
        FURI_LOG_I(TAG, "Trying address 0x%02X...", test_addrs[i]);
        
        I2CResult res = read_manufacturer_access(i2c, 0x01, buf, &len);
        FURI_LOG_I(TAG, "  result: %d, len: %d", res, len);
        
        if(res == I2C_OK && len >= 2) {
            FURI_LOG_I(TAG, "Found device at 0x%02X!", test_addrs[i]);
            found = true;
            break;
        }
    }
    if(!found) {
        FURI_LOG_I(TAG, "No device found, using default 0x0B");
        i2c->dev_addr = 0x0B;
    }
    
    I2CResult res = read_manufacturer_access(i2c, 0x01, buf, &len);
    FURI_LOG_I(TAG, "Device type read result: %d, len: %d", res, len);
    if(res != I2C_OK) {
        FURI_LOG_E(TAG, "Device type read failed!");
        return res;
    }
    
    if(len >= 2) {
        uint16_t device_type = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
        *chip_type = (BQChipType)device_type;
        FURI_LOG_I(TAG, "Device type: 0x%04X (%s)", device_type, bq_chip_name(*chip_type));
    } else {
        *chip_type = BQ_CHIP_UNKNOWN;
        FURI_LOG_W(TAG, "Device type: insufficient data");
    }
    
    res = read_manufacturer_access(i2c, 0x02, buf, &len);
    if(res != I2C_OK) {
        FURI_LOG_W(TAG, "FW version read failed");
    } else if(len >= 2) {
        *fw_version = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
        FURI_LOG_I(TAG, "FW version: 0x%04X", *fw_version);
    }
    
    return I2C_OK;
}

const char* bq_chip_name(BQChipType chip) {
    switch(chip) {
        case BQ_CHIP_BQ30Z55: return "BQ30z55";
        case BQ_CHIP_BQ30Z554: return "BQ30z554";
        case BQ_CHIP_BQ30Z50: return "BQ30z50";
        case BQ_CHIP_BQ40Z307: return "BQ40z307 (BQ9003)";
        case BQ_CHIP_BQ40Z50: return "BQ40z50";
        case BQ_CHIP_BQ40Z60: return "BQ40z60";
        case BQ_CHIP_BQ40Z80: return "BQ40z80";
        default: return "Unknown";
    }
}

const char* seal_state_name(SealState state) {
    switch(state) {
        case SEAL_FULL: return "Full Access";
        case SEAL_UNSEALED: return "Unsealed";
        case SEAL_SEALED: return "Sealed";
        case SEAL_UNKNOWN: return "Unknown";
        default: return "Unknown";
    }
}

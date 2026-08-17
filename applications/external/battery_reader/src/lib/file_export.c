#include "../file_export.h"
#include "../sbs_protocol.h"
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "FileExport"
#define EXPORT_BUFFER_SIZE 1024

static const char* battery_status_error_name(uint16_t status) {
    static const char* const names[] = {
        "OK",
        "Busy",
        "Reserved command",
        "Unsupported command",
        "Access denied",
        "Overflow/underflow",
        "Bad size",
        "Unknown error",
    };
    uint8_t error = status & 0x0F;
    return error < COUNT_OF(names) ? names[error] : "Reserved error";
}

bool save_bms_data_to_sd(BMSData* data) {
    if(!data) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;

    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    char file_serial[sizeof(data->dji_serial)];
    size_t file_serial_length = 0;
    for(size_t index = 0;
        data->dji_serial[index] != '\0' && file_serial_length < sizeof(file_serial) - 1;
        index++) {
        if(isalnum((unsigned char)data->dji_serial[index])) {
            file_serial[file_serial_length++] = data->dji_serial[index];
        }
    }
    if(file_serial_length == 0) {
        memcpy(file_serial, "BMS", 3);
        file_serial_length = 3;
    }
    file_serial[file_serial_length] = '\0';

    char filepath[128];
    snprintf(
        filepath,
        sizeof(filepath),
        "/ext/bms_data_%04u%02u%02u_%02u%02u%02u_%s.txt",
        now.year,
        now.month,
        now.day,
        now.hour,
        now.minute,
        now.second,
        file_serial);
    FURI_LOG_I(TAG, "Saving %s", filepath);

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    if(!storage_file_open(file, filepath, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Failed to open export file");
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    uint8_t manufacture_day = data->manufacture_date & 0x1F;
    uint8_t manufacture_month = (data->manufacture_date >> 5) & 0x0F;
    uint16_t manufacture_year = 1980 + ((data->manufacture_date >> 9) & 0x7F);
    bool manufacture_date_valid = manufacture_day >= 1 && manufacture_day <= 31 &&
                                  manufacture_month >= 1 && manufacture_month <= 12;

    char manufacture_date[40];
    if(manufacture_date_valid) {
        snprintf(
            manufacture_date,
            sizeof(manufacture_date),
            "%04u-%02u-%02u (raw 0x%04X)",
            manufacture_year,
            manufacture_month,
            manufacture_day,
            data->manufacture_date);
    } else {
        snprintf(manufacture_date, sizeof(manufacture_date), "invalid (raw 0x%04X)", data->manufacture_date);
    }

    char firmware[24];
    if(data->firmware_version != 0) {
        snprintf(firmware, sizeof(firmware), "0x%04X", data->firmware_version);
    } else {
        snprintf(firmware, sizeof(firmware), "not read");
    }

    char cell_lines[128];
    size_t cell_lines_length = 0;
    if(data->cell_count == 0) {
        snprintf(cell_lines, sizeof(cell_lines), "Cell Voltages: not read\n");
    } else {
        for(uint8_t index = 0; index < data->cell_count; index++) {
            int added = snprintf(
                cell_lines + cell_lines_length,
                sizeof(cell_lines) - cell_lines_length,
                "Cell %u Voltage: %u mV\n",
                index + 1,
                data->cell_voltage[index]);
            if(added < 0 || (size_t)added >= sizeof(cell_lines) - cell_lines_length) break;
            cell_lines_length += (size_t)added;
        }
    }

    char* buffer = malloc(EXPORT_BUFFER_SIZE);
    if(!buffer) {
        FURI_LOG_E(TAG, "Failed to allocate export buffer");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    int len = snprintf(
        buffer,
        EXPORT_BUFFER_SIZE,
        "BMS Reader Export\n"
        "==================\n"
        "Saved: %04u-%02u-%02u %02u:%02u:%02u\n"
        "I2C Address: 0x%02X\n"
        "Chip: %s\n"
        "Firmware: %s\n"
        "Firmware Build: %u\n"
        "Manufacturer: %s\n"
        "Device: %s\n"
        "Chemistry: %s\n"
        "DJI Serial: %s\n"
        "Manufacture Date: %s\n"
        "Design Capacity: %u mAh\n"
        "Design Voltage: %u mV\n"
        "Full Charge Capacity: %u mAh\n"
        "Voltage: %u mV\n"
        "%s"
        "Current: %d mA\n"
        "Temperature: %d.%d C\n"
        "Relative SoC: %u %%\n"
        "Remaining Capacity: %u mAh\n"
        "Cycle Count: %u\n"
        "Specification Info: 0x%04X\n"
        "Battery Status: 0x%04X\n"
        "Status Error: %s (%u)\n"
        "Status Flags: OCA=%u TCA=%u OTA=%u TDA=%u RCA=%u RTA=%u INIT=%u DSG=%u FC=%u FD=%u\n"
        "Seal State: %s\n",
        now.year,
        now.month,
        now.day,
        now.hour,
        now.minute,
        now.second,
        data->i2c_address,
        bq_chip_name(data->chip_type),
        firmware,
        data->firmware_build,
        data->manufacturer[0] ? data->manufacturer : "not read",
        data->device_name[0] ? data->device_name : "not read",
        data->device_chemistry[0] ? data->device_chemistry : "not read",
        data->dji_serial[0] ? data->dji_serial : "not read",
        manufacture_date,
        data->design_capacity,
        data->design_voltage,
        data->full_charge_capacity,
        data->voltage,
        cell_lines,
        data->current,
        (data->temperature - 2731) / 10,
        abs((int)data->temperature - 2731) % 10,
        data->relative_state_of_charge,
        data->remaining_capacity,
        data->cycle_count,
        data->spec_info,
        data->battery_status,
        battery_status_error_name(data->battery_status),
        data->battery_status & 0x0F,
        !!(data->battery_status & BS_FLAG_OVER_CHARGED_ALARM),
        !!(data->battery_status & BS_FLAG_TERMINATE_CHARGE_ALARM),
        !!(data->battery_status & BS_FLAG_OVERTEMPERATURE_ALARM),
        !!(data->battery_status & BS_FLAG_TERMINATE_DISCHARGE_ALARM),
        !!(data->battery_status & BS_FLAG_REMAINING_CAPACITY_ALARM),
        !!(data->battery_status & BS_FLAG_REMAINING_TIME_ALARM),
        !!(data->battery_status & BS_FLAG_INITIALIZED),
        !!(data->battery_status & BS_FLAG_DISCHARGING),
        !!(data->battery_status & BS_FLAG_FULLY_CHARGED),
        !!(data->battery_status & BS_FLAG_FULLY_DISCHARGED),
        seal_state_name((SealState)data->seal_state));

    size_t output_len = len < 0 ? 0 : (size_t)len;
    if(output_len >= EXPORT_BUFFER_SIZE) output_len = EXPORT_BUFFER_SIZE - 1;
    size_t written = storage_file_write(file, buffer, output_len);
    free(buffer);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    bool success = written == output_len && output_len > 0;
    FURI_LOG_I(TAG, "Export %s, bytes=%u", success ? "saved" : "failed", (unsigned)written);
    return success;
}

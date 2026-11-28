#include "tpms_ford_diag.h"
#include "tpms_encoder.h"

#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define TPMS_FORD_DIAG_FOLDER EXT_PATH("apps_data/tpms_editor")

static uint32_t tpms_ford_diag_last_signature = 0U;
static bool tpms_ford_diag_last_signature_valid = false;

static uint32_t tpms_ford_diag_hash(
    uint32_t id,
    uint32_t frequency_hz,
    uint8_t radio_preset,
    uint8_t wake_profile,
    bool wake_profile_valid,
    const uint8_t* raw_payload,
    size_t raw_payload_size) {
    uint32_t hash = UINT32_C(2166136261);
    const uint8_t* id_bytes = (const uint8_t*)&id;
    for(size_t i = 0U; i < sizeof(id); i++) {
        hash ^= id_bytes[i];
        hash *= UINT32_C(16777619);
    }
    const uint8_t* freq_bytes = (const uint8_t*)&frequency_hz;
    for(size_t i = 0U; i < sizeof(frequency_hz); i++) {
        hash ^= freq_bytes[i];
        hash *= UINT32_C(16777619);
    }
    hash ^= radio_preset;
    hash *= UINT32_C(16777619);
    hash ^= wake_profile_valid ? wake_profile : 0U;
    hash *= UINT32_C(16777619);
    for(size_t i = 0U; i < raw_payload_size; i++) {
        hash ^= raw_payload[i];
        hash *= UINT32_C(16777619);
    }
    return hash ? hash : 1U;
}

void tpms_ford_diag_reset_session(void) {
    tpms_ford_diag_last_signature = 0U;
    tpms_ford_diag_last_signature_valid = false;
}

bool tpms_ford_diag_append(
    uint32_t id,
    float pressure_kpa,
    float temperature_c,
    uint32_t frequency_hz,
    uint8_t radio_preset,
    float rssi_dbm,
    uint8_t wake_profile,
    bool wake_profile_valid,
    const uint8_t* raw_payload,
    size_t raw_payload_size) {
    if(!raw_payload || raw_payload_size < 8U) return false;

    const uint32_t signature = tpms_ford_diag_hash(
        id,
        frequency_hz,
        radio_preset,
        wake_profile,
        wake_profile_valid,
        raw_payload,
        8U);
    if(tpms_ford_diag_last_signature_valid && signature == tpms_ford_diag_last_signature) {
        return true;
    }

    const uint8_t tt = raw_payload[5];
    const uint8_t ff = raw_payload[6];
    const bool temp_valid = ((tt & 0x80U) == 0U) && ((ff & 0x0FU) != 0x0BU);
    const int temp_calc_c = (int)tt - 56;
    const char* tt_mode = (tt & 0x80U) ? "SPECIAL_MSB" :
                          ((ff & 0x0FU) == 0x0BU) ? "FF_xB_NO_TEMP" :
                          (temp_calc_c < -40 ? "TEMP_LOW?" : "TEMP");
    const char* rx_preset = tpms_radio_preset_name(radio_preset);
    const char* wake = wake_profile_valid ? tpms_wake_profile_short_name(wake_profile) : "-";

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char raw_text[3U * 8U];
    size_t raw_pos = 0U;
    raw_text[0] = '\0';
    for(size_t i = 0U; i < 8U && raw_pos + 4U <= sizeof(raw_text); i++) {
        const int written = snprintf(
            raw_text + raw_pos,
            sizeof(raw_text) - raw_pos,
            i == 0U ? "%02X" : " %02X",
            raw_payload[i]);
        if(written <= 0) break;
        raw_pos += (size_t)written;
    }

    char observed_temp[16];
    if(temp_valid) snprintf(observed_temp, sizeof(observed_temp), "%.1f", (double)temperature_c);
    else observed_temp[0] = '\0';

    char line[400];
    const int line_len = snprintf(
        line,
        sizeof(line),
        "%04u-%02u-%02u,%02u:%02u:%02u,%08lX,%lu,%s,%.0f,%.2f,%s,%02X,%02X,%s,%u,%d,%u,%u,%u,%u,%u,%u,%u,%u,%s,%s\r\n",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        (unsigned)dt.second,
        (unsigned long)id,
        (unsigned long)frequency_hz,
        rx_preset,
        (double)rssi_dbm,
        (double)pressure_kpa,
        observed_temp,
        tt,
        ff,
        tt_mode,
        temp_valid ? 1U : 0U,
        temp_calc_c,
        (ff & 0x80U) ? 1U : 0U,
        (ff & 0x40U) ? 1U : 0U,
        (ff & 0x20U) ? 1U : 0U,
        (ff & 0x10U) ? 1U : 0U,
        (ff & 0x08U) ? 1U : 0U,
        (ff & 0x04U) ? 1U : 0U,
        (ff & 0x02U) ? 1U : 0U,
        (ff & 0x01U) ? 1U : 0U,
        wake,
        raw_text);
    if(line_len <= 0 || (size_t)line_len >= sizeof(line)) return false;

    char path[112];
    snprintf(
        path,
        sizeof(path),
        TPMS_FORD_DIAG_FOLDER "/FORD_DIAG_%04u-%02u-%02u.csv",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    storage_common_mkdir(storage, TPMS_FORD_DIAG_FOLDER);

    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(file && storage_file_open(file, path, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        if(storage_file_size(file) == 0U) {
            static const char header[] =
                "Date,Time,ID,FrequencyHz,RxPreset,RSSI_dBm,PressureKPa,TemperatureC,TT,FF,TTMode,TTValid,TTCalcC,Bit80,Bit40_Moving,Bit20_Pressure9,Bit10,Bit08_Learn,Bit04,Bit02,Bit01,Wake,Raw\r\n";
            ok = storage_file_write(file, header, sizeof(header) - 1U) == sizeof(header) - 1U;
        } else {
            ok = true;
        }
        if(ok) ok = storage_file_write(file, line, (size_t)line_len) == (size_t)line_len;
        if(ok) ok = storage_file_sync(file);
        storage_file_close(file);
    }
    if(file) storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(ok) {
        tpms_ford_diag_last_signature = signature;
        tpms_ford_diag_last_signature_valid = true;
    }
    return ok;
}

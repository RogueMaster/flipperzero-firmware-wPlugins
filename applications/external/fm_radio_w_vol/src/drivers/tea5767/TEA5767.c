/**
 * @file TEA5767.c
 * @author Coolshrimp - CoolshrimpModz.com
 * @brief Library for controlling the TEA5767 FM radio chip.
 * @version 0.1
 * @date 2023-09-29
 * 
 * @copyright GPLv3
 */
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <stdio.h> // Include necessary libraries
#include <string.h>

#include "TEA5767.h"

#define TIMEOUT_MS 100

static bool tea5767_snc_enabled = false;
static bool tea5767_dtc_75us_enabled = false;
static bool tea5767_hcc_enabled = false;
static bool tea5767_force_mono_enabled = false;
static uint8_t tea5767_last_write_regs[5];
static bool tea5767_last_write_valid = false;

// Helper function to acquire I2C
static bool acquire_i2c() {
    // Acquire I2C and check for device readiness
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    return furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_external, TEA5767_ADR, 5);
}

static void release_i2c() {
    // Release I2C
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
}

bool tea5767_read_registers(uint8_t* buffer) {
    if(buffer == NULL) return false;
    bool result = acquire_i2c();
    if(result) {
        result =
            furi_hal_i2c_rx(&furi_hal_i2c_handle_external, TEA5767_ADR, buffer, 5, TIMEOUT_MS);
    }
    release_i2c();
    return result;
}

bool tea5767_write_registers(uint8_t* buffer) {
    bool result = false;
    if(buffer == NULL) return false;
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    result = furi_hal_i2c_tx(&furi_hal_i2c_handle_external, TEA5767_ADR, buffer, 5, TIMEOUT_MS);
    if(result) {
        memcpy(tea5767_last_write_regs, buffer, 5);
        tea5767_last_write_valid = true;
    }
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return result;
}

bool tea5767_init(uint8_t* buffer) {
    bool result = false;

    buffer[0] = 0x00;
    buffer[1] = 0x00;
    buffer[2] = 0xB0 | (tea5767_force_mono_enabled ? REG_3_MS : 0);
    buffer[3] = REG_4_XTAL | (tea5767_hcc_enabled ? REG_4_HCC : 0) |
                (tea5767_snc_enabled ? REG_4_SNC : 0);
    buffer[4] = (tea5767_dtc_75us_enabled ? REG_5_DTC : 0);

    result = tea5767_write_registers(buffer);
    return result;
}

void tea5767_set_snc_enabled(bool enabled) {
    tea5767_snc_enabled = enabled;
}

bool tea5767_set_snc(bool enabled) {
    tea5767_snc_enabled = enabled;

    // IMPORTANT: TEA5767 readback register layout is not identical to write register layout.
    // Avoid read-modify-write, which can corrupt PLL/tuning bits and stop audio.
    if(!tea5767_last_write_valid) {
        // Fallback: reapply current frequency with the new SNC setting.
        float freq = tea5767_GetFreq();
        if(freq < 0.0f) return false;
        tea5767_SetFreqMHz(freq);
        return true;
    }

    uint8_t buffer[5];
    memcpy(buffer, tea5767_last_write_regs, 5);

    if(enabled) {
        buffer[REG_4] |= REG_4_SNC;
    } else {
        buffer[REG_4] &= (uint8_t)~REG_4_SNC;
    }

    return tea5767_write_registers(buffer);
}

void tea5767_set_deemphasis_75us_enabled(bool enabled) {
    tea5767_dtc_75us_enabled = enabled;
}

bool tea5767_set_deemphasis_75us(bool enabled) {
    tea5767_dtc_75us_enabled = enabled;

    if(!tea5767_last_write_valid) {
        float freq = tea5767_GetFreq();
        if(freq < 0.0f) return false;
        tea5767_SetFreqMHz(freq);
        return true;
    }

    uint8_t buffer[5];
    memcpy(buffer, tea5767_last_write_regs, 5);

    if(enabled) {
        buffer[REG_5] |= REG_5_DTC;
    } else {
        buffer[REG_5] &= (uint8_t)~REG_5_DTC;
    }

    return tea5767_write_registers(buffer);
}

void tea5767_set_high_cut_enabled(bool enabled) {
    tea5767_hcc_enabled = enabled;
}

bool tea5767_set_high_cut(bool enabled) {
    tea5767_hcc_enabled = enabled;

    if(!tea5767_last_write_valid) {
        float freq = tea5767_GetFreq();
        if(freq < 0.0f) return false;
        tea5767_SetFreqMHz(freq);
        return true;
    }

    uint8_t buffer[5];
    memcpy(buffer, tea5767_last_write_regs, 5);
    if(enabled) {
        buffer[REG_4] |= REG_4_HCC;
    } else {
        buffer[REG_4] &= (uint8_t)~REG_4_HCC;
    }
    return tea5767_write_registers(buffer);
}

void tea5767_set_force_mono_enabled(bool enabled) {
    tea5767_force_mono_enabled = enabled;
}

bool tea5767_set_force_mono(bool enabled) {
    tea5767_force_mono_enabled = enabled;

    if(!tea5767_last_write_valid) {
        float freq = tea5767_GetFreq();
        if(freq < 0.0f) return false;
        tea5767_SetFreqMHz(freq);
        return true;
    }

    uint8_t buffer[5];
    memcpy(buffer, tea5767_last_write_regs, 5);
    if(enabled) {
        buffer[REG_3] |= REG_3_MS;
    } else {
        buffer[REG_3] &= (uint8_t)~REG_3_MS;
    }
    return tea5767_write_registers(buffer);
}

bool tea5767_seek(uint8_t* buffer, bool seek_up) {
    bool result = false;
    if(buffer == NULL) {
        return false;
    }
    buffer[REG_1] |= REG_1_SM; // Set the Search Mode (SM) bit to initiate seek
    if(seek_up) {
        buffer[REG_3] |= REG_3_SUD; // Set Search Up (SUD) bit
    } else {
        buffer[REG_3] &= ~REG_3_SUD; // Set Search Down (SUD) bit
    }
    // Use low search stop level to catch weaker stations.
    buffer[REG_3] &= (uint8_t)~REG_3_SSL;
    buffer[REG_3] |= REG_3_SSL_LOW;
    if(tea5767_force_mono_enabled) {
        buffer[REG_3] |= REG_3_MS;
    } else {
        buffer[REG_3] &= (uint8_t)~REG_3_MS;
    }
    // PLLREF must stay 0 when XTAL=1 (32.768 kHz crystal).
    // Table 16: PLLREF=1 + XTAL=1 = not allowed.
    buffer[REG_5] &= (uint8_t)~REG_5_PLLREF;
    // Write the updated register values to the TEA5767
    result = tea5767_write_registers(buffer);
    return result;
}

bool tea5767_get_frequency(uint8_t* buffer, int* value) {
    bool result = false;
    uint16_t frequency;
    if(buffer == NULL || value == NULL) return false; //NULL check
    if(tea5767_read_registers(buffer)) {
        frequency = ((buffer[REG_1] & REG_1_PLL) << 8) | buffer[1];
        *value = (int)(((uint32_t)frequency * (uint32_t)QUARTZ / 4U - FILTER + 5000U) / 10000U);
        result = true;
    }
    return result;
}

bool tea5767_set_frequency(uint8_t* buffer, int value) {
    bool result = false;
    if(buffer == NULL) {
        return false;
    }

    // Auto-select band limits based on target frequency.
    // value is in 10kHz units (e.g. 88.1MHz => 8810).
    // JP band: 76-90MHz (REG_4_BL = 1), EU/US band: 87.5-108MHz (REG_4_BL = 0).
    if(value < 8750) {
        buffer[REG_4] |= REG_4_BL;
    } else {
        buffer[REG_4] &= ~REG_4_BL;
    }

    uint32_t pll_word = (4U * ((uint32_t)value * 10000U + FILTER) + (QUARTZ / 2U)) / QUARTZ;
    uint16_t frequency = (uint16_t)pll_word;
    buffer[REG_1] =
        ((buffer[0] & ~REG_1_PLL) |
         ((frequency >> 8) & REG_1_PLL)); // Set the upper 8 bits of the PLL word
    buffer[REG_2] = frequency & REG_2_PLL; // Set the lower 8 bits of the PLL word
    if(tea5767_force_mono_enabled) {
        buffer[REG_3] |= REG_3_MS;
    } else {
        buffer[REG_3] &= (uint8_t)~REG_3_MS;
    }
    // PLLREF must stay 0 when XTAL=1 (32.768 kHz crystal).
    // Table 16: PLLREF=1 + XTAL=1 = not allowed.
    buffer[REG_5] &= (uint8_t)~REG_5_PLLREF;
    result = tea5767_write_registers(buffer);
    return result;
}

bool tea5767_get_radio_info(uint8_t* buffer, struct RADIO_INFO* info) {
    bool result = false;
    int frequency_khz;

    // Error handling: Check if buffer and info are not NULL
    if(buffer && info && tea5767_read_registers(buffer)) {
        info->ready = (buffer[REG_1] & 0x80U) != 0U;
        info->bandLimit = (buffer[REG_1] & 0x40U) != 0U;
        info->ifCounter = buffer[REG_3] & 0x7FU;

        // TEA5767 read byte 3 (index 2) bit 7 = stereo indication (1=stereo, 0=mono).
        if(tea5767_force_mono_enabled) {
            info->stereo = false;
        } else {
            info->stereo = (buffer[REG_3] & 0x80) ? true : false;
        }

        info->signalLevel = buffer[REG_4] >> 4;

        // Determine signal quality based on signal level
        if(info->signalLevel >= 0 && info->signalLevel <= 3) {
            strncpy(info->signalQuality, "Poor", sizeof(info->signalQuality));
        } else if(info->signalLevel >= 4 && info->signalLevel <= 7) {
            strncpy(info->signalQuality, "Fair", sizeof(info->signalQuality));
        } else if(info->signalLevel >= 8 && info->signalLevel <= 11) {
            strncpy(info->signalQuality, "Good", sizeof(info->signalQuality));
        } else if(info->signalLevel >= 12 && info->signalLevel <= 15) {
            strncpy(info->signalQuality, "Excellent", sizeof(info->signalQuality));
        } else {
            strncpy(info->signalQuality, "Unknown", sizeof(info->signalQuality));
        }

        // Now get the frequency
        if(tea5767_get_frequency(buffer, &frequency_khz)) {
            info->frequency = frequency_khz / 100.0f; // Convert kHz to MHz
            result = true; // Only return true if both read_registers and get_frequency succeeded
        }

        // Mute status comes from the WRITE register cache, not READ registers.
        // READ byte 1 bit 7 = Ready Flag (RF), not MUTE.
        info->muted = tea5767_last_write_valid && (tea5767_last_write_regs[REG_1] & REG_1_MUTE);
    }
    return result;
}

void tea5767_seekFrom10kHz(uint32_t freq_10khz, bool seek_up) {
    uint8_t buffer[5];
    if(tea5767_init(buffer)) {
        tea5767_set_frequency(buffer, (int)freq_10khz);
        tea5767_seek(buffer, seek_up);
    }
}

void tea5767_SetFreqMHz(float freq_mhz) {
    uint8_t buffer[5];
    if(tea5767_init(buffer)) {
        int freq_khz = (int)(freq_mhz * 100.0); // Convert MHz to kHz
        tea5767_set_frequency(buffer, freq_khz);
    }
}

float tea5767_GetFreq() {
    uint8_t buffer[5];
    int value;
    if(tea5767_get_frequency(buffer, &value)) {
        return value / 100.0; // Convert to MHz
    }
    return -1; // Error
}

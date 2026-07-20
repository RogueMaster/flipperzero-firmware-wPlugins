/**
 * @file Si4703.c
 * @author Coolshrimp - CoolshrimpModz.com
 * @brief Library for controlling the Si4703 FM radio chip.
 * @version 1.0
 * @date 2026-02-02
 * 
 * @copyright GPLv3
 */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <stdio.h>

#include "Si4703.h"

#define TIMEOUT_MS 100
#define SI4703_I2C_ADDR_DEFAULT 0x20  // 8-bit write address (0x10 << 1)
#define SI4703_I2C_ADDR_ALT 0x22     // Alternative address (0x11 << 1) for some modules

// Track detected I2C address
static uint8_t si4703_i2c_addr = SI4703_I2C_ADDR_DEFAULT;

// Si4703 uses GPIO Pin 4 (PA4) for reset
static const GpioPin si4703_rst_pin = {.port = GPIOA, .pin = LL_GPIO_PIN_4}; // Pin 4 (PA4)

// Track if powerup sequence has been performed
static bool si4703_powered_up = false;

typedef enum {
    Si4703OpNone = 0,
    Si4703OpTuneWaitSet,
    Si4703OpTuneWaitClear,
    Si4703OpSeekWaitSet,
    Si4703OpSeekWaitClear,
} Si4703Op;

static Si4703Op si4703_op = Si4703OpNone;
static uint32_t si4703_op_start_tick = 0;

static const uint32_t si4703_tune_timeout_ms = 2000;
static const uint32_t si4703_seek_timeout_ms = 8000;

static bool si4703_ids_are_valid(const uint16_t* registers) {
    if(!registers) return false;
    const uint16_t device_id = registers[SI4703_DEVICEID];
    const uint16_t chip_id = registers[SI4703_CHIPID];
    return !((device_id == 0x0000 && chip_id == 0x0000) ||
             (device_id == 0xFFFF && chip_id == 0xFFFF));
}

// Reset powerup state (call when chip is disconnected)
void si4703_reset_state(void) {
    si4703_powered_up = false;
    si4703_op = Si4703OpNone;
    FURI_LOG_I("Si4703", "State reset - chip disconnected");
}

bool si4703_is_busy(void) {
    return si4703_op != Si4703OpNone;
}

static bool si4703_calculate_frequency_khz_from_regs(const uint16_t* registers, int* freq_khz) {
    if(!registers || !freq_khz) return false;

    // Read channel from READCHAN register (0x0B)
    uint16_t channel = registers[SI4703_READCHAN] & 0x03FF; // 10-bit channel value

    // Get band setting from SYSCONFIG2
    uint8_t band = (registers[SI4703_SYSCONFIG2] & SI4703_SYS2_BAND) >> 6;

    // Get spacing setting
    uint8_t spacing = (registers[SI4703_SYSCONFIG2] & SI4703_SYS2_SPACE) >> 4;

    // Calculate base frequency based on band
    int base_freq = 8750; // 87.5 MHz in units of 10 kHz
    if(band == SI4703_BAND_WIDE || band == SI4703_BAND_JAPAN) {
        base_freq = 7600; // 76 MHz
    }

    // Calculate spacing in 10 kHz units
    int spacing_val = 20; // 200 kHz default
    if(spacing == SI4703_SPACE_100KHZ) spacing_val = 10;
    else if(spacing == SI4703_SPACE_50KHZ) spacing_val = 5;

    // Calculate frequency: base + (channel * spacing)
    *freq_khz = (base_freq + (channel * spacing_val)) * 10; // Convert to kHz
    return true;
}

static void si4703_service_after_read(uint16_t* registers) {
    if(!registers) return;
    if(si4703_op == Si4703OpNone) return;

    const uint32_t now = furi_get_tick();
    const uint32_t elapsed = now - si4703_op_start_tick;
    const bool is_seek =
        (si4703_op == Si4703OpSeekWaitSet || si4703_op == Si4703OpSeekWaitClear);
    const bool waiting_for_clear =
        (si4703_op == Si4703OpTuneWaitClear || si4703_op == Si4703OpSeekWaitClear);
    const uint32_t timeout = is_seek ? si4703_seek_timeout_ms : si4703_tune_timeout_ms;

    const bool stc = (registers[SI4703_STATUSRSSI] & SI4703_STAT_STC) != 0;

    // After clearing TUNE/SEEK, the datasheet requires waiting for STC to clear
    // before another operation may begin.
    if(waiting_for_clear) {
        if(!stc) {
            FURI_LOG_I("Si4703", "STC cleared; operation complete");
            si4703_op = Si4703OpNone;
        } else if(elapsed >= timeout) {
            FURI_LOG_W("Si4703", "Timed out waiting for STC to clear");
            si4703_op = Si4703OpNone;
        }
        return;
    }

    if(!stc) {
        if(elapsed < timeout) return;
        FURI_LOG_W("Si4703",
            "%s timeout: DEV=0x%04X CHIP=0x%04X PWR=0x%04X CHAN=0x%04X TEST1=0x%04X STATUS=0x%04X READCHAN=0x%04X",
            is_seek ? "Seek" : "Tune",
            registers[SI4703_DEVICEID], registers[SI4703_CHIPID],
            registers[SI4703_POWERCFG], registers[SI4703_CHANNEL],
            registers[SI4703_TEST1],
            registers[SI4703_STATUSRSSI], registers[SI4703_READCHAN]);
        // Timeout: best-effort clear the initiating bit to avoid getting stuck
        if(is_seek) {
            registers[SI4703_POWERCFG] &= ~SI4703_PWR_SEEK;
            (void)si4703_write_registers(registers);
        } else {
            registers[SI4703_CHANNEL] &= ~SI4703_CH_TUNE;
            (void)si4703_write_registers(registers);
        }
        si4703_op = Si4703OpNone;
        return;
    }

    // STC set: clear the initiating bit and finish
    FURI_LOG_I("Si4703", "STC set (STATUSRSSI=0x%04X); clearing %s",
        registers[SI4703_STATUSRSSI], is_seek ? "SEEK" : "TUNE");
    if(is_seek) {
        registers[SI4703_POWERCFG] &= ~SI4703_PWR_SEEK;
    } else {
        registers[SI4703_CHANNEL] &= ~SI4703_CH_TUNE;
    }
    if(si4703_write_registers(registers)) {
        si4703_op = is_seek ? Si4703OpSeekWaitClear : Si4703OpTuneWaitClear;
        si4703_op_start_tick = now;
    } else {
        si4703_op = Si4703OpNone;
    }
}

// Helper functions for I2C
static bool acquire_i2c(void) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    // Try primary address first
    if(furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_external, SI4703_I2C_ADDR_DEFAULT, 10)) {
        si4703_i2c_addr = SI4703_I2C_ADDR_DEFAULT;
        return true;
    }
    // Try alternative address
    if(furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_external, SI4703_I2C_ADDR_ALT, 10)) {
        si4703_i2c_addr = SI4703_I2C_ADDR_ALT;
        FURI_LOG_I("Si4703", "Using alternative I2C address 0x11");
        return true;
    }
    return false;
}

static void release_i2c(void) {
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
}

/**
 * @brief Check if Si4703 device is present on I2C bus
 * @note This performs the powerup sequence and then checks for device
 */
bool si4703_is_device_ready(void) {
    FURI_LOG_I("Si4703", "=== Starting Si4703 detection ===");
    
    // Si4703 needs powerup sequence before it will respond on I2C
    if(!si4703_powerup()) {
        FURI_LOG_E("Si4703", "Powerup failed - check wiring:");
        FURI_LOG_E("Si4703", "  RST -> Flipper A4 (MCU PA4)");
        FURI_LOG_E("Si4703", "  SDA/SDIO -> Pin 15 (PC1 / C1)");
        FURI_LOG_E("Si4703", "  SCL/SCLK -> Pin 16 (PC0 / C0)");
        FURI_LOG_E("Si4703", "  SEN -> 3.3V (2-wire I2C mode; pulled high on most breakouts)");
        FURI_LOG_E("Si4703", "  VCC -> 3.3V");
        FURI_LOG_E("Si4703", "  GND -> GND");
        return false;
    }
    
    // Now check if device responds
    FURI_LOG_I("Si4703", "Checking I2C device response...");
    bool result = acquire_i2c();
    release_i2c();
    
    if(result) {
        FURI_LOG_I("Si4703", "=== Si4703 device detected successfully ===");
    } else {
        FURI_LOG_W("Si4703", "=== Si4703 device not responding on I2C ===");
        FURI_LOG_W("Si4703", "Check SEN is high (3.3V) for 2-wire I2C mode");
    }
    
    return result;
}

/**
 * @brief Test I2C communication with Si4703 (assumes chip is already powered up)
 * @return true if device responds on I2C bus
 */
bool si4703_test_i2c_communication(void) {
    FURI_LOG_I("Si4703", "Testing I2C communication (chip should already be powered up)...");
    
    bool result = acquire_i2c();
    release_i2c();
    
    if(result) {
        FURI_LOG_I("Si4703", "I2C communication test PASSED");
    } else {
        FURI_LOG_W("Si4703", "I2C communication test FAILED - device not responding");
    }
    
    return result;
}

/**
 * @brief Read all 16 registers from Si4703
 * 
 * Si4703 read sequence starts at register 0x0A (STATUSRSSI) and wraps around
 * to 0x00-0x09. This function reads all 32 bytes and rearranges them into
 * the correct register order.
 */
bool si4703_read_registers(uint16_t* registers) {
    if(registers == NULL) return false;
    
    uint8_t buffer[32];  // 16 registers * 2 bytes each
    bool result = false;
    
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    
    // Read 32 bytes starting from register 0x0A
    if(furi_hal_i2c_rx(&furi_hal_i2c_handle_external, si4703_i2c_addr, buffer, 32, TIMEOUT_MS)) {
        // Rearrange buffer: read starts at 0x0A, wraps to 0x00-0x09
        // Buffer contains: [0x0A_MSB, 0x0A_LSB, 0x0B_MSB, 0x0B_LSB, ... 0x0F_MSB, 0x0F_LSB, 0x00_MSB, 0x00_LSB, ... 0x09_MSB, 0x09_LSB]
        
        // Registers 0x0A through 0x0F (first 12 bytes of buffer)
        for(int i = 0x0A; i <= 0x0F; i++) {
            int buf_index = (i - 0x0A) * 2;
            registers[i] = (buffer[buf_index] << 8) | buffer[buf_index + 1];
        }
        
        // Registers 0x00 through 0x09 (last 20 bytes of buffer)
        for(int i = 0x00; i <= 0x09; i++) {
            int buf_index = 12 + (i * 2);  // Offset by 12 bytes (6 registers)
            registers[i] = (buffer[buf_index] << 8) | buffer[buf_index + 1];
        }
        
        result = true;
    } else {
        FURI_LOG_W("Si4703", "I2C read failed");
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return result;
}

/**
 * @brief Write registers 0x02-0x07 to Si4703
 * 
 * Si4703 write sequence starts at register 0x02 and continues through 0x07
 * (only these registers are writable). We send 12 bytes total.
 */
bool si4703_write_registers(uint16_t* registers) {
    if(registers == NULL) return false;
    
    uint8_t buffer[12];  // 6 writable registers * 2 bytes each
    bool result = false;
    
    // Pack registers 0x02 through 0x07 into buffer
    for(int i = 0; i < 6; i++) {
        buffer[i * 2] = (registers[i + 0x02] >> 8) & 0xFF;      // MSB
        buffer[i * 2 + 1] = registers[i + 0x02] & 0xFF;         // LSB
    }
    
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    result = furi_hal_i2c_tx(&furi_hal_i2c_handle_external, si4703_i2c_addr, buffer, 12, TIMEOUT_MS);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    
    if(!result) FURI_LOG_W("Si4703", "I2C write failed");

    return result;
}

/**
 * @brief Powerup Si4703 with special reset sequence
 * 
 * The Si4703 requires a specific reset sequence to enter I2C mode:
 * 1. SDIO must be low during reset release to select I2C mode
 * 2. Drive RST low, then high while SDIO is low
 * 3. After reset, oscillator must be enabled separately (in init)
 */
bool si4703_powerup(void) {
    // Only run powerup sequence once
    if(si4703_powered_up) {
        FURI_LOG_I("Si4703", "Already powered up, skipping reset");
        return true;
    }
    
    FURI_LOG_I("Si4703", "Starting reset sequence...");
    
    // Configure RST pin as output
    furi_hal_gpio_init(&si4703_rst_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    
    // IMPORTANT: Flipper external I2C pins are:
    //   Pin 15 = C1 = PC1 = SDA/SDIO
    //   Pin 16 = C0 = PC0 = SCL/SCLK
    // Match the known-working module: SDIO and SCLK LOW while RST rises.
    const GpioPin sda_pin = {.port = GPIOC, .pin = LL_GPIO_PIN_1};
    const GpioPin scl_pin = {.port = GPIOC, .pin = LL_GPIO_PIN_0};
    furi_hal_gpio_init(&scl_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_write(&scl_pin, false); // Hold SCLK low during mode selection
    
    // Reset sequence - CRITICAL: SDIO must be LOW when RST transitions LOW to HIGH
    // This selects I2C mode instead of SPI mode
    FURI_LOG_I("Si4703", "Asserting reset (RST low)...");
    furi_hal_gpio_write(&si4703_rst_pin, false);  // Pull RST low first
    
    FURI_LOG_I("Si4703", "Holding SDIO low...");
    furi_hal_gpio_init(&sda_pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_write(&sda_pin, false);  // Hold SDIO low
    furi_delay_ms(100);
    
    FURI_LOG_I("Si4703", "Releasing reset (RST high)...");
    furi_hal_gpio_write(&si4703_rst_pin, true);   // Release RST (goes high)
    // Interface mode is latched on the rising edge. Release the bus promptly;
    // holding SDIO low throughout the entire startup can stall some modules.
    furi_delay_ms(1);
    
    // Release SDIO and SCLK back to high-Z/open-drain so I2C hardware can reclaim them
    FURI_LOG_I("Si4703", "Releasing SDIO/SCLK pins...");
    furi_hal_gpio_init(&sda_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&scl_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    // Allow the internal power-on calibration to complete after releasing the bus.
    furi_delay_ms(500);
    
    // Reinitialize I2C - equivalent of Wire.begin()
    FURI_LOG_I("Si4703", "Reinitializing I2C bus...");
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    furi_delay_ms(10);

    // Detect which I2C address the module responds on (0x10 or 0x11)
    // This also primes si4703_i2c_addr for subsequent reads/writes.
    bool ready = acquire_i2c();
    release_i2c();
    if(!ready) {
        FURI_LOG_E("Si4703", "No I2C response at 0x10 or 0x11 after reset");
        return false;
    }
    
    // DON'T check device ready yet - chip won't respond until oscillator is enabled!
    // Read registers and check ID first (per reference implementation)
    FURI_LOG_I("Si4703", "Reading initial registers...");
    uint16_t temp_registers[16] = {0};
    
    if(!si4703_read_registers(temp_registers)) {
        FURI_LOG_E("Si4703", "Failed to read registers - check RST, SDIO, SCLK and SEN high");
        return false;
    }

    if(!si4703_ids_are_valid(temp_registers)) {
        FURI_LOG_E("Si4703", "Invalid device identity (DEVICEID=0x%04X CHIPID=0x%04X)",
            temp_registers[SI4703_DEVICEID], temp_registers[SI4703_CHIPID]);
        return false;
    }
    
    FURI_LOG_I("Si4703", "Read successful! DeviceID=0x%04X ChipID=0x%04X", 
        temp_registers[SI4703_DEVICEID], temp_registers[SI4703_CHIPID]);
    
    si4703_powered_up = true;
    FURI_LOG_I("Si4703", "Reset complete - oscillator will be enabled in init");
    
    return true;
}

/**
 * @brief Initialize Si4703 with default settings
 * 
 * Follows AN230 application note sequence:
 * 1. Read registers
 * 2. Enable oscillator (register 0x07 = 0x8100)
 * 3. Wait 500ms for oscillator to stabilize
 * 4. Configure and enable device
 */
bool si4703_init(uint16_t* registers) {
    if(registers == NULL) return false;
    
    FURI_LOG_I("Si4703", "Starting initialization...");
    
    // Read current register values
    FURI_LOG_I("Si4703", "Reading registers...");
    if(!si4703_read_registers(registers)) {
        FURI_LOG_E("Si4703", "Failed to read registers");
        return false;
    }
    
    // Enable crystal oscillator (TEST1 register) - must be done before power enable.
    // Write the literal value 0x8100 (XOSCEN + reserved bit 8) per AN230 rev 0.61.
    // OR-ing only bit 15 into the read-back value is the AN230 rev 0.5 method and
    // is known NOT to work — STC never asserts and every tune/seek times out.
    registers[SI4703_TEST1] = 0x8100;
    if(!si4703_write_registers(registers)) {
        FURI_LOG_E("Si4703", "Failed to enable oscillator");
        return false;
    }
    
    // Wait for oscillator to stabilize (AN230 page 9 - 500ms required)
    FURI_LOG_I("Si4703", "Waiting 500ms for oscillator stabilization...");
    furi_delay_ms(500);
    
    // Configure POWERCFG register (0x02)
    // Enable device (bit 0), disable mute (bit 14), enable softmute (bit 15=0)
    registers[SI4703_POWERCFG] = 0x4001;
    // Bit 15: 0 = Enable Softmute
    // Bit 14: 1 = Disable Mute (normal audio)
    // Bit 13: 0 = Stereo mode (not mono)
    // Bit 6:  0 = Power up (0 = powerup, 1 = powerdown)
    // Bit 0:  1 = Enable powerup
    
    // Configure SYSCONFIG1 register (0x04)
    // GPIO3 (bits 5:4) MUST stay 00 (high impedance): the module's 32.768 kHz
    // crystal is connected across RCLK and GPIO3, so driving GPIO3 as an output
    // stops the oscillator — powerup never completes and tune/seek never sets STC.
    registers[SI4703_SYSCONFIG1] = 0x1000;  // RDS enabled, 75us de-emphasis (North America), GPIOs high-Z
    // Bit 12: 1 = RDS Enable
    // Bit 11: 0 = 75us de-emphasis (US/Canada); set to 1 for 50us (Europe)
    
    // Configure SYSCONFIG2 register (0x05)
    // Band: 87.5-108 MHz (US/Europe), Space: 100 kHz, Volume: 8
    registers[SI4703_SYSCONFIG2] = (0x19 << 8) | (SI4703_BAND_US_EUROPE << 6) | (SI4703_SPACE_100KHZ << 4) | 0x08;
    // Bits 15-8: Seek threshold (0x19 = 25)
    // Bits 7-6: Band select (00 = 87.5-108 MHz)
    // Bits 5-4: Channel spacing (01 = 100 kHz)
    // Bits 3-0: Volume (8 = mid level)
    
    // Configure SYSCONFIG3 register (0x06) - Seek parameters
    registers[SI4703_SYSCONFIG3] = 0x0048;  // Seek SNR = 4, Seek count = 8
    // Bits 7-4: SNR threshold (0100 = 4dB)
    // Bits 3-0: Impulse detection threshold (1000 = 8)
    
    // Write all configuration
    FURI_LOG_I("Si4703", "Writing configuration...");
    if(!si4703_write_registers(registers)) {
        FURI_LOG_E("Si4703", "Failed to write configuration");
        return false;
    }
    
    // Wait for powerup to complete
    furi_delay_ms(110);

    // Mark chip as ready so seek/tune are allowed (powerup set this earlier,
    // but fast-recovery paths call init without going through powerup)
    si4703_powered_up = true;
    return true;
}

/**
 * @brief Set mute state
 */
bool si4703_set_mute(uint16_t* registers, bool mute) {
    if(registers == NULL) return false;
    
    if(!si4703_read_registers(registers)) return false;
    
    if(mute) {
        registers[SI4703_POWERCFG] &= ~SI4703_PWR_DMUTE;  // Clear DMUTE bit to mute
    } else {
        registers[SI4703_POWERCFG] |= SI4703_PWR_DMUTE;   // Set DMUTE bit to unmute
    }
    
    return si4703_write_registers(registers);
}

/**
 * @brief Set stereo/mono mode
 */
bool si4703_set_stereo(uint16_t* registers, bool stereo) {
    if(registers == NULL) return false;
    
    if(!si4703_read_registers(registers)) return false;
    
    if(stereo) {
        registers[SI4703_POWERCFG] &= ~SI4703_PWR_MONO;  // Clear MONO bit for stereo
    } else {
        registers[SI4703_POWERCFG] |= SI4703_PWR_MONO;   // Set MONO bit to force mono
    }
    
    return si4703_write_registers(registers);
}

/**
 * @brief Start seek operation and wait for completion
 */
bool si4703_seek(uint16_t* registers, bool seek_up) {
    if(registers == NULL) return false;

    // Check if chip is powered up
    if(!si4703_powered_up) {
        FURI_LOG_W("Si4703", "Cannot seek - chip not powered up");
        return false;
    }

    // Service any pending tune/seek before starting a new one
    if(si4703_op != Si4703OpNone) {
        if(!si4703_read_registers(registers)) return false;
        si4703_service_after_read(registers);
        if(si4703_op != Si4703OpNone) return false;
    }
    
    if(!si4703_read_registers(registers)) return false;
    
    // Set seek direction
    if(seek_up) {
        registers[SI4703_POWERCFG] |= SI4703_PWR_SEEKUP;   // Seek up
    } else {
        registers[SI4703_POWERCFG] &= ~SI4703_PWR_SEEKUP;  // Seek down
    }
    
    // Set seek mode to wrap at band limits
    registers[SI4703_POWERCFG] &= ~SI4703_PWR_SKMODE;
    
    // Start seek
    registers[SI4703_POWERCFG] |= SI4703_PWR_SEEK;
    
    if(!si4703_write_registers(registers)) return false;

    si4703_op = Si4703OpSeekWaitSet;
    si4703_op_start_tick = furi_get_tick();
    FURI_LOG_I("Si4703", "Seek %s started (POWERCFG=0x%04X)",
        seek_up ? "up" : "down", registers[SI4703_POWERCFG]);
    return true;
}

/**
 * @brief Get current tuned frequency
 */
bool si4703_get_frequency(uint16_t* registers, int* freq_khz) {
    if(registers == NULL || freq_khz == NULL) return false;
    
    if(!si4703_read_registers(registers)) return false;

    return si4703_calculate_frequency_khz_from_regs(registers, freq_khz);
}

/**
 * @brief Set frequency
 */
bool si4703_set_frequency(uint16_t* registers, int freq_khz) {
    if(registers == NULL) return false;

    // Check if chip is powered up
    if(!si4703_powered_up) {
        FURI_LOG_W("Si4703", "Cannot tune - chip not powered up");
        return false;
    }

    // Service any pending tune/seek before starting a new one
    if(si4703_op != Si4703OpNone) {
        if(!si4703_read_registers(registers)) return false;
        si4703_service_after_read(registers);
        if(si4703_op != Si4703OpNone) return false;
    }
    
    if(!si4703_read_registers(registers)) return false;
    
    // Get band and spacing settings
    uint8_t band = (registers[SI4703_SYSCONFIG2] & SI4703_SYS2_BAND) >> 6;
    uint8_t spacing = (registers[SI4703_SYSCONFIG2] & SI4703_SYS2_SPACE) >> 4;
    
    // Calculate base frequency
    int base_freq = 8750;  // 87.5 MHz in units of 10 kHz
    if(band == SI4703_BAND_WIDE || band == SI4703_BAND_JAPAN) {
        base_freq = 7600;  // 76 MHz
    }
    
    // Calculate spacing
    int spacing_val = 20;  // 200 kHz
    if(spacing == SI4703_SPACE_100KHZ) spacing_val = 10;
    else if(spacing == SI4703_SPACE_50KHZ) spacing_val = 5;
    
    // Validate before signed arithmetic is converted to an unsigned channel.
    int max_freq = (band == SI4703_BAND_JAPAN) ? 9000 : 10800;
    if(freq_khz < base_freq * 10 || freq_khz > max_freq * 10) return false;

    // Calculate channel number
    int freq_10khz = freq_khz / 10;  // Convert to 10 kHz units
    uint16_t channel = (freq_10khz - base_freq) / spacing_val;
    
    // Clamp channel to 10-bit value
    if(channel > 0x03FF) channel = 0x03FF;
    
    // Set channel in CHANNEL register
    registers[SI4703_CHANNEL] &= 0xFC00;  // Clear lower 10 bits
    registers[SI4703_CHANNEL] |= channel;
    registers[SI4703_CHANNEL] |= SI4703_CH_TUNE;  // Set TUNE bit
    
    if(!si4703_write_registers(registers)) return false;

    si4703_op = Si4703OpTuneWaitSet;
    si4703_op_start_tick = furi_get_tick();
    FURI_LOG_I("Si4703", "Tune started: %d kHz, channel=%u (CHANNEL=0x%04X)",
        freq_khz, channel, registers[SI4703_CHANNEL]);
    return true;
}

/**
 * @brief Get comprehensive radio information
 */
bool si4703_get_radio_info(uint16_t* registers, SI4703_RADIO_INFO* info) {
    if(registers == NULL || info == NULL) return false;
    
    if(!si4703_read_registers(registers)) return false;
    
    // Sanity check: if DEVICEID and CHIPID are both 0x0000 or 0xFFFF, chip is disconnected
    if(!si4703_ids_are_valid(registers)) {
        FURI_LOG_W("Si4703", "Chip health check failed - disconnected or bus error");
        si4703_reset_state();
        return false;
    }

    // If a tune/seek is in progress, finalize it opportunistically.
    si4703_service_after_read(registers);
    
    // Get frequency
    int freq_khz;
    if(si4703_calculate_frequency_khz_from_regs(registers, &freq_khz)) {
        info->frequency = freq_khz / 1000.0f;  // Convert kHz to MHz
    } else {
        return false;
    }
    
    // Get stereo indicator
    info->stereo = (registers[SI4703_STATUSRSSI] & SI4703_STAT_STEREO) ? true : false;
    
    // Get RSSI (signal strength)
    info->signalLevel = registers[SI4703_STATUSRSSI] & SI4703_STAT_RSSI;  // 0-255
    
    // Determine signal quality
    if(info->signalLevel >= 0 && info->signalLevel <= 20) {
        snprintf(info->signalQuality, sizeof(info->signalQuality), "Poor");
    } else if(info->signalLevel <= 30) {
        snprintf(info->signalQuality, sizeof(info->signalQuality), "Fair");
    } else if(info->signalLevel <= 40) {
        snprintf(info->signalQuality, sizeof(info->signalQuality), "Good");
    } else {
        snprintf(info->signalQuality, sizeof(info->signalQuality), "Excellent");
    }
    
    // Get mute status
    info->muted = (registers[SI4703_POWERCFG] & SI4703_PWR_DMUTE) ? false : true;
    
    return true;
}

/**
 * @brief Set volume level (0-15)
 */
bool si4703_set_volume(uint16_t* registers, uint8_t volume) {
    if(registers == NULL) return false;
    if(volume > 15) volume = 15;
    
    if(!si4703_read_registers(registers)) return false;
    
    // Clear volume bits and set new volume
    registers[SI4703_SYSCONFIG2] &= 0xFFF0;  // Clear lower 4 bits
    registers[SI4703_SYSCONFIG2] |= (volume & 0x0F);
    
    return si4703_write_registers(registers);
}

/**
 * @brief Get current volume level
 */
uint8_t si4703_get_volume(uint16_t* registers) {
    if(registers == NULL) return 0;
    
    si4703_read_registers(registers);
    return registers[SI4703_SYSCONFIG2] & 0x0F;
}

/**
 * @brief Put Si4703 into low power mode
 */
bool si4703_sleep(uint16_t* registers) {
    if(registers == NULL) return false;
    
    if(!si4703_read_registers(registers)) return false;
    
    // Disable powerup and enable power disable
    registers[SI4703_POWERCFG] &= ~SI4703_PWR_ENABLE;
    registers[SI4703_POWERCFG] |= SI4703_PWR_DISABLE;
    
    return si4703_write_registers(registers);
}

/**
 * @brief Set seek threshold
 */
bool si4703_set_seek_threshold(uint16_t* registers, uint8_t threshold) {
    if(registers == NULL) return false;
    if(threshold > 127) threshold = 127;
    
    if(!si4703_read_registers(registers)) return false;
    
    // Clear SEEKTH bits and set new threshold
    registers[SI4703_SYSCONFIG2] &= 0x00FF;  // Clear upper 8 bits
    registers[SI4703_SYSCONFIG2] |= (threshold << 8);
    
    return si4703_write_registers(registers);
}

// ============================================================================
// High-level convenience functions
// ============================================================================

void si4703_seekUp(void) {
    uint16_t registers[16];
    // Use read instead of init to avoid resetting the chip mid-session
    if(si4703_read_registers(registers)) {
        si4703_seek(registers, true);
    }
}

void si4703_seekDown(void) {
    uint16_t registers[16];
    if(si4703_read_registers(registers)) {
        si4703_seek(registers, false);
    }
}

void si4703_ToggleMute(void) {
    uint16_t registers[16];
    if(si4703_read_registers(registers)) {
        bool currently_muted = (registers[SI4703_POWERCFG] & SI4703_PWR_DMUTE) ? false : true;
        si4703_set_mute(registers, !currently_muted);
    }
}

void si4703_MuteOn(void) {
    uint16_t registers[16];
    si4703_set_mute(registers, true);
}

void si4703_MuteOff(void) {
    uint16_t registers[16];
    si4703_set_mute(registers, false);
}

void si4703_SetFreqKHz(int freq_khz) {
    uint16_t registers[16];
    si4703_set_frequency(registers, freq_khz);
}

void si4703_SetFreqMHz(float freq_mhz) {
    uint16_t registers[16];
    int freq_khz = (int)(freq_mhz * 1000.0f);
    si4703_set_frequency(registers, freq_khz);
}

float si4703_GetFreq(void) {
    uint16_t registers[16];
    int freq_khz;
    if(si4703_get_frequency(registers, &freq_khz)) {
        return freq_khz / 1000.0f;
    }
    return -1.0f;  // Error
}

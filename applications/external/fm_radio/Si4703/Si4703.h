/**
 * @file Si4703.h
 * @author Coolshrimp - CoolshrimpModz.com
 * @brief Header file for controlling the Si4703 FM radio chip.
 * @version 1.0
 * @date 2026-02-02
 * 
 * Si4703 Datasheet: https://www.sparkfun.com/datasheets/BreakoutBoards/Si4702-03-C19-1.pdf
 * 
 * PIN CONNECTIONS FOR Si4703:
 * ----------------------------
 * Pin (SDIO) -> Flipper GPIO Pin 15 (SDA, PC1) - ESP32 Pin 19 - I2C Data + GPIO control during powerup
 * Pin (SCLK) -> Flipper GPIO Pin 16 (SCL, PC0) - ESP32 Pin 18 - I2C Clock
 * Pin (RST)  -> Flipper header A4 (MCU PA4) - **REQUIRED** Reset pin (active low)
 * Pin (SEN)  -> Tie to 3.3V for 2-wire I2C mode (pulled high on most breakouts)
 * Pin (GPIO1) -> Optional: Can be used for interrupt (not required for basic operation)
 * Pin (GPIO2) -> Optional: Can be used for interrupt (not required for basic operation)
 * VCC        -> 3.3V
 * GND        -> GND
 * 
 * IMPORTANT: RST pin is REQUIRED! Si4703 needs a special powerup sequence where
 * SDIO is held low while RST transitions from low to high with SEN high
 * to enable I2C mode.
 * Without RST control, the chip will not respond on I2C bus.
 * 
 * @copyright GPLv3
 */

#ifndef SI4703_H
#define SI4703_H

#include <stdbool.h>
#include <stdint.h>

// I2C address for Si4703 (7-bit address)
#define SI4703_ADR 0x10  // Si4703 I2C address (0x10 for write, 0x11 for read in 8-bit format)

// Si4703 has 16 registers (0x00 to 0x0F), each 16-bits (2 bytes)
// Only registers 0x02-0x07 are writable
// Registers 0x00-0x01 and 0x08-0x0F are read-only status registers

// Register addresses
#define SI4703_DEVICEID    0x00  // Device ID (read-only)
#define SI4703_CHIPID      0x01  // Chip ID (read-only)
#define SI4703_POWERCFG    0x02  // Power Configuration
#define SI4703_CHANNEL     0x03  // Channel
#define SI4703_SYSCONFIG1  0x04  // System Configuration 1
#define SI4703_SYSCONFIG2  0x05  // System Configuration 2
#define SI4703_SYSCONFIG3  0x06  // System Configuration 3
#define SI4703_TEST1       0x07  // Test 1
#define SI4703_TEST2       0x08  // Test 2 (read-only)
#define SI4703_BOOTCONFIG  0x09  // Boot Configuration (read-only)
#define SI4703_STATUSRSSI  0x0A  // Status RSSI (read-only)
#define SI4703_READCHAN    0x0B  // Read Channel (read-only)
#define SI4703_RDSA        0x0C  // RDSA (read-only)
#define SI4703_RDSB        0x0D  // RDSB (read-only)
#define SI4703_RDSC        0x0E  // RDSC (read-only)
#define SI4703_RDSD        0x0F  // RDSD (read-only)

// POWERCFG (0x02) register bits
#define SI4703_PWR_DSMUTE   0x8000  // Softmute Disable
#define SI4703_PWR_DMUTE    0x4000  // Mute Disable (1=normal, 0=mute)
#define SI4703_PWR_MONO     0x2000  // Mono Select (1=force mono, 0=stereo)
#define SI4703_PWR_RDSM     0x0800  // RDS Mode (not used in this basic library)
#define SI4703_PWR_SKMODE   0x0400  // Seek Mode (0=wrap, 1=stop at band limits)
#define SI4703_PWR_SEEKUP   0x0200  // Seek Direction (1=up, 0=down)
#define SI4703_PWR_SEEK     0x0100  // Seek (1=enable seek)
#define SI4703_PWR_DISABLE  0x0040  // Powerup Disable (0=powerup, 1=powerdown)
#define SI4703_PWR_ENABLE   0x0001  // Powerup Enable (1=powerup, 0=powerdown)

// CHANNEL (0x03) register bits
#define SI4703_CH_TUNE      0x8000  // Tune (1=enable tune operation)
#define SI4703_CH_CHAN_MASK 0x03FF  // Channel Select (10 bits)

// SYSCONFIG1 (0x04) register bits
#define SI4703_SYS1_RDSIEN  0x8000  // RDS Interrupt Enable
#define SI4703_SYS1_STCIEN  0x4000  // Seek/Tune Complete Interrupt Enable
#define SI4703_SYS1_RDS     0x1000  // RDS Enable
#define SI4703_SYS1_DE      0x0800  // De-emphasis (1=50us, 0=75us)
#define SI4703_SYS1_AGCD    0x0400  // AGC Disable
#define SI4703_SYS1_BLNDADJ 0x00C0  // Stereo/Mono Blend Level Adjustment
#define SI4703_SYS1_GPIO3   0x0030  // General Purpose I/O 3
#define SI4703_SYS1_GPIO2   0x000C  // General Purpose I/O 2
#define SI4703_SYS1_GPIO1   0x0003  // General Purpose I/O 1

// SYSCONFIG2 (0x05) register bits
#define SI4703_SYS2_SEEKTH  0xFF00  // RSSI Seek Threshold (8 bits, default 0x19)
#define SI4703_SYS2_BAND    0x00C0  // Band Select (00=87.5-108MHz, 01=76-108MHz, 10=76-90MHz)
#define SI4703_SYS2_SPACE   0x0030  // Channel Spacing (00=200kHz, 01=100kHz, 10=50kHz)
#define SI4703_SYS2_VOLUME  0x000F  // Volume (0-15)

// SYSCONFIG3 (0x06) register bits
#define SI4703_SYS3_SMUTER  0xC000  // Softmute Attack/Recover Rate
#define SI4703_SYS3_SMUTEA  0x3000  // Softmute Attenuation
#define SI4703_SYS3_SKSNR   0x00F0  // Seek SNR Threshold
#define SI4703_SYS3_SKCNT   0x000F  // Seek FM Impulse Detection Threshold

// STATUSRSSI (0x0A) register bits
#define SI4703_STAT_RDSR    0x8000  // RDS Ready
#define SI4703_STAT_STC     0x4000  // Seek/Tune Complete
#define SI4703_STAT_SFBL    0x2000  // Seek Fail/Band Limit
#define SI4703_STAT_AFCRL   0x1000  // AFC Rail
#define SI4703_STAT_RDSS    0x0800  // RDS Synchronized
#define SI4703_STAT_STEREO  0x0100  // Stereo Indicator
#define SI4703_STAT_RSSI    0x00FF  // RSSI (8 bits)

// Band select values
typedef enum {
    SI4703_BAND_US_EUROPE = 0,  // 87.5-108 MHz (US/Europe)
    SI4703_BAND_WIDE = 1,        // 76-108 MHz (Wide)
    SI4703_BAND_JAPAN = 2        // 76-90 MHz (Japan)
} SI4703_BAND;

// Channel spacing values
typedef enum {
    SI4703_SPACE_200KHZ = 0,  // 200 kHz spacing (US/Australia)
    SI4703_SPACE_100KHZ = 1,  // 100 kHz spacing (Europe/Japan)
    SI4703_SPACE_50KHZ = 2    // 50 kHz spacing
} SI4703_SPACE;

// Radio information structure
typedef struct {
    float frequency;       // Current frequency in MHz
    int signalLevel;       // RSSI signal level (0-255)
    bool stereo;           // True if receiving stereo signal
    bool muted;            // True if audio is muted
    char signalQuality[10]; // Text description of signal quality
} SI4703_RADIO_INFO;

// Function prototypes

/**
 * @brief Check if Si4703 device is present and ready on I2C bus
 * @return true if device is ready, false otherwise
 */
bool si4703_is_device_ready(void);

/**
 * @brief Test I2C communication with Si4703 (assumes chip is already powered up)
 * @return true if device responds on I2C bus
 */
bool si4703_test_i2c_communication(void);

/**
 * @brief Reset Si4703 state (call when chip is disconnected/reconnected)
 */
void si4703_reset_state(void);

/**
 * @brief Check if a seek or tune operation is currently in progress
 * @return true if busy (async op pending), false if idle
 */
bool si4703_is_busy(void);

/**
 * @brief Read all 16 registers from Si4703 (32 bytes)
 * @param registers Buffer to store 16 16-bit registers (must be 16 uint16_t elements)
 * @return true if read successful, false otherwise
 * @note Si4703 read starts at register 0x0A and wraps to 0x00-0x09
 */
bool si4703_read_registers(uint16_t* registers);

/**
 * @brief Write registers 0x02-0x07 to Si4703 (writable registers)
 * @param registers Buffer containing all 16 registers (only 0x02-0x07 are written)
 * @return true if write successful, false otherwise
 */
bool si4703_write_registers(uint16_t* registers);

/**
 * @brief Initialize Si4703 with default settings
 * @param registers Register buffer (will be populated)
 * @return true if initialization successful, false otherwise
 * @note Performs powerup sequence and sets default band/spacing
 */
bool si4703_init(uint16_t* registers);

/**
 * @brief Powerup Si4703 using special sequence
 * @return true if powerup successful, false otherwise
 * @note Requires RST pin control and SDIO manipulation
 */
bool si4703_powerup(void);

/**
 * @brief Set mute state
 * @param registers Register buffer
 * @param mute true to mute audio, false to unmute
 * @return true if successful, false otherwise
 */
bool si4703_set_mute(uint16_t* registers, bool mute);

/**
 * @brief Set stereo/mono mode
 * @param registers Register buffer
 * @param stereo true for stereo, false to force mono
 * @return true if successful, false otherwise
 */
bool si4703_set_stereo(uint16_t* registers, bool stereo);

/**
 * @brief Start seek operation
 * @param registers Register buffer
 * @param seek_up true to seek up, false to seek down
 * @return true if seek started successfully, false otherwise
 * @note Will automatically wait for seek to complete
 */
bool si4703_seek(uint16_t* registers, bool seek_up);

/**
 * @brief Get current tuned frequency
 * @param registers Register buffer
 * @param freq_khz Pointer to store frequency in kHz (e.g., 100700 for 100.7 MHz)
 * @return true if successful, false otherwise
 */
bool si4703_get_frequency(uint16_t* registers, int* freq_khz);

/**
 * @brief Set frequency
 * @param registers Register buffer
 * @param freq_khz Frequency in kHz (e.g., 100700 for 100.7 MHz)
 * @return true if successful, false otherwise
 */
bool si4703_set_frequency(uint16_t* registers, int freq_khz);

/**
 * @brief Get comprehensive radio information
 * @param registers Register buffer
 * @param info Pointer to SI4703_RADIO_INFO structure to populate
 * @return true if successful, false otherwise
 */
bool si4703_get_radio_info(uint16_t* registers, SI4703_RADIO_INFO* info);

/**
 * @brief Set volume level
 * @param registers Register buffer
 * @param volume Volume level (0-15)
 * @return true if successful, false otherwise
 */
bool si4703_set_volume(uint16_t* registers, uint8_t volume);

/**
 * @brief Get current volume level
 * @param registers Register buffer
 * @return Volume level (0-15)
 */
uint8_t si4703_get_volume(uint16_t* registers);

/**
 * @brief Put Si4703 into low power mode
 * @param registers Register buffer
 * @return true if successful, false otherwise
 */
bool si4703_sleep(uint16_t* registers);

/**
 * @brief Set seek threshold (signal strength required to stop seek)
 * @param registers Register buffer
 * @param threshold RSSI threshold (0-127, default is 25)
 * @return true if successful, false otherwise
 */
bool si4703_set_seek_threshold(uint16_t* registers, uint8_t threshold);

// High-level convenience functions (similar to TEA5767)

/**
 * @brief Seek up to next station
 */
void si4703_seekUp(void);

/**
 * @brief Seek down to previous station
 */
void si4703_seekDown(void);

/**
 * @brief Toggle mute on/off
 */
void si4703_ToggleMute(void);

/**
 * @brief Turn mute on
 */
void si4703_MuteOn(void);

/**
 * @brief Turn mute off
 */
void si4703_MuteOff(void);

/**
 * @brief Set frequency in kHz
 * @param freq_khz Frequency in kHz (e.g., 100700 for 100.7 MHz)
 */
void si4703_SetFreqKHz(int freq_khz);

/**
 * @brief Set frequency in MHz
 * @param freq_mhz Frequency in MHz (e.g., 100.7)
 */
void si4703_SetFreqMHz(float freq_mhz);

/**
 * @brief Get current frequency in MHz
 * @return Frequency in MHz, or -1.0 on error
 */
float si4703_GetFreq(void);

#endif // SI4703_H

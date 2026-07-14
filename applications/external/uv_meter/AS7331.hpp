/**
 * @file AS7331.hpp
 * @brief UV Spectral Sensor - AS7331 Driver
 *
 * This driver provides a communication interface for the AS7331 UV spectral sensor.
 * It includes configurations for I2C addressing, device modes, gain, clock frequency,
 * and measurement settings.
 *
 * Inspired by: https://github.com/sparkfun/SparkFun_AS7331_Arduino_Library/blob/main/src/sfeAS7331.h
 * Datasheet: https://look.ams-osram.com/m/1856fd2c69c35605/original/AS7331-Spectral-UVA-B-C-Sensor.pdf
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <furi_hal_i2c_types.h>

/**
 * @defgroup AS7331_I2C_Addressing I2C Addressing
 * @brief I2C address definitions for AS7331
 *
 * The 7-bit I2C address is defined as [1, 1, 1, 0, 1, A1, A0], where A1 and A0 are configurable address pins.
 * @{
 */

/** Default I2C address when A1 = 0, A0 = 0 */
const uint8_t DefaultI2CAddr = 0x74;

/** Secondary I2C address when A1 = 0, A0 = 1 */
const uint8_t SecondaryI2CAddr = 0x75;

/** Tertiary I2C address when A1 = 1, A0 = 0 */
const uint8_t TertiaryI2CAddr = 0x76;

/** Quaternary I2C address when A1 = 1, A0 = 1 */
const uint8_t QuaternaryI2CAddr = 0x77;

/** Expected API Generation Register content (Device ID + Mutation number) */
const uint8_t ExpectedAGENContent = 0x21;

/** @} */ // End of AS7331_I2C_Addressing group

/**
 * @defgroup AS7331_Enums Enumerations
 * @brief Enumerations for AS7331 settings
 * @{
 */

/**
 * @brief Device Operating Modes
 */
enum as7331_device_mode_t : uint8_t {
    DEVICE_MODE_CONFIG = 0x2, /**< Configuration mode */
    DEVICE_MODE_MEASURE = 0x3 /**< Measurement mode */
};

/**
 * @brief Sensor Gain Settings (CREG1:GAIN)
 *
 * Default: GAIN_2 (1010)
 *
 * Gain value is calculated as:  
 * **gain = 2<sup>(11 - gain_code)</sup>**
 */
enum as7331_gain_t : uint8_t {
    GAIN_2048 = 0x0000,
    GAIN_1024,
    GAIN_512,
    GAIN_256,
    GAIN_128,
    GAIN_64,
    GAIN_32,
    GAIN_16,
    GAIN_8,
    GAIN_4,
    GAIN_2,
    GAIN_1
};

/**
 * @brief Integration Time Settings (CREG1:TIME)
 *
 * Default: TIME_64MS (0110)
 *
 * The conversion time in the enumerators (in milliseconds) is valid for a clock frequency of 1.024 MHz.
 * For higher clock frequencies, the conversion time is divided by 2 each time the clock frequency is doubled.
 *
 * **Conversion Time [ms] = 2<sup>integration_time_code</sup>**
 */
enum as7331_integration_time_t : uint8_t {
    TIME_1MS = 0x0,
    TIME_2MS,
    TIME_4MS,
    TIME_8MS,
    TIME_16MS,
    TIME_32MS,
    TIME_64MS,
    TIME_128MS,
    TIME_256MS,
    TIME_512MS,
    TIME_1024MS,
    TIME_2048MS,
    TIME_4096MS,
    TIME_8192MS,
    TIME_16384MS
};

/**
 * @brief Divider Settings (CREG2:DIV)
 *
 * Default: DIV_2 (000)
 *
 * Divider value is calculated as:  
 * **divider = 2<sup>(1 + divider_code)</sup>**
 */
enum as7331_divider_t : uint8_t {
    DIV_2 = 0x0,
    DIV_4,
    DIV_8,
    DIV_16,
    DIV_32,
    DIV_64,
    DIV_128,
    DIV_256
};

/**
 * @brief Internal Clock Frequency Settings fCLK (CREG3:CCLK)
 *
 * Default: CCLK_1_024_MHZ
 *
 * Clock frequency is calculated as:  
 * **clock_frequency = 1.024 × 2<sup>clock_frequency_code</sup>** (in MHz)
 */
enum as7331_clock_frequency_t : uint8_t {
    CCLK_1_024_MHZ = 0x00, /**< 1.024 MHz: 1 - 16384 ms (Conversion Time) */
    CCLK_2_048_MHZ,
    CCLK_4_096_MHZ,
    CCLK_8_192_MHZ /**< 8.192 MHz: 0.125 - 2048 ms (Conversion Time) */
};

/**
 * @brief Measurement Modes (CREG3:MMODE)
 *
 * Default: MEASUREMENT_MODE_COMMAND (01)
 */
enum as7331_measurement_mode_t : uint8_t {
    MEASUREMENT_MODE_CONTINUOUS = 0x0, /**< Continuous Measurement Mode (CONT) */
    MEASUREMENT_MODE_COMMAND, /**< Command Measurement Mode (CMD) */
    MEASUREMENT_MODE_SYNC_START, /**< Synchronous Measurement Mode (SYNS) - externally synchronized start of measurement */
    MEASUREMENT_MODE_SYNC_START_END /**< Synchronous Measurement Start and End Mode (SYND) - start and end of measurement are externally synchronized */
};

/** @brief UV Type Selection */
enum as7331_uv_type_t : uint8_t {
    UV_A,
    UV_B,
    UV_C
};

/** @} */ // End of AS7331_Enums group

/**
 * @defgroup AS7331_Config_Registers Configuration Register Definitions
 * @brief Definitions for configuration registers
 *
 * Registers are 8 bits long and can only be accessed in the configuration mode.
 * @{
 */

/** @brief OSR configuration register address */
const uint8_t RegCfgOsr = 0x00;

/**
 * @brief Operational State Register (OSR) Register Layout
 */
typedef union {
    struct {
        as7331_device_mode_t
            operating_state    : 3; /**< DOS (010) - Device Operating State (OSR[2:0]) */
        uint8_t software_reset : 1; /**< SW_RES (0) - Software Reset (OSR[3]) */
        uint8_t reserved       : 2; /**< Reserved, do not write (OSR[5:4]) */
        uint8_t power_down     : 1; /**< PD (1) - Power Down (OSR[6]) */
        uint8_t start_state    : 1; /**< SS (0) - Start State (OSR[7]) */
    };
    uint8_t byte;
} as7331_osr_reg_t;

/** @brief AGEN register address */
const uint8_t RegCfgAgen = 0x02;

/**
 * @brief AGEN Register Layout
 */
typedef union {
    struct {
        uint8_t
            mutation : 4; /**< MUT (0001) - Mutation number of control register bank. Incremented when control registers change (AGEN[3:0]) */
        uint8_t device_id : 4; /**< DEVID (0010) - Device ID number (AGEN[7:4]) */
    };
    uint8_t byte;
} as7331_agen_reg_t;

/** @brief CREG1 -- Configuration Register 1 address */
const uint8_t RegCfgCreg1 = 0x06;

/**
 * @brief CREG1 Register Layout
 */
typedef union {
    struct {
        as7331_integration_time_t
            integration_time : 4; /**< TIME (0110) - Integration time (CREG1[3:0]) */
        as7331_gain_t gain   : 4; /**< GAIN (1010) - Sensor gain (CREG1[7:4]) */
    };
    uint8_t byte;
} as7331_creg1_reg_t;

/** @brief CREG2 -- Configuration Register 2 address */
const uint8_t RegCfgCreg2 = 0x07;

/**
 * @brief CREG2 Register Layout
 */
typedef union {
    struct {
        as7331_divider_t divider : 3; /**< DIV (000) - Divider value (CREG2[2:0]) */
        uint8_t enable_divider   : 1; /**< EN_DIV (0) - Divider enable (CREG2[3]) */
        uint8_t reserved         : 2; /**< Reserved, do not write (CREG2[5:4]) */
        uint8_t
            enable_temp : 1; /**< EN_TM (1) - Temperature measurement enable in SYND mode (CREG2[6]) */
        uint8_t reserved1 : 1; /**< Reserved, do not write (CREG2[7]) */
    };
    uint8_t byte;
} as7331_creg2_reg_t;

/** @brief CREG3 -- Configuration Register 3 address */
const uint8_t RegCfgCreg3 = 0x08;

/**
 * @brief CREG3 Register Layout
 */
typedef union {
    struct {
        as7331_clock_frequency_t
            clock_frequency : 2; /**< CCLK (00) - Internal clock frequency (CREG3[1:0]) */
        uint8_t reserved    : 1; /**< Reserved, do not write (CREG3[2]) */
        uint8_t ready_mode  : 1; /**< RDYOD (0) - Ready pin mode (CREG3[3]) */
        uint8_t standby     : 1; /**< SB (0) - Standby mode (CREG3[4]) */
        uint8_t reserved1   : 1; /**< Reserved, do not write (CREG3[5]) */
        as7331_measurement_mode_t
            measurement_mode : 2; /**< MMODE (01) - Measurement mode selection (CREG3[7:6]) */
    };
    uint8_t byte;
} as7331_creg3_reg_t;

/** @brief BREAK register address */
const uint8_t RegCfgBreak =
    0x09; /**< BREAK (0x19) - Break time TBREAK between two measurements (except CMD mode) */

/** @brief EDGES register address */
const uint8_t RegCfgEdges = 0x0A; /**< EDGES (0x1) - Number of SYN falling edges */

/** @brief OPTREG - Option Register address */
const uint8_t RegCfgOptReg = 0x0B;

/**
 * @brief OPTREG Register Layout
 */
typedef union {
    struct {
        uint8_t init_idx : 1; /**< INIT_IDX (1) - I2C repeat start mode flag (OPTREG[0]) */
        uint8_t reserved : 7; /**< Reserved, do not write (OPTREG[7:1]) */
    };
    uint8_t byte;
} as7331_optreg_reg_t;

/** @} */ // End of AS7331_Config_Registers group

/**
 * @defgroup AS7331_Measurement_Registers Measurement Register Definitions
 * @brief Definitions for measurement registers
 *
 * Registers are 16 bits long and read-only, except for the lower byte of the OSR/STATUS register.
 * @{
 */

/** @brief OSR/Status register address */
const uint8_t RegMeasOsrStatus = 0x00;

/**
 * @brief OSR/STATUS Register Layout
 */
typedef union {
    struct {
        as7331_osr_reg_t osr; /**< OSR settings (lower byte) (OSRSTAT[7:0]) */
        uint8_t power_state   : 1; /**< POWERSTATE - Power Down state (OSRSTAT[8]) */
        uint8_t standby_state : 1; /**< STANDBYSTATE - Standby mode state (OSRSTAT[9]) */
        uint8_t not_ready     : 1; /**< NOTREADY - Inverted ready pin state (OSRSTAT[10]) */
        uint8_t new_data      : 1; /**< NDATA - New data available (OSRSTAT[11]) */
        uint8_t lost_data     : 1; /**< LDATA - Data overwritten before retrieval (OSRSTAT[12]) */
        uint8_t adc_overflow  : 1; /**< ADCOF - Overflow of ADC channel (OSRSTAT[13]) */
        uint8_t result_overflow : 1; /**< MRESOF - Overflow of MRES1...MRES3 (OSRSTAT[14]) */
        uint8_t
            out_conv_overflow : 1; /**< OUTCONVOF - Overflow of internal 24-bit OUTCONV (OSRSTAT[15]) */
    };
    uint16_t word;
} as7331_osr_status_reg_t;

/** @brief TEMP register address (12-bit temperature, MS 4 bits are 0) */
const uint8_t RegMeasTemp = 0x01;

/** @brief MRES1 register address - Measurement Result A */
const uint8_t RegMeasResultA = 0x02;

/** @brief MRES2 register address - Measurement Result B */
const uint8_t RegMeasResultB = 0x03;

/** @brief MRES3 register address - Measurement Result C */
const uint8_t RegMeasResultC = 0x04;

/** @brief OUTCONVL register address - First 16 bits of 24-bit OUTCONV */
const uint8_t RegMeasOutConvL = 0x05;

/** @brief OUTCONVH register address - Upper 8 bits of OUTCONV, MSB is 0 */
const uint8_t RegMeasOutConvH = 0x06;

/** @} */ // End of AS7331_Measurement_Registers group

/**
 * @brief AS7331 UV Spectral Sensor Driver
 *
 * This class provides an interface for the AS7331 UV spectral sensor,
 * allowing configuration and measurement.
 */
class AS7331 {
public:
    /** @brief Struct to hold raw measurement results */
    struct RawResults {
        uint16_t uv_a; /**< Raw measurement for UV-A channel */
        uint16_t uv_b; /**< Raw measurement for UV-B channel */
        uint16_t uv_c; /**< Raw measurement for UV-C channel */
    };

    /** @brief Struct to hold processed measurement results */
    struct Results {
        double uv_a; /**< Irradiance for UV-A in µW/cm² */
        double uv_b; /**< Irradiance for UV-B in µW/cm² */
        double uv_c; /**< Irradiance for UV-C in µW/cm² */
    };

    /**
     * @brief Constructs an AS7331 sensor object
     *
     * @param address I2C address of the sensor (default is DefaultI2CAddr)
     */
    AS7331(uint8_t address = DefaultI2CAddr);

    /**
     * @brief Initialize the sensor
     *
     * @param address I2C address in 7-bit format. If 0x0, scan the I2C bus to find the device.
     * @return true if initialization is successful, false otherwise
     */
    bool init(const uint8_t& address = 0);

    // Configuration methods

    /**
     * @brief Set sensor gain (CREG1:GAIN)
     *
     * @param gain Gain setting from as7331_gain_t
     * @return true if successful, false otherwise
     */
    bool setGain(const as7331_gain_t& gain);

    /**
     * @brief Set integration time (CREG1:TIME)
     *
     * @param time Integration time setting from as7331_integration_time_t
     * @return true if successful, false otherwise
     */
    bool setIntegrationTime(const as7331_integration_time_t& time);

    /**
     * @brief Set output divider (CREG2:DIV)
     *
     * @param divider Divider setting from as7331_divider_t
     * @param enable Enable or disable the divider
     * @return true if successful, false otherwise
     */
    bool setDivider(const as7331_divider_t& divider, const bool enable = true);

    /**
     * @brief Set clock frequency (CREG3:CCLK)
     *
     * @param freq Clock frequency setting from as7331_clock_frequency_t
     * @return true if successful, false otherwise
     */
    bool setClockFrequency(const as7331_clock_frequency_t& freq);

    /**
     * @brief Set measurement mode (CREG3:MMODE)
     *
     * @param mode Measurement mode setting from as7331_measurement_mode_t
     * @return true if successful, false otherwise
     */
    bool setMeasurementMode(const as7331_measurement_mode_t& mode);

    // Power management

    /**
     * @brief Enable or disable power-down state (OSR:PD)
     *
     * @param power_down true to enable power-down, false to disable
     * @return true if successful, false otherwise
     */
    bool setPowerDown(const bool& power_down);

    /**
     * @brief Enable or disable standby mode (CREG3:SB)
     *
     * @param standby true to enable standby, false to disable
     * @return true if successful, false otherwise
     */
    bool setStandby(const bool& standby);

    // Measurement methods

    /**
     * @brief Start the measurement (OSR:SS = 1)
     *
     * @return true if successful, false otherwise
     */
    bool startMeasurement();

    /**
     * @brief Stop the measurement (OSR:SS = 0)
     *
     * @return true if successful, false otherwise
     */
    bool stopMeasurement();

    /**
     * @brief Wait for the measurement to complete based on conversion time
     */
    void waitForMeasurement();

    /**
     * @brief Get raw measurement results
     *
     * Before reading results, wait an appropriate amount of time for the measurement to complete (TCONV).
     *
     * @param rawResults Struct to hold raw measurement results
     * @return true if successful, false otherwise
     */
    bool getRawResults(RawResults& rawResults);

    /**
     * @brief Get processed measurement results
     *
     * Before reading results, wait an appropriate amount of time for the measurement to complete (TCONV).
     *
     * @param results Struct to hold processed measurement results
     * @param rawResults Struct to hold raw measurement results
     * @return true if successful, false otherwise
     */
    bool getResults(Results& results, RawResults& rawResults);

    /**
     * @brief Get processed measurement results
     *
     * Before reading results, wait an appropriate amount of time for the measurement to complete (TCONV).
     *
     * @param results Struct to hold processed measurement results
     * @return true if successful, false otherwise
     */
    bool getResults(Results& results);

    /**
     * @brief Get temperature measurement
     *
     * Before reading results, wait an appropriate amount of time for the measurement to complete (TCONV).
     *
     * @param temperature Temperature in degrees Celsius
     * @return true if successful, false otherwise
     */
    bool getTemperature(double& temperature);

    // Utility methods

    /**
     * @brief Check if the device is ready
     *
     * @param i2c_address_7bit I2C address in 7-bit format (default uses internal address)
     * @return true if device is ready, false otherwise
     */
    bool deviceReady(const uint8_t& i2c_address_7bit = 0);

    /**
     * @brief Perform a software reset
     *
     * This immediately stops any running measurements and resets the device to its configuration state,
     * with all registers reverting to their initial values.
     *
     * @return true if the reset was successful, false otherwise
     */
    bool reset();

    // Getter methods

    /**
     * @brief Get device ID (including mutation number)
     *
     * @param deviceID Struct to hold device ID
     * @return true if successful, false otherwise
     */
    bool getDeviceID(as7331_agen_reg_t& deviceID);

    /**
     * @brief Get status register in measurement mode
     *
     * @param status Struct to hold status register data
     * @return true if successful, false otherwise
     */
    bool getStatus(as7331_osr_status_reg_t& status);

    // Local config getter methods

    /**
     * @brief Get current gain setting
     *
     * @return Current gain setting as as7331_gain_t
     */
    as7331_gain_t getGain() const;

    /**
     * @brief Get actual gain value
     *
     * @return Gain value as integer
     */
    int16_t getGainValue() const;

    /**
     * @brief Get current integration time setting
     *
     * @return Current integration time as as7331_integration_time_t
     */
    as7331_integration_time_t getIntegrationTime() const;

    /**
     * @brief Get current conversion time in seconds
     *
     * @return Conversion time in seconds
     */
    double getConversionTime() const;

    /**
     * @brief Get current divider setting
     *
     * @return Current divider setting as as7331_divider_t
     */
    as7331_divider_t getDivider() const;

    /**
     * @brief Check if divider is enabled
     *
     * @return true if divider is enabled, false otherwise
     */
    bool isDividerEnabled() const;

    /**
     * @brief Get actual divider value
     *
     * @return Divider value as integer
     */
    uint8_t getDividerValue() const;

    /**
     * @brief Get current clock frequency setting
     *
     * @return Current clock frequency as as7331_clock_frequency_t
     */
    as7331_clock_frequency_t getClockFrequency() const;

    /**
     * @brief Get actual clock frequency value
     *
     * @return Clock frequency in MHz
     */
    double getClockFrequencyValue() const;

    /**
     * @brief Get current measurement mode
     *
     * @return Current measurement mode as as7331_measurement_mode_t
     */
    as7331_measurement_mode_t getMeasurementMode() const;

    /**
     * @brief Check if power-down is enabled
     *
     * @return true if power-down is enabled, false otherwise
     */
    bool isPowerDown() const;

    /**
     * @brief Check if standby mode is enabled
     *
     * @return true if standby is enabled, false otherwise
     */
    bool isStandby() const;

private:
    // Internal helper methods

    /**
     * @brief Set the device mode
     *
     * @param mode Device mode to set
     * @return true if successful, false otherwise
     */
    bool setDeviceMode(const as7331_device_mode_t& mode);

    /**
     * @brief Read OSR register and update local config
     *
     * @param osr Struct to hold OSR register data
     * @return true if successful, false otherwise
     */
    bool readOSRRegister(as7331_osr_reg_t& osr);

    /**
     * @brief Write OSR register and update local config
     *
     * @param osr OSR register data to write
     * @return true if successful, false otherwise
     */
    bool writeOSRRegister(const as7331_osr_reg_t& osr);

    /**
     * @defgroup AS7331_I2C_Methods AS7331 Custom I2C Methods
     * @brief Custom I2C read and write functions for the AS7331 sensor.
     *
     * These functions utilize a "repeated start" condition instead of terminating each transaction with a STOP condition, as required by the AS7331.
     * This is achieved using `FuriHalI2cEndAwaitRestart` and `FuriHalI2cBeginRestart`, despite their documentation referring to "Clock Stretching," which the AS7331 does not support.
     *
     * @note The caller is responsible for calling `furi_hal_i2c_acquire` and `furi_hal_i2c_release`.
     * @{
     */

    /**
     * @brief Read multiple bytes from consecutive registers
     *
     * @param start_register_addr Starting register address
     * @param buffer Buffer to store read data
     * @param length Number of bytes to read
     * @return true if successful, false otherwise
     * @ingroup AS7331_I2C_Methods
     */
    bool readRegisters(uint8_t start_register_addr, uint8_t* buffer, size_t length);

    /**
     * @brief Read an 8-bit register
     *
     * @param register_addr Register address to read
     * @param data Variable to store read data
     * @return true if successful, false otherwise
     * @ingroup AS7331_I2C_Methods
     */
    bool readRegister(uint8_t register_addr, uint8_t& data);

    /**
     * @brief Read a 16-bit register
     *
     * @param register_addr Register address to read
     * @param data Variable to store read data
     * @return true if successful, false otherwise
     * @ingroup AS7331_I2C_Methods
     */
    bool readRegister16(uint8_t register_addr, uint16_t& data);

    /**
     * @brief Write an 8-bit register
     *
     * @param register_addr Register address to write
     * @param data Data to write
     * @return true if successful, false otherwise
     * @ingroup AS7331_I2C_Methods
     */
    bool writeRegister(uint8_t register_addr, const uint8_t& data);

    /** @} */ // End of AS7331_I2C_Methods group

    /**
     * @brief Scan I2C bus for AS7331 device
     *
     * @return Found I2C address in 7-bit format, 0 if not found
     */
    uint8_t scan_i2c_bus();

    /**
     * @brief Update local configuration variables from device registers
     *
     * @return true if successful, false otherwise
     */
    bool updateLocalConfig();

    /**
     * @brief Calculate Full-Scale Range (FSREe) for a given UV type
     *
     * @param uvType UV type (UV_A, UV_B, UV_C)
     * @param adjustForIntegrationTime Optional. A boolean flag indicating whether to adjust FSREe for the integration TIME setting. Default is `true`.
     * @return FSREe value in µW/cm², `-1.0` if an error occurs
     */
    double calculateFSREe(as7331_uv_type_t uvType, bool adjustForIntegrationTime = true);

    // Private member variables
    uint8_t _i2c_addr_8bit;
    as7331_device_mode_t _deviceMode;
    bool _power_down;
    as7331_gain_t _gain; /**< Gain code (0 to 11) */
    as7331_integration_time_t _integration_time; /**< Integration time code (0 to 15) */
    bool _enable_divider; /**< Divider enabled (CREG2:EN_DIV) */
    as7331_divider_t _divider; /**< Divider value (CREG2:DIV) */
    as7331_clock_frequency_t _clock_frequency; /**< Clock frequency code (0 to 3) */
    bool _standby;
    as7331_measurement_mode_t _measurement_mode;
};

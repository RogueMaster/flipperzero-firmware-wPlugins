/**
 * @file AS7331.cpp
 * @brief Implementation of AS7331 UV Spectral Sensor Driver
 */

#include "AS7331.hpp"
#include <furi_hal_i2c.h>
#include <furi_hal.h>
#include <cmath> // For pow()

// Prevent compiler optimization, for debugging.
// #pragma GCC optimize("O0")

// Timeout value for I2C operations (in milliseconds)
#define AS7331_I2C_TIMEOUT 100

// Constructor
AS7331::AS7331(uint8_t i2c_address_7bit)
    // Convert 7-bit to 8-bit address
    : _i2c_addr_8bit(i2c_address_7bit << 1)
    // Initialize to invalid mode to force first-time setup
    , _deviceMode(static_cast<as7331_device_mode_t>(0x00))
    , _power_down(true)
    , _gain(GAIN_2)
    , _integration_time(TIME_64MS)
    , _enable_divider(false)
    , _divider(DIV_2)
    , _clock_frequency(CCLK_1_024_MHZ)
    , _standby(false)
    , _measurement_mode(MEASUREMENT_MODE_COMMAND) {
}

// Initialize the sensor
bool AS7331::init(const uint8_t& i2c_address_7bit) {
    uint8_t detected_address_7bit = 0;

    if(i2c_address_7bit == 0x0) {
        // Scan I2C bus for AS7331 device
        detected_address_7bit = scan_i2c_bus();
        if(detected_address_7bit == 0x0) {
            FURI_LOG_E(
                "AS7331",
                "Failed to find an AS7331 device with one of the four valid addresses on the I2C bus.");
            return false;
        }
    } else {
        if(deviceReady(i2c_address_7bit)) {
            detected_address_7bit = i2c_address_7bit;
        } else {
            FURI_LOG_E("AS7331", "No device found at I2C address 0x%02X.", i2c_address_7bit);
            return false;
        }
    }
    // Set the I2C address (7-Bit to 8-Bit)
    _i2c_addr_8bit = detected_address_7bit << 1;

    // Wake up the device from power-down mode
    if(!setPowerDown(false)) {
        FURI_LOG_E("AS7331", "Failed to wake up the device from power-down mode.");
        return false;
    }

    // Set the device into configuration mode
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool success = setDeviceMode(DEVICE_MODE_CONFIG);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    if(!success) {
        FURI_LOG_E("AS7331", "Failed to set the device into configuration mode.");
        return false;
    }

    // Populate local configuration variables from device registers
    if(!updateLocalConfig()) {
        FURI_LOG_E("AS7331", "Failed to read device configuration registers.");
        return false;
    }

    // Read the device ID
    as7331_agen_reg_t deviceID;
    if(!getDeviceID(deviceID)) {
        FURI_LOG_E("AS7331", "Failed to read device ID.");
        return false;
    }
    // Compare with expected Device ID
    if(deviceID.byte != ExpectedAGENContent) {
        FURI_LOG_E(
            "AS7331",
            "Device ID mismatch: expected 0x%02X, got 0x%02X.",
            ExpectedAGENContent,
            deviceID.byte);
        return false;
    }

    return true;
}

// Set sensor gain (CREG1:GAIN)
bool AS7331::setGain(const as7331_gain_t& gain) {
    if(gain > GAIN_1) {
        FURI_LOG_E("AS7331", "Invalid gain value: %d.", gain);
        return false;
    }

    as7331_creg1_reg_t creg1;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Ensure we are in configuration mode
    if(!setDeviceMode(DEVICE_MODE_CONFIG)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed set device mode to configuration.");
        return false;
    }

    // Read CREG1
    if(!readRegister(RegCfgCreg1, creg1.byte)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read CREG1 register from device.");
        return false;
    }

    // Write new gain value
    creg1.gain = gain;
    if(!writeRegister(RegCfgCreg1, creg1.byte)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to write gain value to device: %d.", creg1.gain);
        return false;
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    // Update local config
    _gain = gain;

    return true;
}

// Set integration time (CREG1:TIME)
bool AS7331::setIntegrationTime(const as7331_integration_time_t& time) {
    // Validate integration time value
    if(time > TIME_16384MS) {
        FURI_LOG_E("AS7331", "Invalid integration time value: %d.", time);
        return false;
    }

    // Automatically set divider depending on ADC resolution to prevent overflow
    // Since this voids advantages of higher integration times, remove once overflow checking is implemented.
    /*
    int adc_resolution;
    if(time == 15) {
        // Special case: Same as TIME 0
        adc_resolution = 10;
    } else {
        adc_resolution = 10 + time;
    }
    if(adc_resolution > 16) {
        as7331_divider_t divider = static_cast<as7331_divider_t>(adc_resolution - 17);
        setDivider(divider, true);
    } else if(_enable_divider) {
        setDivider(DIV_2, false);
    }
    */

    as7331_creg1_reg_t creg1;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Ensure we are in configuration mode
    if(!setDeviceMode(DEVICE_MODE_CONFIG)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to set device mode to configuration.");
        return false;
    }

    // Read CREG1
    if(!readRegister(RegCfgCreg1, creg1.byte)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read CREG1 register from device.");
        return false;
    }

    // Write new integration time value
    creg1.integration_time = time;
    if(!writeRegister(RegCfgCreg1, creg1.byte)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E(
            "AS7331",
            "Failed to write integration time value to device: %d.",
            creg1.integration_time);
        return false;
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    // Update local config
    _integration_time = time;

    return true;
}

// Set output divider (CREG2:DIV)
bool AS7331::setDivider(const as7331_divider_t& divider, const bool enable) {
    // Validate divider value
    if(divider > DIV_256) {
        FURI_LOG_E("AS7331", "Invalid divider value: %d.", divider);
        return false;
    }

    as7331_creg2_reg_t creg2;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Ensure we are in configuration mode
    if(!setDeviceMode(DEVICE_MODE_CONFIG)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to set device mode to configuration.");
        return false;
    }

    // Read CREG2
    if(!readRegister(RegCfgCreg2, creg2.byte)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read CREG2 register from device.");
        return false;
    }

    // Write new divider value and enable/disable it
    creg2.divider = divider;
    creg2.enable_divider = enable;
    if(!writeRegister(RegCfgCreg2, creg2.byte)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E(
            "AS7331",
            "Failed to write divider settings to device: DIV=%d, ENABLE=%d.",
            creg2.divider,
            creg2.enable_divider);
        return false;
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    // Update local config
    _enable_divider = enable;
    _divider = divider;

    return true;
}

// Set clock frequency (CREG3:CCLK)
bool AS7331::setClockFrequency(const as7331_clock_frequency_t& frequency) {
    // Validate clock frequency value
    if(frequency > CCLK_8_192_MHZ) {
        FURI_LOG_E("AS7331", "Invalid clock frequency value: %d.", frequency);
        return false;
    }

    as7331_creg3_reg_t creg3;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Ensure we are in configuration mode
    if(!setDeviceMode(DEVICE_MODE_CONFIG)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to set device mode to configuration.");
        return false;
    }

    // Read CREG3
    if(!readRegister(RegCfgCreg3, creg3.byte)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read CREG3 register from device.");
        return false;
    }

    // Write new clock frequency value
    creg3.clock_frequency = frequency;
    if(!writeRegister(RegCfgCreg3, creg3.byte)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E(
            "AS7331",
            "Failed to write clock frequency value to device: %d.",
            creg3.clock_frequency);
        return false;
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    // Update local config
    _clock_frequency = frequency;

    return true;
}

// Set measurement mode (CREG3:MMODE)
bool AS7331::setMeasurementMode(const as7331_measurement_mode_t& mode) {
    // Validate measurement mode value
    if(mode > MEASUREMENT_MODE_SYNC_START_END) {
        FURI_LOG_E("AS7331", "Invalid measurement mode value: %d.", mode);
        return false;
    }

    as7331_creg3_reg_t creg3;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Ensure we are in configuration mode
    if(!setDeviceMode(DEVICE_MODE_CONFIG)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to set device mode to configuration.");
        return false;
    }

    // Read CREG3
    if(!readRegister(RegCfgCreg3, creg3.byte)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read CREG3 register from device.");
        return false;
    }

    // Write new measurement mode value
    creg3.measurement_mode = mode;
    if(!writeRegister(RegCfgCreg3, creg3.byte)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E(
            "AS7331", "Failed to write measurement mode to device: %d.", creg3.measurement_mode);
        return false;
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    // Update local config
    _measurement_mode = mode;

    return true;
}

// Enable or disable power down state (OSR:PD)
bool AS7331::setPowerDown(const bool& power_down) {
    // The power-down feature affects both operational states: configuration and measurement.
    // When the power-down state is activated while the device is in a measurement cycle,
    // the power-down action is only executed during the intervals between consecutive conversions.

    // Power Down Current Consumption: max 1µA
    // Startup Time after Power Down state: 1.2-2ms

    as7331_osr_reg_t osr;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // OSR is accessible in any mode
    // Read OSR register
    if(!readOSRRegister(osr)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read OSR register from device.");
        return false;
    }

    // Check if the desired power down state is already set
    if(osr.power_down != power_down) {
        osr.power_down = power_down; // Set PD bit

        // Write back the OSR register
        // Local configuration (_power_down) is updated in writeOSRRegister
        if(!writeOSRRegister(osr)) {
            furi_hal_i2c_release(&furi_hal_i2c_handle_external);
            FURI_LOG_E("AS7331", "Failed to write power down state to device: %d.", power_down);
            return false;
        }
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return true;
}

// Put the sensor in standby mode or wake it up (CREG3:SB)
bool AS7331::setStandby(const bool& standby) {
    // Standby Current Consumption: max 970 µA
    // Startup Time after Standby state: 4-5 µs

    as7331_creg3_reg_t creg3;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Ensure we are in configuration mode
    if(!setDeviceMode(DEVICE_MODE_CONFIG)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to set device mode to configuration.");
        return false;
    }

    // Read CREG3
    if(!readRegister(RegCfgCreg3, creg3.byte)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read CREG3 register from device.");
        return false;
    }

    // Check if the desired standby state is already set
    if(creg3.standby != standby) {
        creg3.standby = standby; // Set SB bit

        // Write back the CREG3 register
        if(!writeRegister(RegCfgCreg3, creg3.byte)) {
            furi_hal_i2c_release(&furi_hal_i2c_handle_external);
            FURI_LOG_E("AS7331", "Failed to write standby state to device: %d.", standby);
            return false;
        }
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    // Update local config
    _standby = standby;

    return true;
}

// Start the measurement (OSR:SS = 1)
bool AS7331::startMeasurement() {
    as7331_osr_reg_t osr;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Ensure we are in measurement mode
    if(!setDeviceMode(DEVICE_MODE_MEASURE)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to set device mode to measurement.");
        return false;
    }

    // Read OSR register
    if(!readOSRRegister(osr)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read OSR register from device.");
        return false;
    }

    // Check if measurement is already started
    if(osr.start_state != 1) {
        osr.start_state = 1; // Set SS bit to start measurement

        // Write back OSR register
        if(!writeOSRRegister(osr)) {
            furi_hal_i2c_release(&furi_hal_i2c_handle_external);
            FURI_LOG_E("AS7331", "Failed to write OSR register to start measurement.");
            return false;
        }
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return true;
}

// Stop the measurement (OSR:SS = 0)
bool AS7331::stopMeasurement() {
    as7331_osr_reg_t osr;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Read OSR register
    if(!readOSRRegister(osr)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read OSR register from device.");
        return false;
    }

    // Check if measurement is already stopped
    if(osr.start_state != 0) {
        osr.start_state = 0; // Clear SS bit to stop measurement

        // Write back OSR register
        if(!writeOSRRegister(osr)) {
            furi_hal_i2c_release(&furi_hal_i2c_handle_external);
            FURI_LOG_E("AS7331", "Failed to write OSR register to stop measurement.");
            return false;
        }
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return true;
}

// Wait for measurement to complete based on conversion time
void AS7331::waitForMeasurement() {
    double TCONV = getConversionTime();

    if(TCONV > 0) {
        // Wait for TCONV time, adding a small buffer
        furi_delay_ms(static_cast<uint32_t>(TCONV * 1000) + 1); // Add 1 ms to ensure completion
    }
}

// Get raw measurement results
bool AS7331::getRawResults(RawResults& rawResults) {
    uint8_t buffer[6]; // Each result is 2 bytes, so 3 results * 2 bytes = 6 bytes

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Ensure we are in measurement mode
    if(!setDeviceMode(DEVICE_MODE_MEASURE)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to set device mode to measurement.");
        return false;
    }

    // Read all measurement results in one I²C transaction
    if(!readRegisters(RegMeasResultA, buffer, 6)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read measurement results from device.");
        return false;
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    // Combine the bytes into 16-bit values (LSB first)
    rawResults.uv_a = (static_cast<uint16_t>(buffer[1]) << 8) | buffer[0];
    rawResults.uv_b = (static_cast<uint16_t>(buffer[3]) << 8) | buffer[2];
    rawResults.uv_c = (static_cast<uint16_t>(buffer[5]) << 8) | buffer[4];

    return true;
}

// Get processed measurement results with raw data
bool AS7331::getResults(Results& results, RawResults& rawResults) {
    bool success = true;

    // Get the raw measurement results
    success &= getRawResults(rawResults);

    if(!success) {
        return false;
    }

    // Calculate FSREe for each channel
    double FSREe_A = calculateFSREe(UV_A, false);
    double FSREe_B = calculateFSREe(UV_B, false);
    double FSREe_C = calculateFSREe(UV_C, false);
    FURI_LOG_I("AS7331", "FSREe_A: %f", FSREe_B);
    FURI_LOG_I("AS7331", "FSREe_B: %f", FSREe_C);
    FURI_LOG_I("AS7331", "FSREe_C: %f", FSREe_C);

    if(FSREe_A < 0 || FSREe_B < 0 || FSREe_C < 0) {
        // Invalid FSREe calculation
        return false;
    }

    // Adjust FSREe for clock frequency
    // Since FSREe is inversely proportional to fCLK,
    // we adjust FSREe by dividing by the clock frequency ratio
    double fCLK_ratio = static_cast<double>(1 << _clock_frequency); // fCLK_actual / fCLK_base

    FSREe_A /= fCLK_ratio;
    FSREe_B /= fCLK_ratio;
    FSREe_C /= fCLK_ratio;

    // Get NCLK from TIME_code
    uint32_t NCLK;
    if(_integration_time == 15) {
        NCLK = 1024;
    } else if(_integration_time <= 14) {
        NCLK = 1 << (10 + _integration_time);
    } else {
        return false; // Invalid TIME_code
    }

    // Adjust NCLK and FSREe for Divider if enabled
    if(_enable_divider) {
        int divider_factor = 1 << (_divider + 1); // Divider factor is 2^(DIV + 1)
        NCLK /= divider_factor; // Adjust NCLK
        // Alternatively, we could adjust MRES, but adjusting NCLK is equivalent and simpler
    }

    // Calculate LSB for each channel
    double LSB_A = FSREe_A / NCLK;
    double LSB_B = FSREe_B / NCLK;
    double LSB_C = FSREe_C / NCLK;

    // Calculate Ee (Irradiance in µW/cm²) for each channel
    results.uv_a = rawResults.uv_a * LSB_A;
    results.uv_b = rawResults.uv_b * LSB_B;
    results.uv_c = rawResults.uv_c * LSB_C;

    return true;
}

// Get processed measurement results
bool AS7331::getResults(Results& results) {
    RawResults rawResults; // Temporary variable
    return getResults(results, rawResults); // Call the core function
}

// Read temperature measurement (16-bit)
bool AS7331::getTemperature(double& temperature) {
    uint16_t temperature_raw;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Ensure we are in measurement mode
    if(!setDeviceMode(DEVICE_MODE_MEASURE)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to set device mode to measurement.");
        return false;
    }

    // Read temperature register
    if(!readRegister16(RegMeasTemp, temperature_raw)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read temperature register from device.");
        return false;
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    // Calculate temperature in Celsius (Datasheet p. 42)
    temperature = temperature_raw * 0.05 - 66.9; // [°C]

    return true;
}

// Check if the device is ready
bool AS7331::deviceReady(const uint8_t& i2c_address_7bit) {
    uint8_t i2c_address_8bit;

    if(i2c_address_7bit == 0) {
        i2c_address_8bit = _i2c_addr_8bit;
    } else {
        i2c_address_8bit = i2c_address_7bit << 1; // Convert 7-bit to 8-bit address
    }

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    bool ready = furi_hal_i2c_is_device_ready(
        &furi_hal_i2c_handle_external, i2c_address_8bit, AS7331_I2C_TIMEOUT);

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return ready;
}

// Perform a software reset (OSR:SW_RES = 1)
bool AS7331::reset() {
    // Setting SW_RES to '1' triggers a software reset of the AS7331.
    as7331_osr_reg_t osr;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // OSR is accessible in any mode
    if(!readOSRRegister(osr)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read OSR register from device.");
        return false;
    }

    osr.software_reset = 1; // Set SW_RES bit

    if(!writeOSRRegister(osr)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to write OSR register to reset device.");
        return false;
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    // Update local configuration variables from device registers
    _deviceMode = DEVICE_MODE_CONFIG;
    if(!updateLocalConfig()) {
        FURI_LOG_E("AS7331", "Failed to read device configuration registers.");
        return false;
    }

    return true;
}

// Getter methods

// Read device ID (including mutation number)
bool AS7331::getDeviceID(as7331_agen_reg_t& deviceID) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    if(!readRegister(RegCfgAgen, deviceID.byte)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read device ID register from device.");
        return false;
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return true;
}

// Read status register in measurement mode
bool AS7331::getStatus(as7331_osr_status_reg_t& status) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Ensure we are in measurement mode
    if(!setDeviceMode(DEVICE_MODE_MEASURE)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to set device mode to measurement.");
        return false;
    }

    if(!readRegister16(RegMeasOsrStatus, status.word)) {
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        FURI_LOG_E("AS7331", "Failed to read status register from device.");
        return false;
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return true;
}

// Local config getter methods

// Getter for gain code
as7331_gain_t AS7331::getGain() const {
    return _gain;
}

// Getter for actual gain value
int16_t AS7331::getGainValue() const {
    // Gain value is calculated using the formula: gain = 2^(11 - gain_code)
    return 1 << (11 - _gain);
}

// Getter for integration time code
as7331_integration_time_t AS7331::getIntegrationTime() const {
    return _integration_time;
}

// Get conversion time in seconds
double AS7331::getConversionTime() const {
    // Base clock frequency is 1.024 MHz
    double fCLK_base = 1.024e6; // Hz

    // Actual clock frequency based on CCLK code
    double fCLK = fCLK_base * (1 << _clock_frequency);

    // Calculate NCLK based on TIME code
    uint32_t NCLK; // Number of clock cycles within conversion time
    if(_integration_time == 15) {
        NCLK = 1024; // Special case: Same as TIME 0
    } else if(_integration_time <= 14) {
        NCLK = 1 << (10 + _integration_time);
    } else {
        // Invalid TIME code
        return -1.0;
    }

    // Conversion time TCONV
    double TCONV = static_cast<double>(NCLK) / fCLK; // in seconds

    return TCONV;
}

// Getter for divider code
as7331_divider_t AS7331::getDivider() const {
    return _divider;
}

// Getter for divider enabled state
bool AS7331::isDividerEnabled() const {
    return _enable_divider;
}

// Getter for actual divider value
uint8_t AS7331::getDividerValue() const {
    // Divider value is calculated using the formula: divider = 2^(1 + divider_code)
    uint8_t divider_value = 1 << (1 + _divider);
    return divider_value;
}

// Getter for clock frequency code
as7331_clock_frequency_t AS7331::getClockFrequency() const {
    return _clock_frequency;
}

// Getter for actual clock frequency value
double AS7331::getClockFrequencyValue() const {
    // Clock frequency is calculated using the formula: fCLK = 1.024 * 2^clock_frequency_code (in MHz)
    uint32_t freq_multiplier = 1 << _clock_frequency; // 2^clock_frequency_code
    double clock_frequency_value = 1.024 * freq_multiplier; // in MHz
    return clock_frequency_value;
}

// Getter for measurement mode
as7331_measurement_mode_t AS7331::getMeasurementMode() const {
    return _measurement_mode;
}

// Getter for power down state
bool AS7331::isPowerDown() const {
    return _power_down;
}

// Getter for standby state
bool AS7331::isStandby() const {
    return _standby;
}

// Internal helper methods

// Set the device mode
bool AS7331::setDeviceMode(const as7331_device_mode_t& mode) {
    if(mode != DEVICE_MODE_CONFIG && mode != DEVICE_MODE_MEASURE) {
        FURI_LOG_E("AS7331", "Invalid device mode value: %d.", mode);
        return false;
    }
    if(_deviceMode == mode) {
        return true; // Already in desired mode
    }

    // Read OSR register
    as7331_osr_reg_t osr;
    if(!readOSRRegister(osr)) {
        FURI_LOG_E("AS7331", "Failed to read OSR register from device.");
        return false;
    }

    // Set operating state (DOS bits)
    osr.operating_state = mode;

    // If transitioning to Measurement state, ensure power-down is disabled
    if(mode == DEVICE_MODE_MEASURE) {
        osr.power_down = 0;
    }

    // Write back OSR register
    // '_deviceMode' is updated in writeOSRRegister
    if(!writeOSRRegister(osr)) {
        FURI_LOG_E("AS7331", "Failed to write OSR register to set device mode.");
        return false;
    }

    return true;
}

// Read OSR register and update local config
bool AS7331::readOSRRegister(as7331_osr_reg_t& osr) {
    if(!readRegister(RegCfgOsr, osr.byte)) {
        FURI_LOG_E("AS7331", "Failed to read OSR Register from device.");
        return false;
    }

    if(osr.operating_state != DEVICE_MODE_CONFIG && osr.operating_state != DEVICE_MODE_MEASURE) {
        FURI_LOG_E(
            "AS7331", "Invalid operating state value read from device: %d.", osr.operating_state);
        return false;
    }

    // Update local config
    _deviceMode = osr.operating_state;
    _power_down = osr.power_down;

    return true;
}

// Write OSR register and update local config
bool AS7331::writeOSRRegister(const as7331_osr_reg_t& osr) {
    if(osr.operating_state != DEVICE_MODE_CONFIG && osr.operating_state != DEVICE_MODE_MEASURE) {
        FURI_LOG_E("AS7331", "Invalid device mode value: %d.", osr.operating_state);
        return false;
    }

    // Write back OSR register
    if(!writeRegister(RegCfgOsr, osr.byte)) {
        FURI_LOG_E("AS7331", "Failed to write OSR to device: %d.", osr.byte);
        return false;
    }

    // Update local config
    _deviceMode = osr.operating_state;
    _power_down = osr.power_down;

    return true;
}

// Generalized method to read multiple bytes from consecutive registers
bool AS7331::readRegisters(uint8_t start_register_addr, uint8_t* buffer, size_t length) {
    bool success;

    // Write Phase: Send the start register address, end with AwaitRestart
    success = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        _i2c_addr_8bit,
        false, // 7-bit address
        &start_register_addr,
        1,
        FuriHalI2cBeginStart,
        FuriHalI2cEndAwaitRestart, // Prepare for repeated start
        AS7331_I2C_TIMEOUT);
    if(!success) {
        FURI_LOG_E("AS7331", "Failed to send start register address");
        return false;
    }

    // Read Phase: Read the data, begin with Restart, end with Stop
    success = furi_hal_i2c_rx_ext(
        &furi_hal_i2c_handle_external,
        _i2c_addr_8bit,
        false, // 7-bit address
        buffer,
        length,
        FuriHalI2cBeginRestart, // Repeated start
        FuriHalI2cEndStop,
        AS7331_I2C_TIMEOUT);
    if(!success) {
        FURI_LOG_E("AS7331", "Failed to read data");
        return false;
    }

    return true;
}

// Function to read an 8-bit register
bool AS7331::readRegister(uint8_t register_addr, uint8_t& data) {
    return readRegisters(register_addr, &data, 1);
}

// Function to read a 16-bit register
bool AS7331::readRegister16(uint8_t register_addr, uint16_t& data) {
    uint8_t buffer[2];
    bool success = readRegisters(register_addr, buffer, 2);
    if(!success) {
        return false;
    }

    // Combine the two bytes into a 16-bit value (LSB first)
    data = (static_cast<uint16_t>(buffer[1]) << 8) | buffer[0];

    return true;
}

// Function to write an 8-bit register
bool AS7331::writeRegister(uint8_t register_addr, const uint8_t& data) {
    bool success;
    uint8_t buffer[2] = {register_addr, data};

    // Write the register address and data in one transaction
    success = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        _i2c_addr_8bit,
        false, // 7-bit address
        buffer,
        2,
        FuriHalI2cBeginStart,
        FuriHalI2cEndStop,
        AS7331_I2C_TIMEOUT);
    if(!success) {
        FURI_LOG_E("AS7331", "Failed to write data");
        return false;
    }

    return true;
}

// Scan I2C bus for AS7331 device
uint8_t AS7331::scan_i2c_bus() {
    uint8_t addr_found = 0; // Will hold the address if found

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Iterate through possible I2C addresses
    for(uint8_t addr_7bit = DefaultI2CAddr; addr_7bit <= QuaternaryI2CAddr; addr_7bit++) {
        uint8_t addr_8bit = addr_7bit << 1;

        // Check for device readiness
        if(furi_hal_i2c_is_device_ready(
               &furi_hal_i2c_handle_external, addr_8bit, AS7331_I2C_TIMEOUT)) {
            addr_found = addr_7bit;
            break;
        }
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if(addr_found == 0) {
        FURI_LOG_E("AS7331", "No AS7331 device found on I²C bus.");
    }

    return addr_found;
}

// Update local configuration variables from device registers
bool AS7331::updateLocalConfig() {
    bool success = true;
    as7331_creg1_reg_t creg1;
    as7331_creg2_reg_t creg2;
    as7331_creg3_reg_t creg3;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Ensure we are in configuration mode
    success &= setDeviceMode(DEVICE_MODE_CONFIG);

    // Read configuration registers
    success &= readRegister(RegCfgCreg1, creg1.byte);
    success &= readRegister(RegCfgCreg2, creg2.byte);
    success &= readRegister(RegCfgCreg3, creg3.byte);

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if(!success) {
        FURI_LOG_E("AS7331", "Failed to read configuration registers.");
        return false;
    }

    // Validate and populate local configuration variables
    if(creg1.gain <= GAIN_1) {
        _gain = static_cast<as7331_gain_t>(creg1.gain);
    } else {
        FURI_LOG_E("AS7331", "Invalid gain value read from device: %d.", creg1.gain);
        return false;
    }

    if(creg1.integration_time <= TIME_16384MS) {
        _integration_time = static_cast<as7331_integration_time_t>(creg1.integration_time);
    } else {
        FURI_LOG_E(
            "AS7331",
            "Invalid integration time value read from device: %d.",
            creg1.integration_time);
        return false;
    }

    _enable_divider = creg2.enable_divider;

    if(creg2.divider <= DIV_256) {
        _divider = static_cast<as7331_divider_t>(creg2.divider);
    } else {
        FURI_LOG_E("AS7331", "Invalid divider value read from device: %d.", creg2.divider);
        return false;
    }

    if(creg3.clock_frequency <= CCLK_8_192_MHZ) {
        _clock_frequency = static_cast<as7331_clock_frequency_t>(creg3.clock_frequency);
    } else {
        FURI_LOG_E(
            "AS7331",
            "Invalid clock frequency value read from device: %d.",
            creg3.clock_frequency);
        return false;
    }

    _standby = creg3.standby;

    if(creg3.measurement_mode <= MEASUREMENT_MODE_SYNC_START_END) {
        _measurement_mode = static_cast<as7331_measurement_mode_t>(creg3.measurement_mode);
    } else {
        FURI_LOG_E(
            "AS7331",
            "Invalid measurement mode value read from device: %d.",
            creg3.measurement_mode);
        return false;
    }

    // _deviceMode and _power_down is updated in readOSRRegister()/writeOSRRegister()

    return true;
}

/**
 * @brief Calculates the Full-Scale Range irradiance (FSREe) for a given UV type.
 *
 * This function computes the FSREe, which is the maximum detectable irradiance (Ee) that the sensor can measure without saturating the ADC, based on the selected GAIN settings. Optionally, it can adjust FSREe for the integration TIME setting.
 *
 * **Definitions:**
 * - **FSR (FSREe in equations)**: Full-Scale Range—the maximum detectable irradiance (Ee) the sensor can measure without saturating the ADC, given the selected GAIN (and optionally TIME) settings.
 * - **LSB**: Least Significant Bit—the smallest change in irradiance that the sensor can detect with the selected settings. In the datasheet, it's given in nW/cm², but here we calculate it in µW/cm² (same as FSREe).
 * - **NCLK**: Number of clock cycles during the conversion.
 * - **MRES**: ADC count. The most straightforward way to get Ee is:
 *   - *Ee = MRES × LSB* (ADC count times the value of a single step)
 * - **LSB Calculation**:
 *   - *LSB = FSR / NCLK*
 *
 * **Note:**
 * - Both LSB and FSR depend on the clock frequency (fCLK); the tables in the datasheet assume an fCLK of 1.024 MHz.
 * - When calculating LSB, FSREe should **not** be adjusted for integration TIME, as higher TIME values increase the resolution beyond 16 bits, and only the least 16 bits are stored in the result registers.
 *
 * @param uvType The type of UV measurement (UV_A, UV_B, or UV_C).
 * @param adjustForIntegrationTime Optional. A boolean flag indicating whether to adjust FSREe for the integration TIME setting. Default is `true`.
 * @return The calculated FSREe value in µW/cm², or `-1.0` if an error occurs.
 */
double AS7331::calculateFSREe(as7331_uv_type_t uvType, bool adjustForIntegrationTime) {
    // Base FSREe at GAIN = 1x for each channel, from datasheet (p.32 - p.38)
    double FSREe_gain1;
    switch(uvType) {
    case UV_A: // Channel A
        FSREe_gain1 = 348160.0; // µW/cm² at GAIN=1x, TIME code 6
        break;
    case UV_B: // Channel B
        FSREe_gain1 = 387072.0; // µW/cm² at GAIN=1x, TIME code 6
        break;
    case UV_C: // Channel C
        FSREe_gain1 = 169984.0; // µW/cm² at GAIN=1x, TIME code 3
        break;
    default:
        // Invalid UV type
        return -1.0;
    }

    // Calculate the actual gain value
    int GAIN_value = getGainValue();
    if(GAIN_value <= 0) {
        return -1.0; // Invalid GAIN
    }

    // Calculate the base FSREe for the current gain setting
    double FSREe = FSREe_gain1 / GAIN_value;

    if(adjustForIntegrationTime) {
        // Adjust FSREe for integration TIME
        if(uvType == UV_A || uvType == UV_B) {
            if(_integration_time <= 6) {
                // No adjustment needed for TIME codes 0-6
            } else if(_integration_time <= 14) {
                // For each TIME code above 6, FSREe halves
                int time_diff = _integration_time - 6;
                FSREe /= (1 << time_diff); // FSREe /= 2^(TIME_code - 6)
            } else if(_integration_time == 15) {
                // Special case: TIME code 15 is equivalent to TIME code 0
                // No adjustment needed
            } else {
                return -1.0; // Invalid TIME code
            }
        } else if(uvType == UV_C) {
            if(_integration_time <= 3) {
                // No adjustment needed for TIME codes 0-3
            } else if(_integration_time <= 7) {
                // Divide FSREe by 2
                FSREe /= 2;
            } else if(_integration_time <= 14) {
                // For each TIME code above 7, FSREe halves
                int time_diff = _integration_time - 7;
                FSREe /= (1 << time_diff); // FSREe /= 2^(TIME_code - 3)
            } else if(_integration_time == 15) {
                // Special case: TIME code 15 is equivalent to TIME code 0
                // No adjustment needed
            } else {
                return -1.0; // Invalid TIME code
            }
        }
    }

    return FSREe;
}

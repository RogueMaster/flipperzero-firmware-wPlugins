#include "chip_db.h"
#include "i2c_worker.h"

#include <furi.h>
#include <string.h>

// Every constant below was checked against the manufacturer datasheet or the
// vendor's own driver. A wrong value here makes the app accuse a genuine
// sensor of being counterfeit, which is worse than not supporting the chip,
// so anything that could not be pinned down was left out.
//
// Sources: Bosch BST-BNO055-DS000, BST-BMP280-DS001, BST-BME280-DS002,
// BMP3/BME68x/BMI160/BMI270/BMI08x SensorAPI headers; InvenSense/TDK
// MPU-6050, MPU-6886, ICM-20948 (DS-000189), ICM-42605 (DS-000292),
// ICM-42688-P (DS-000347); ST LSM6DS3, LSM6DSO, LSM6DSV16X, LIS3DH, LIS3MDL,
// LIS2MDL, LPS22HB, VL53L0X; QST QMC5883L, QMI8658; Honeywell HMC5883L;
// ams TCS3472, TSL2591; Broadcom APDS-9960; Lite-On LTR-390UV; Memsic
// MMC5603NJ; ADI ADXL345, ADXL355, MAX30102; TI INA226/228/260, TMP117;
// Infineon DPS310; ScioSense ENS160; Hynitron CST816S (community drivers).

#define M8  0x00FF // convenience: full 8-bit mask
#define M16 0xFFFF

// BNO055: checking all four sub-IDs is the strongest fake test — clones get
// CHIP_ID right but rarely the BMA280/BMM150/BMG160 sub-IDs.
static const IdCheck bno055_checks[] = {
    {0x00, 0xA0, M8, false, false}, // CHIP_ID
    {0x01, 0xFB, M8, false, false}, // ACC_ID  (BMA280)
    {0x02, 0x32, M8, false, false}, // MAG_ID  (BMM150)
    {0x03, 0x0F, M8, false, false}, // GYR_ID  (BMG160)
};

/* --- pressure / environmental --- */
static const IdCheck bmp280_checks[] = {{0xD0, 0x58, M8, false, false}};
static const IdCheck bme280_checks[] = {{0xD0, 0x60, M8, false, false}};
static const IdCheck bmp180_checks[] = {{0xD0, 0x55, M8, false, false}};
static const IdCheck bmp388_checks[] = {{0x00, 0x50, M8, false, false}};
static const IdCheck bmp390_checks[] = {{0x00, 0x60, M8, false, false}};
// BME680 and BME688 share CHIP_ID 0x61; the variant register separates them.
static const IdCheck bme680_checks[] = {
    {0xD0, 0x61, M8, false, false},
    {0xF0, 0x00, M8, false, false}};
static const IdCheck bme688_checks[] = {
    {0xD0, 0x61, M8, false, false},
    {0xF0, 0x01, M8, false, false}};
static const IdCheck dps310_checks[] = {{0x0D, 0x10, M8, false, false}};
static const IdCheck ccs811_checks[] = {{0x20, 0x81, M8, false, false}};
static const IdCheck ens160_checks[] = {
    {0x00, 0x60, M8, false, false},
    {0x01, 0x01, M8, false, false}};
// HDC1080: 16-bit registers, manufacturer ID reads ASCII "TI".
static const IdCheck hdc1080_checks[] = {
    {0xFE, 0x5449, M16, true, false},
    {0xFF, 0x1050, M16, true, false}};

/* --- IMU --- */
static const IdCheck mpu6050_checks[] = {{0x75, 0x68, M8, false, false}};
static const IdCheck mpu6500_checks[] = {{0x75, 0x70, M8, false, false}};
static const IdCheck mpu9250_checks[] = {{0x75, 0x71, M8, false, false}};
static const IdCheck mpu6886_checks[] = {{0x75, 0x19, M8, false, false}};
static const IdCheck icm42605_checks[] = {{0x75, 0x42, M8, false, false}};
static const IdCheck icm42688_checks[] = {{0x75, 0x47, M8, false, false}};
static const IdCheck icm20948_checks[] = {{0x00, 0xEA, M8, false, false}};
static const IdCheck bmi160_checks[] = {{0x00, 0xD1, M8, false, false}};
static const IdCheck bmi270_checks[] = {{0x00, 0x24, M8, false, false}};
static const IdCheck bmi088a_checks[] = {{0x00, 0x1E, M8, false, false}};
static const IdCheck bmi088g_checks[] = {{0x00, 0x0F, M8, false, false}};
static const IdCheck lsm6ds3_checks[] = {{0x0F, 0x69, M8, false, false}};
static const IdCheck lsm6ds3trc_checks[] = {{0x0F, 0x6A, M8, false, false}};
static const IdCheck lsm6dso_checks[] = {{0x0F, 0x6C, M8, false, false}};
static const IdCheck lsm6dsv_checks[] = {{0x0F, 0x70, M8, false, false}};
static const IdCheck qmi8658_checks[] = {{0x00, 0x05, M8, false, false}};
static const IdCheck lis3dh_checks[] = {{0x0F, 0x33, M8, false, false}};
static const IdCheck adxl345_checks[] = {{0x00, 0xE5, M8, false, false}};
static const IdCheck adxl355_checks[] = {
    {0x00, 0xAD, M8, false, false},
    {0x01, 0x1D, M8, false, false},
    {0x02, 0xED, M8, false, false},
};

/* --- magnetometers --- */
static const IdCheck lis3mdl_checks[] = {{0x0F, 0x3D, M8, false, false}};
static const IdCheck lis2mdl_checks[] = {{0x4F, 0x40, M8, false, false}};
static const IdCheck mmc5603_checks[] = {{0x39, 0x10, M8, false, false}};
// HMC5883L identification registers A/B/C spell "H43".
static const IdCheck hmc5883l_checks[] = {
    {0x0A, 0x48, M8, false, false},
    {0x0B, 0x34, M8, false, false},
    {0x0C, 0x33, M8, false, false},
};
static const IdCheck qmc5883l_checks[] = {{0x0D, 0xFF, M8, false, false}};

/* --- light / proximity / ToF --- */
// ST time-of-flight parts index their registers with a 16-bit address.
static const IdCheck vl6180x_checks[] = {{0x0000, 0xB4, M8, false, true}};
static const IdCheck vl53l1x_checks[] = {
    {0x010F, 0xEA, M8, false, true}, // MODEL_ID
    {0x0110, 0xCC, M8, false, true}, // MODULE_TYPE
};

static const IdCheck vl53l0x_checks[] = {
    {0xC0, 0xEE, M8, false, false}}; // IDENTIFICATION_MODEL_ID
// TCS34725 and TSL2591 need the command bit set in the register byte.
static const IdCheck tcs34725_checks[] = {{0x92, 0x44, M8, false, false}};
static const IdCheck tsl2591_checks[] = {{0xB2, 0x50, M8, false, false}};
static const IdCheck apds9960_checks[] = {{0x92, 0xAB, M8, false, false}};
static const IdCheck ltr390_checks[] = {
    {0x06, 0xB0, 0x00F0, false, false}}; // low nibble = revision
static const IdCheck max30102_checks[] = {{0xFF, 0x15, M8, false, false}};

/* --- power / temperature --- */
static const IdCheck ina226_checks[] = {
    {0xFE, 0x5449, M16, true, false},
    {0xFF, 0x2260, M16, true, false}};
static const IdCheck ina260_checks[] = {
    {0xFE, 0x5449, M16, true, false},
    {0xFF, 0x2270, M16, true, false}};
static const IdCheck ina228_checks[] = {{0x3E, 0x5449, M16, true, false}};
static const IdCheck tmp117_checks[] = {
    {0x0F, 0x0117, 0x0FFF, true, false}}; // top nibble = revision
static const IdCheck lps22hb_checks[] = {{0x0F, 0xB1, M8, false, false}};
static const IdCheck lps25hb_checks[] = {{0x0F, 0xBD, M8, false, false}};

/* --- touch --- */
static const IdCheck cst816s_checks[] = {{0xA7, 0xB4, M8, false, false}};

static const ChipEntry chip_db[] = {
    /* name, addrs, range_lo, range_hi, checks, count, note */
    {"BNO055", "9-axis IMU + fusion", {0x28, 0x29, 0xFF}, 0, 0, bno055_checks, 4, NULL},
    {"BMP280", "Pressure sensor", {0x76, 0x77, 0xFF}, 0, 0, bmp280_checks, 1, NULL},
    {"BME280", "Press/temp/humidity", {0x76, 0x77, 0xFF}, 0, 0, bme280_checks, 1, NULL},
    {"BMP180", "Pressure sensor", {0x77, 0xFF}, 0, 0, bmp180_checks, 1, NULL},
    {"BMP388", "Pressure sensor", {0x76, 0x77, 0xFF}, 0, 0, bmp388_checks, 1, NULL},
    {"BMP390", "Pressure sensor", {0x76, 0x77, 0xFF}, 0, 0, bmp390_checks, 1, NULL},
    {"BME680", "Air quality + climate", {0x76, 0x77, 0xFF}, 0, 0, bme680_checks, 2, NULL},
    {"BME688", "Air quality + climate", {0x76, 0x77, 0xFF}, 0, 0, bme688_checks, 2, NULL},
    {"DPS310", "Pressure sensor", {0x76, 0x77, 0xFF}, 0, 0, dps310_checks, 1, NULL},
    {"CCS811",
     "Air quality (VOC)",
     {0x5A, 0x5B, 0xFF},
     0,
     0,
     ccs811_checks,
     1,
     "EOL part, clones common"},
    {"ENS160", "Air quality (VOC)", {0x52, 0x53, 0xFF}, 0, 0, ens160_checks, 2, NULL},
    {"HDC1080", "Temp + humidity", {0x40, 0xFF}, 0, 0, hdc1080_checks, 2, NULL},

    {"MPU6050", "6-axis IMU", {0x68, 0x69, 0xFF}, 0, 0, mpu6050_checks, 1, "TDK EOL, old stock"},
    {"MPU6500", "6-axis IMU", {0x68, 0x69, 0xFF}, 0, 0, mpu6500_checks, 1, "often sold as MPU9250"},
    {"MPU9250", "9-axis IMU", {0x68, 0x69, 0xFF}, 0, 0, mpu9250_checks, 1, "TDK EOL, often faked"},
    {"MPU6886", "6-axis IMU", {0x68, 0x69, 0xFF}, 0, 0, mpu6886_checks, 1, NULL},
    {"ICM20948", "9-axis IMU", {0x68, 0x69, 0xFF}, 0, 0, icm20948_checks, 1, NULL},
    {"ICM42605", "6-axis IMU", {0x68, 0x69, 0xFF}, 0, 0, icm42605_checks, 1, NULL},
    {"ICM42688P", "6-axis IMU", {0x68, 0x69, 0xFF}, 0, 0, icm42688_checks, 1, NULL},
    {"BMI160", "6-axis IMU", {0x68, 0x69, 0xFF}, 0, 0, bmi160_checks, 1, NULL},
    {"BMI270", "6-axis IMU", {0x68, 0x69, 0xFF}, 0, 0, bmi270_checks, 1, NULL},
    {"BMI088 gyro", "Gyroscope", {0x68, 0x69, 0xFF}, 0, 0, bmi088g_checks, 1, NULL},
    {"BMI088 accel", "Accelerometer", {0x18, 0x19, 0xFF}, 0, 0, bmi088a_checks, 1, NULL},
    {"LSM6DS3", "6-axis IMU", {0x6A, 0x6B, 0xFF}, 0, 0, lsm6ds3_checks, 1, NULL},
    {"LSM6DS3TR-C", "6-axis IMU", {0x6A, 0x6B, 0xFF}, 0, 0, lsm6ds3trc_checks, 1, NULL},
    {"LSM6DSO/OX",
     "6-axis IMU",
     {0x6A, 0x6B, 0xFF},
     0,
     0,
     lsm6dso_checks,
     1,
     "DSO and DSOX share the ID"},
    {"LSM6DSV16X", "6-axis IMU", {0x6A, 0x6B, 0xFF}, 0, 0, lsm6dsv_checks, 1, NULL},
    {"QMI8658", "6-axis IMU", {0x6A, 0x6B, 0xFF}, 0, 0, qmi8658_checks, 1, NULL},
    {"LIS3DH/2DH12",
     "Accelerometer",
     {0x18, 0x19, 0xFF},
     0,
     0,
     lis3dh_checks,
     1,
     "same ID as LIS2DH12"},
    {"ADXL345/343", "Accelerometer", {0x53, 0x1D, 0xFF}, 0, 0, adxl345_checks, 1, NULL},
    {"ADXL355", "Accelerometer", {0x1D, 0x53, 0xFF}, 0, 0, adxl355_checks, 3, NULL},

    {"LIS3MDL", "Magnetometer", {0x1C, 0x1E, 0xFF}, 0, 0, lis3mdl_checks, 1, NULL},
    {"LIS2MDL", "Magnetometer", {0x1E, 0xFF}, 0, 0, lis2mdl_checks, 1, NULL},
    {"MMC5603", "Magnetometer", {0x30, 0xFF}, 0, 0, mmc5603_checks, 1, NULL},
    {"HMC5883L",
     "Magnetometer",
     {0x1E, 0xFF},
     0,
     0,
     hmc5883l_checks,
     3,
     "EOL since 2016, mostly fake"},
    {"QMC5883L", "Magnetometer", {0x0D, 0xFF}, 0, 0, qmc5883l_checks, 1, NULL},

    {"VL53L0X", "Laser rangefinder", {0x29, 0xFF}, 0, 0, vl53l0x_checks, 1, NULL},
    {"VL53L1X", "Laser rangefinder", {0x29, 0xFF}, 0, 0, vl53l1x_checks, 2, NULL},
    {"VL6180X", "Laser rangefinder", {0x29, 0xFF}, 0, 0, vl6180x_checks, 1, NULL},
    {"TCS34725", "Colour sensor", {0x29, 0xFF}, 0, 0, tcs34725_checks, 1, NULL},
    {"TSL2591", "Light sensor", {0x29, 0xFF}, 0, 0, tsl2591_checks, 1, NULL},
    {"APDS9960", "Gesture + colour", {0x39, 0xFF}, 0, 0, apds9960_checks, 1, NULL},
    {"LTR-390UV", "UV + light sensor", {0x53, 0xFF}, 0, 0, ltr390_checks, 1, NULL},
    {"MAX30102",
     "Heart rate / SpO2",
     {0x57, 0xFF},
     0,
     0,
     max30102_checks,
     1,
     "0x11 here = MAX30100 relabel"},

    {"INA226", "Current monitor", {0xFF}, 0x40, 0x4F, ina226_checks, 2, NULL},
    {"INA260", "Current monitor", {0xFF}, 0x40, 0x4F, ina260_checks, 2, NULL},
    {"INA228", "Current monitor", {0xFF}, 0x40, 0x4F, ina228_checks, 1, NULL},
    {"TMP117", "Temperature sensor", {0xFF}, 0x48, 0x4B, tmp117_checks, 1, NULL},
    {"LPS22HB", "Pressure sensor", {0x5C, 0x5D, 0xFF}, 0, 0, lps22hb_checks, 1, NULL},
    {"LPS25HB", "Pressure sensor", {0x5C, 0x5D, 0xFF}, 0, 0, lps25hb_checks, 1, NULL},
    {"CST816S", "Touch controller", {0x15, 0xFF}, 0, 0, cst816s_checks, 1, "sleeps until touched"},

    /* Chips with no readable ID register: presence is all we can prove. */
    {"DS3231", "Real-time clock", {0x68, 0xFF}, 0, 0, NULL, 0, NULL},
    {"DS1307", "Real-time clock", {0x68, 0xFF}, 0, 0, NULL, 0, NULL},
    {"PCF8563", "Real-time clock", {0x51, 0xFF}, 0, 0, NULL, 0, NULL},
    {"SSD1306/SH1106",
     "OLED display",
     {0x3C, 0x3D, 0xFF},
     0,
     0,
     NULL,
     0,
     "SH1106 fakes undetectable"},
    {"AHT10/AHT20", "Temp + humidity", {0x38, 0xFF}, 0, 0, NULL, 0, NULL},
    {"BH1750", "Light sensor", {0x23, 0x5C, 0xFF}, 0, 0, NULL, 0, NULL},
    {"SHT3x/SHT4x",
     "Temp + humidity",
     {0x44, 0x45, 0xFF},
     0,
     0,
     NULL,
     0,
     "grade relabels undetectable"},
    {"SCD4x", "CO2 sensor", {0x62, 0xFF}, 0, 0, NULL, 0, NULL},
    {"SGP30", "Air quality (VOC)", {0x58, 0xFF}, 0, 0, NULL, 0, NULL},
    {"SGP40/41", "Air quality (VOC)", {0x59, 0xFF}, 0, 0, NULL, 0, NULL},
    {"SCD30", "CO2 sensor", {0x61, 0xFF}, 0, 0, NULL, 0, NULL},
    {"Si7021/HTU21D", "Temp + humidity", {0x40, 0xFF}, 0, 0, NULL, 0, NULL},
    {"MLX90614", "IR thermometer", {0x5A, 0xFF}, 0, 0, NULL, 0, NULL},
    {"MLX90640", "Thermal camera", {0x33, 0xFF}, 0, 0, NULL, 0, NULL},
    {"AS5600", "Magnetic angle", {0x36, 0xFF}, 0, 0, NULL, 0, NULL},
    {"MAX17048", "Battery fuel gauge", {0x36, 0xFF}, 0, 0, NULL, 0, NULL},
    {"ADS1115", "ADC", {0xFF}, 0x48, 0x4B, NULL, 0, NULL},
    {"INA219", "Current monitor", {0xFF}, 0x40, 0x4F, NULL, 0, NULL},
    {"MCP23017", "GPIO expander", {0xFF}, 0x20, 0x27, NULL, 0, NULL},
    {"PCF8574", "GPIO expander", {0xFF}, 0x20, 0x27, NULL, 0, NULL},
    {"PCF8574A", "GPIO expander", {0xFF}, 0x38, 0x3F, NULL, 0, NULL},
    {"MCP4725", "DAC", {0xFF}, 0x60, 0x67, NULL, 0, NULL},
    {"PCA9685", "PWM / servo driver", {0x40, 0xFF}, 0, 0, NULL, 0, NULL},
    {"TCA9548A", "I2C multiplexer", {0xFF}, 0x70, 0x77, NULL, 0, NULL},
    {"AT24Cxx", "EEPROM memory", {0xFF}, 0x50, 0x57, NULL, 0, NULL},
    {"MS5611", "Pressure sensor", {0x76, 0x77, 0xFF}, 0, 0, NULL, 0, NULL},
    {"VEML6070", "UV sensor", {0x38, 0x39, 0xFF}, 0, 0, NULL, 0, NULL},
    {"MAX44009", "Light sensor", {0x4A, 0x4B, 0xFF}, 0, 0, NULL, 0, NULL},
    {"BNO085",
     "9-axis IMU + fusion",
     {0x4A, 0x4B, 0xFF},
     0,
     0,
     NULL,
     0,
     "SHTP protocol, no WHO_AM_I"},
};

#define CHIP_DB_COUNT (sizeof(chip_db) / sizeof(chip_db[0]))

size_t chip_db_count(void) {
    return CHIP_DB_COUNT;
}

static bool chip_has_addr(const ChipEntry* chip, uint8_t addr7) {
    if(chip->range_lo && addr7 >= chip->range_lo && addr7 <= chip->range_hi) return true;
    for(size_t i = 0; i < CHIP_MAX_ADDRS && chip->addrs[i] != 0xFF; i++) {
        if(chip->addrs[i] == addr7) return true;
    }
    return false;
}

// One ID register read, with a single retry. A marginal bus (long jumpers,
// weak pull-ups) can drop one transaction, and without the retry that single
// glitch would brand a genuine chip as counterfeit.
static bool read_id_reg(uint8_t addr7, const IdCheck* check, uint16_t* value) {
    size_t len = check->wide ? 2 : 1;
    for(uint8_t attempt = 0; attempt < 2; attempt++) {
        uint8_t buf[2] = {0};
        bool ok;
        if(check->reg16) {
            ok = i2c_worker_read_reg16_addr(addr7, check->reg, buf, len, I2C_REG_TIMEOUT_MS);
        } else if(check->wide) {
            ok = i2c_worker_read_mem(addr7, (uint8_t)check->reg, buf, len, I2C_REG_TIMEOUT_MS);
        } else {
            ok = i2c_worker_read_reg(addr7, (uint8_t)check->reg, buf, I2C_REG_TIMEOUT_MS);
        }
        if(ok) {
            *value = check->wide ? (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]) : buf[0];
            return true;
        }
        furi_delay_ms(2); // let a confused slave finish its previous transfer
    }
    return false;
}

// Reads every ID register of a candidate.
// Returns the number of matches, or -1 if any read failed.
static int32_t chip_try_candidate(const ChipEntry* chip, uint8_t addr7, IdReadResult* reads) {
    memset(reads, 0, sizeof(IdReadResult) * CHIP_MAX_CHECKS);

    int32_t matches = 0;
    bool all_reads_ok = true;
    for(uint8_t i = 0; i < chip->check_count && i < CHIP_MAX_CHECKS; i++) {
        const IdCheck* check = &chip->checks[i];
        uint16_t mask = check->mask ? check->mask : (check->wide ? 0xFFFF : 0x00FF);

        reads[i].reg = check->reg;
        reads[i].expected = check->expected;
        reads[i].wide = check->wide;
        reads[i].reg16 = check->reg16;
        reads[i].has_expected = true;
        reads[i].read_ok = read_id_reg(addr7, check, &reads[i].actual);

        if(!reads[i].read_ok) {
            all_reads_ok = false;
        } else if((reads[i].actual & mask) == (check->expected & mask)) {
            reads[i].match = true;
            matches++;
        }
    }
    return all_reads_ok ? matches : -1;
}

void chip_db_identify(uint8_t addr7, ChipIdentification* out) {
    // Zero here means VerdictNotChecked, not VerdictGenuine. Every return below
    // sets a verdict of its own, so this only ever shows if someone later adds
    // a path that forgets to -- and then it says "not checked" rather than
    // calling an unexamined part real.
    memset(out, 0, sizeof(*out));

    const ChipEntry* no_id_candidate = NULL;
    const ChipEntry* best_chip = NULL;
    IdReadResult best_reads[CHIP_MAX_CHECKS] = {0};
    uint8_t best_read_count = 0;
    int32_t best_matches = -1; // -1 = every read of every candidate failed
    bool any_read_ok = false;

    for(size_t i = 0; i < CHIP_DB_COUNT; i++) {
        const ChipEntry* chip = &chip_db[i];
        if(!chip_has_addr(chip, addr7)) continue;

        if(chip->checks == NULL) {
            if(!no_id_candidate) no_id_candidate = chip;
            continue;
        }

        IdReadResult reads[CHIP_MAX_CHECKS];
        int32_t matches = chip_try_candidate(chip, addr7, reads);
        for(uint8_t r = 0; r < chip->check_count && r < CHIP_MAX_CHECKS; r++) {
            if(reads[r].read_ok) any_read_ok = true;
        }

        if(matches == (int32_t)chip->check_count) {
            out->chip = chip;
            out->verdict = VerdictGenuine;
            memcpy(out->reads, reads, sizeof(reads));
            out->read_count = chip->check_count;
            return;
        }
        if(matches > best_matches) {
            best_matches = matches;
            best_chip = chip;
            memcpy(best_reads, reads, sizeof(reads));
            best_read_count = chip->check_count;
        }
    }

    // The device ACKed its address but no register read ever succeeded.
    // Report that honestly instead of guessing at a no-ID chip.
    if(!any_read_ok && (best_chip != NULL || no_id_candidate == NULL)) {
        out->chip = best_chip;
        out->verdict = VerdictNoAnswer;
        memcpy(out->reads, best_reads, sizeof(best_reads));
        out->read_count = best_read_count;
        return;
    }

    if(best_chip == NULL && no_id_candidate == NULL) {
        // Address is unknown: probe the common WHO_AM_I locations so the user
        // has raw bytes to search for.
        static const uint8_t probe_regs[] = {0x00, 0x0F, 0x75, 0xD0};
        out->verdict = VerdictUnknown;
        for(size_t i = 0; i < sizeof(probe_regs) && out->read_count < CHIP_MAX_CHECKS; i++) {
            IdReadResult* r = &out->reads[out->read_count];
            uint8_t byte = 0;
            r->reg = probe_regs[i];
            r->has_expected = false;
            r->read_ok = i2c_worker_read_reg(addr7, probe_regs[i], &byte, I2C_REG_TIMEOUT_MS);
            r->actual = byte;
            out->read_count++;
        }
        return;
    }

    if(no_id_candidate && best_matches <= 0) {
        // Reads worked but matched nothing, and a known chip without an ID
        // register lives here (DS3231 at 0x68, SSD1306 at 0x3C). Presence is
        // all we can honestly claim — never GENUINE without an ID to check.
        out->chip = no_id_candidate;
        out->verdict = VerdictDetectedNoId;
        memcpy(out->reads, best_reads, sizeof(best_reads));
        out->read_count = best_read_count;
        return;
    }

    memcpy(out->reads, best_reads, sizeof(best_reads));
    out->read_count = best_read_count;

    if(best_matches > 0) {
        // Some of a known chip's IDs match and the rest do not. A genuine part
        // has all of them, so this is real evidence of a counterfeit.
        out->chip = best_chip;
        out->verdict = VerdictWrongChip;
    } else {
        // Nothing matched at all. That is far more often a chip missing from
        // the database than a fake, so do not accuse it — show the bytes and
        // let the user look them up.
        out->chip = NULL;
        out->verdict = VerdictNoMatch;
    }
}

const char* chip_verdict_str(ChipVerdict verdict) {
    switch(verdict) {
    case VerdictNotChecked:
        return "NOT CHECKED";
    case VerdictGenuine:
        return "GENUINE";
    case VerdictWrongChip:
        return "LIKELY FAKE";
    case VerdictNoMatch:
        return "UNIDENTIFIED";
    case VerdictDetectedNoId:
        return "DETECTED (no ID reg)";
    case VerdictUnknown:
        return "UNKNOWN";
    case VerdictNoAnswer:
        return "NO ANSWER";
    default:
        return "?";
    }
}

const char* chip_verdict_headline(ChipVerdict verdict) {
    switch(verdict) {
    case VerdictNotChecked:
        return "NOT CHECKED";
    case VerdictGenuine:
        return "GENUINE";
    case VerdictWrongChip:
        return "LIKELY FAKE";
    case VerdictNoMatch:
        return "UNIDENTIFIED";
    case VerdictDetectedNoId:
        return "DETECTED";
    case VerdictUnknown:
        return "UNKNOWN";
    case VerdictNoAnswer:
        return "NO ANSWER";
    default:
        return "?";
    }
}

// The app can only ever say which chip the silicon identifies itself as. It
// cannot see the silkscreen, the listing or the packaging, so it must never
// imply it compared anything to a label — it asks the user to do that.
void chip_verdict_explain(ChipVerdict verdict, const char** line1, const char** line2) {
    switch(verdict) {
    case VerdictNotChecked:
        *line1 = "Nothing was checked yet.";
        *line2 = "This should not appear.";
        break;
    case VerdictGenuine:
        *line1 = "All ID registers match.";
        *line2 = "Does the label agree?";
        break;
    case VerdictWrongChip:
        *line1 = "Some IDs do not match.";
        *line2 = "A real one matches all.";
        break;
    case VerdictNoMatch:
        *line1 = "No known ID matched.";
        *line2 = "Unknown, or not in the DB.";
        break;
    case VerdictDetectedNoId:
        *line1 = "This chip has no ID reg.";
        *line2 = "Presence only, no proof.";
        break;
    case VerdictNoAnswer:
        *line1 = "It answers, reads fail.";
        *line2 = "Check pull-ups and wires.";
        break;
    default:
        *line1 = "Address not in database.";
        *line2 = "Raw bytes are in details.";
        break;
    }
}

bool chip_verdict_is_good(ChipVerdict verdict) {
    return verdict == VerdictGenuine || verdict == VerdictDetectedNoId;
}

const char* chip_verdict_short_str(ChipVerdict verdict) {
    switch(verdict) {
    case VerdictNotChecked:
        return "not checked";
    case VerdictGenuine:
        return "GENUINE";
    case VerdictWrongChip:
        return "FAKE?";
    case VerdictNoMatch:
        return "no match";
    case VerdictDetectedNoId:
        return "no ID reg";
    case VerdictUnknown:
        return "unknown";
    case VerdictNoAnswer:
        return "silent";
    default:
        return "?";
    }
}

const ChipEntry* chip_db_get(size_t index) {
    if(index >= CHIP_DB_COUNT) return NULL;
    return &chip_db[index];
}

// Pins that take a healthy part off the I2C bus. Same rule as the ID table
// above, and it matters more here: a wrong `i2c_high` sends someone to strap a
// pin the wrong way round, so a row nobody could verify is a row that is not
// here. Quotes are from the datasheet named on each line.
//
// Two things deliberately absent. Address-select pins (AD0, ADR, SDO used as
// an address bit) are not mode pins -- the sweep covers 0x08-0x77, so they
// cannot hide anything, and listing them would have the screen cry wolf. And
// the MPU6050 has no SPI at all; only the MPU6500/9250 siblings do.
//
// Still to verify before adding: BMP388, BMP390, BME688, BMI160, BMI270,
// BMI088, MPU6500, MPU9250, MPU6886, ICM20948, ICM42605, ICM42688P,
// LSM6DS3TR-C, LSM6DSO/OX, LSM6DSV16X, LIS3MDL, LIS2MDL, LPS22HB, LPS25HB,
// ADXL355, and XSHUT on the VL53/VL6180X family.
static const ChipModePin chip_mode_pins[] = {
    // Bosch BST-BNO055-DS000 Table 4-4: PS1.PS0 = 0b00 is I2C, 0b10 is UART.
    // Table 4-5: in UART mode COM2 (the SCL pad) becomes Tx and COM3 (the ADR
    // pad) becomes Rx, so the part has no I2C address at all. The PS pins are
    // read at reset, and the datasheet forbids leaving them floating.
    {"BNO055", "PS1", ModePinProtocol, ModeAltUart, false, true},

    // Bosch BST-BMP280-DS001, BST-BME280-DS002 and BME680 all carry the same
    // sentence: "If CSB is connected to VDDIO, the I2C interface is active. If
    // CSB is pulled down, the SPI interface is activated. After CSB has been
    // pulled down once (regardless of whether any clock cycle occurred), the
    // I2C interface is disabled until the next power-on-reset." One glitch on
    // that pad is therefore permanent until the rail is cycled.
    {"BMP280", "CSB", ModePinProtocol, ModeAltSpi, true, true},
    {"BME280", "CSB", ModePinProtocol, ModeAltSpi, true, true},
    {"BME680", "CSB", ModePinProtocol, ModeAltSpi, true, true},

    // ST LIS3DH: "When using the I2C, CS must be tied high." Sampled live
    // rather than latched, so raising the pad is enough on its own.
    {"LIS3DH/2DH12", "CS", ModePinProtocol, ModeAltSpi, true, false},

    // ST LSM6DS3 pin table, pin 12 CS: "I2C/SPI mode selection (1: SPI idle
    // mode / I2C communication enabled; 0: SPI communication mode / I2C
    // disabled)".
    {"LSM6DS3", "CS", ModePinProtocol, ModeAltSpi, true, false},

    // ADI ADXL345: "I2C mode is enabled if the CS pin is tied high to VDD I/O.
    // The CS pin should always be tied high to VDD I/O."
    {"ADXL345/343", "CS", ModePinProtocol, ModeAltSpi, true, false},
};

#define CHIP_MODE_PIN_COUNT (sizeof(chip_mode_pins) / sizeof(chip_mode_pins[0]))

const ChipModePin* chip_mode_pin_for(const char* chip_name) {
    if(!chip_name) return NULL;
    for(size_t i = 0; i < CHIP_MODE_PIN_COUNT; i++) {
        if(strcmp(chip_mode_pins[i].chip, chip_name) == 0) return &chip_mode_pins[i];
    }
    return NULL;
}

size_t chip_mode_pin_count(void) {
    return CHIP_MODE_PIN_COUNT;
}

const ChipModePin* chip_mode_pin_get(size_t index) {
    if(index >= CHIP_MODE_PIN_COUNT) return NULL;
    return &chip_mode_pins[index];
}

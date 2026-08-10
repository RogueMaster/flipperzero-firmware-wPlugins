#include "chip_db.h"
#include "i2c_worker.h"

#include <furi.h>

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

#define M8 0x00FF // convenience: full 8-bit mask
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
static const IdCheck bme680_checks[] = {{0xD0, 0x61, M8, false, false}, {0xF0, 0x00, M8, false, false}};
static const IdCheck bme688_checks[] = {{0xD0, 0x61, M8, false, false}, {0xF0, 0x01, M8, false, false}};
static const IdCheck dps310_checks[] = {{0x0D, 0x10, M8, false, false}};
static const IdCheck ccs811_checks[] = {{0x20, 0x81, M8, false, false}};
static const IdCheck ens160_checks[] = {{0x00, 0x60, M8, false, false}, {0x01, 0x01, M8, false, false}};
// HDC1080: 16-bit registers, manufacturer ID reads ASCII "TI".
static const IdCheck hdc1080_checks[] = {{0xFE, 0x5449, M16, true, false}, {0xFF, 0x1050, M16, true, false}};

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

static const IdCheck vl53l0x_checks[] = {{0xC0, 0xEE, M8, false, false}}; // IDENTIFICATION_MODEL_ID
// TCS34725 and TSL2591 need the command bit set in the register byte.
static const IdCheck tcs34725_checks[] = {{0x92, 0x44, M8, false, false}};
static const IdCheck tsl2591_checks[] = {{0xB2, 0x50, M8, false, false}};
static const IdCheck apds9960_checks[] = {{0x92, 0xAB, M8, false, false}};
static const IdCheck ltr390_checks[] = {{0x06, 0xB0, 0x00F0, false, false}}; // low nibble = revision
static const IdCheck max30102_checks[] = {{0xFF, 0x15, M8, false, false}};

/* --- power / temperature --- */
static const IdCheck ina226_checks[] = {{0xFE, 0x5449, M16, true, false}, {0xFF, 0x2260, M16, true, false}};
static const IdCheck ina260_checks[] = {{0xFE, 0x5449, M16, true, false}, {0xFF, 0x2270, M16, true, false}};
static const IdCheck ina228_checks[] = {{0x3E, 0x5449, M16, true, false}};
static const IdCheck tmp117_checks[] = {{0x0F, 0x0117, 0x0FFF, true, false}}; // top nibble = revision
static const IdCheck lps22hb_checks[] = {{0x0F, 0xB1, M8, false, false}};
static const IdCheck lps25hb_checks[] = {{0x0F, 0xBD, M8, false, false}};

/* --- touch --- */
static const IdCheck cst816s_checks[] = {{0xA7, 0xB4, M8, false, false}};

static const ChipEntry chip_db[] = {
    /* name, addrs, range_lo, range_hi, checks, count, note */
    {"BNO055", {0x28, 0x29, 0xFF}, 0, 0, bno055_checks, 4, NULL},
    {"BMP280", {0x76, 0x77, 0xFF}, 0, 0, bmp280_checks, 1, NULL},
    {"BME280", {0x76, 0x77, 0xFF}, 0, 0, bme280_checks, 1, NULL},
    {"BMP180", {0x77, 0xFF}, 0, 0, bmp180_checks, 1, NULL},
    {"BMP388", {0x76, 0x77, 0xFF}, 0, 0, bmp388_checks, 1, NULL},
    {"BMP390", {0x76, 0x77, 0xFF}, 0, 0, bmp390_checks, 1, NULL},
    {"BME680", {0x76, 0x77, 0xFF}, 0, 0, bme680_checks, 2, NULL},
    {"BME688", {0x76, 0x77, 0xFF}, 0, 0, bme688_checks, 2, NULL},
    {"DPS310", {0x76, 0x77, 0xFF}, 0, 0, dps310_checks, 1, NULL},
    {"CCS811", {0x5A, 0x5B, 0xFF}, 0, 0, ccs811_checks, 1, "EOL part, clones common"},
    {"ENS160", {0x52, 0x53, 0xFF}, 0, 0, ens160_checks, 2, NULL},
    {"HDC1080", {0x40, 0xFF}, 0, 0, hdc1080_checks, 2, NULL},

    {"MPU6050", {0x68, 0x69, 0xFF}, 0, 0, mpu6050_checks, 1, "TDK EOL, old stock"},
    {"MPU6500", {0x68, 0x69, 0xFF}, 0, 0, mpu6500_checks, 1, "often sold as MPU9250"},
    {"MPU9250", {0x68, 0x69, 0xFF}, 0, 0, mpu9250_checks, 1, "TDK EOL, often faked"},
    {"MPU6886", {0x68, 0x69, 0xFF}, 0, 0, mpu6886_checks, 1, NULL},
    {"ICM20948", {0x68, 0x69, 0xFF}, 0, 0, icm20948_checks, 1, NULL},
    {"ICM42605", {0x68, 0x69, 0xFF}, 0, 0, icm42605_checks, 1, NULL},
    {"ICM42688P", {0x68, 0x69, 0xFF}, 0, 0, icm42688_checks, 1, NULL},
    {"BMI160", {0x68, 0x69, 0xFF}, 0, 0, bmi160_checks, 1, NULL},
    {"BMI270", {0x68, 0x69, 0xFF}, 0, 0, bmi270_checks, 1, NULL},
    {"BMI088 gyro", {0x68, 0x69, 0xFF}, 0, 0, bmi088g_checks, 1, NULL},
    {"BMI088 accel", {0x18, 0x19, 0xFF}, 0, 0, bmi088a_checks, 1, NULL},
    {"LSM6DS3", {0x6A, 0x6B, 0xFF}, 0, 0, lsm6ds3_checks, 1, NULL},
    {"LSM6DS3TR-C", {0x6A, 0x6B, 0xFF}, 0, 0, lsm6ds3trc_checks, 1, NULL},
    {"LSM6DSO/OX", {0x6A, 0x6B, 0xFF}, 0, 0, lsm6dso_checks, 1, "DSO and DSOX share the ID"},
    {"LSM6DSV16X", {0x6A, 0x6B, 0xFF}, 0, 0, lsm6dsv_checks, 1, NULL},
    {"QMI8658", {0x6A, 0x6B, 0xFF}, 0, 0, qmi8658_checks, 1, NULL},
    {"LIS3DH/2DH12", {0x18, 0x19, 0xFF}, 0, 0, lis3dh_checks, 1, "same ID as LIS2DH12"},
    {"ADXL345/343", {0x53, 0x1D, 0xFF}, 0, 0, adxl345_checks, 1, NULL},
    {"ADXL355", {0x1D, 0x53, 0xFF}, 0, 0, adxl355_checks, 3, NULL},

    {"LIS3MDL", {0x1C, 0x1E, 0xFF}, 0, 0, lis3mdl_checks, 1, NULL},
    {"LIS2MDL", {0x1E, 0xFF}, 0, 0, lis2mdl_checks, 1, NULL},
    {"MMC5603", {0x30, 0xFF}, 0, 0, mmc5603_checks, 1, NULL},
    {"HMC5883L", {0x1E, 0xFF}, 0, 0, hmc5883l_checks, 3, "EOL since 2016, mostly fake"},
    {"QMC5883L", {0x0D, 0xFF}, 0, 0, qmc5883l_checks, 1, NULL},

    {"VL53L0X", {0x29, 0xFF}, 0, 0, vl53l0x_checks, 1, NULL},
    {"VL53L1X", {0x29, 0xFF}, 0, 0, vl53l1x_checks, 2, NULL},
    {"VL6180X", {0x29, 0xFF}, 0, 0, vl6180x_checks, 1, NULL},
    {"TCS34725", {0x29, 0xFF}, 0, 0, tcs34725_checks, 1, NULL},
    {"TSL2591", {0x29, 0xFF}, 0, 0, tsl2591_checks, 1, NULL},
    {"APDS9960", {0x39, 0xFF}, 0, 0, apds9960_checks, 1, NULL},
    {"LTR-390UV", {0x53, 0xFF}, 0, 0, ltr390_checks, 1, NULL},
    {"MAX30102", {0x57, 0xFF}, 0, 0, max30102_checks, 1, "0x11 here = MAX30100 relabel"},

    {"INA226", {0xFF}, 0x40, 0x4F, ina226_checks, 2, NULL},
    {"INA260", {0xFF}, 0x40, 0x4F, ina260_checks, 2, NULL},
    {"INA228", {0xFF}, 0x40, 0x4F, ina228_checks, 1, NULL},
    {"TMP117", {0xFF}, 0x48, 0x4B, tmp117_checks, 1, NULL},
    {"LPS22HB", {0x5C, 0x5D, 0xFF}, 0, 0, lps22hb_checks, 1, NULL},
    {"LPS25HB", {0x5C, 0x5D, 0xFF}, 0, 0, lps25hb_checks, 1, NULL},
    {"CST816S", {0x15, 0xFF}, 0, 0, cst816s_checks, 1, "sleeps until touched"},

    /* Chips with no readable ID register: presence is all we can prove. */
    {"DS3231 RTC", {0x68, 0xFF}, 0, 0, NULL, 0, NULL},
    {"DS1307 RTC", {0x68, 0xFF}, 0, 0, NULL, 0, NULL},
    {"PCF8563 RTC", {0x51, 0xFF}, 0, 0, NULL, 0, NULL},
    {"SSD1306/SH1106", {0x3C, 0x3D, 0xFF}, 0, 0, NULL, 0, "SH1106 fakes undetectable"},
    {"AHT10/AHT20", {0x38, 0xFF}, 0, 0, NULL, 0, NULL},
    {"BH1750", {0x23, 0x5C, 0xFF}, 0, 0, NULL, 0, NULL},
    {"SHT3x/SHT4x", {0x44, 0x45, 0xFF}, 0, 0, NULL, 0, "grade relabels undetectable"},
    {"SCD4x CO2", {0x62, 0xFF}, 0, 0, NULL, 0, NULL},
    {"SGP30", {0x58, 0xFF}, 0, 0, NULL, 0, NULL},
    {"SGP40/41", {0x59, 0xFF}, 0, 0, NULL, 0, NULL},
    {"SCD30 CO2", {0x61, 0xFF}, 0, 0, NULL, 0, NULL},
    {"Si7021/HTU21D", {0x40, 0xFF}, 0, 0, NULL, 0, NULL},
    {"MLX90614", {0x5A, 0xFF}, 0, 0, NULL, 0, NULL},
    {"MLX90640", {0x33, 0xFF}, 0, 0, NULL, 0, NULL},
    {"AS5600", {0x36, 0xFF}, 0, 0, NULL, 0, NULL},
    {"MAX17048", {0x36, 0xFF}, 0, 0, NULL, 0, NULL},
    {"ADS111x/101x", {0xFF}, 0x48, 0x4B, NULL, 0, NULL},
    {"INA219", {0xFF}, 0x40, 0x4F, NULL, 0, NULL},
    {"MCP23017/08", {0xFF}, 0x20, 0x27, NULL, 0, NULL},
    {"PCF8574", {0xFF}, 0x20, 0x27, NULL, 0, NULL},
    {"PCF8574A", {0xFF}, 0x38, 0x3F, NULL, 0, NULL},
    {"MCP4725 DAC", {0xFF}, 0x60, 0x67, NULL, 0, NULL},
    {"PCA9685 PWM", {0x40, 0xFF}, 0, 0, NULL, 0, NULL},
    {"TCA9548A mux", {0xFF}, 0x70, 0x77, NULL, 0, NULL},
    {"AT24Cxx EEPROM", {0xFF}, 0x50, 0x57, NULL, 0, NULL},
    {"MS5611", {0x76, 0x77, 0xFF}, 0, 0, NULL, 0, NULL},
    {"VEML6070 UV", {0x38, 0x39, 0xFF}, 0, 0, NULL, 0, NULL},
    {"MAX44009", {0x4A, 0x4B, 0xFF}, 0, 0, NULL, 0, NULL},
    {"BNO085", {0x4A, 0x4B, 0xFF}, 0, 0, NULL, 0, "SHTP protocol, no WHO_AM_I"},
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

void chip_verdict_explain(ChipVerdict verdict, const char** line1, const char** line2) {
    switch(verdict) {
    case VerdictGenuine:
        *line1 = "Its ID register matches.";
        *line2 = "The chip is what it says.";
        break;
    case VerdictWrongChip:
        *line1 = "Some of its IDs are wrong.";
        *line2 = "Not the labelled part.";
        break;
    case VerdictNoMatch:
        *line1 = "No known ID matched here.";
        *line2 = "A chip we do not know yet.";
        break;
    case VerdictDetectedNoId:
        *line1 = "This chip has no ID reg.";
        *line2 = "Only presence is proven.";
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


/**
 * @file bme280.c
 * @brief Реализация драйвера BME280 для Flipper Zero.
 *
 * Особенности:
 *  - Все I2C операции оборачиваются в furi_hal_i2c_acquire/release,
 *    чтобы избежать гонки данных при работе нескольких потоков.
 *  - Таймаут ожидания готовности datчика ограничен 100 мс,
 *    что исключает бесконечное зависание при отключённом датчике.
 *  - Компенсационные формулы соответствуют datasheet BST-BME280-DS002,
 *    раздел 4.2.3 (целочисленная 64-битная версия для давления).
 */

#include "bme280.h"
#include <furi.h>
#include <furi_hal_i2c.h>
#include <string.h>

#define TAG "BME280"

/* Таймаут одной I2C транзакции в миллисекундах.
 * 5 мс достаточно для BME280 на любой разумной частоте I2C. */
#define BME280_I2C_TIMEOUT_MS 5u

/* Максимальное время ожидания флага "идёт измерение" в миллисекундах. */
#define BME280_MEAS_TIMEOUT_MS 100u

/* ── Низкоуровневые I2C операции ──────────────────────────────────────── */

/**
 * Читает @size байт из регистра @reg датчика @dev.
 * Захватывает шину I2C перед операцией и освобождает после.
 */
static bool bme280_read_reg(BME280* dev, uint8_t reg, uint8_t* data, uint8_t size) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_trx(
        &furi_hal_i2c_handle_external,
        dev->i2c_addr,
        &reg,  1,      /* TX: адрес регистра */
        data,  size,   /* RX: читаемые данные */
        BME280_I2C_TIMEOUT_MS);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return ok;
}

/**
 * Записывает один байт @value в регистр @reg датчика @dev.
 * Захватывает шину I2C перед операцией и освобождает после.
 */
static bool bme280_write_reg(BME280* dev, uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_tx(
        &furi_hal_i2c_handle_external,
        dev->i2c_addr,
        buf, 2,
        BME280_I2C_TIMEOUT_MS);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return ok;
}

/* ── Чтение калибровочных коэффициентов из NVM датчика ───────────────── */

static bool bme280_read_calibration(BME280* dev) {
    uint8_t buf[24];

    /* --- Температура и давление: регистры 0x88..0x9F (24 байта) --- */
    if(!bme280_read_reg(dev, 0x88, buf, 24)) return false;

    dev->calib.dig_T1 = (uint16_t)((uint16_t)buf[1]  << 8 | buf[0]);
    dev->calib.dig_T2 = (int16_t) ((uint16_t)buf[3]  << 8 | buf[2]);
    dev->calib.dig_T3 = (int16_t) ((uint16_t)buf[5]  << 8 | buf[4]);

    dev->calib.dig_P1 = (uint16_t)((uint16_t)buf[7]  << 8 | buf[6]);
    dev->calib.dig_P2 = (int16_t) ((uint16_t)buf[9]  << 8 | buf[8]);
    dev->calib.dig_P3 = (int16_t) ((uint16_t)buf[11] << 8 | buf[10]);
    dev->calib.dig_P4 = (int16_t) ((uint16_t)buf[13] << 8 | buf[12]);
    dev->calib.dig_P5 = (int16_t) ((uint16_t)buf[15] << 8 | buf[14]);
    dev->calib.dig_P6 = (int16_t) ((uint16_t)buf[17] << 8 | buf[16]);
    dev->calib.dig_P7 = (int16_t) ((uint16_t)buf[19] << 8 | buf[18]);
    dev->calib.dig_P8 = (int16_t) ((uint16_t)buf[21] << 8 | buf[20]);
    dev->calib.dig_P9 = (int16_t) ((uint16_t)buf[23] << 8 | buf[22]);

    /* --- dig_H1: отдельный регистр 0xA1 --- */
    if(!bme280_read_reg(dev, 0xA1, buf, 1)) return false;
    dev->calib.dig_H1 = buf[0];

    /* --- Влажность: регистры 0xE1..0xE7 (7 байт) ---
     * Упаковка H4 и H5 нестандартная — строго по datasheet таблица 16:
     *   dig_H4 = 0xE4[7:0] << 4 | 0xE5[3:0]
     *   dig_H5 = 0xE6[7:0] << 4 | 0xE5[7:4]          */
    if(!bme280_read_reg(dev, 0xE1, buf, 7)) return false;

    dev->calib.dig_H2 = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    dev->calib.dig_H3 = buf[2];
    dev->calib.dig_H4 = (int16_t)((int16_t)buf[3] << 4 | (buf[4] & 0x0F));
    dev->calib.dig_H5 = (int16_t)((int16_t)buf[5] << 4 | (buf[4] >> 4));
    dev->calib.dig_H6 = (int8_t)buf[6];

    return true;
}

/* ── Публичные функции инициализации и конфигурации ──────────────────── */

bool bme280_init(BME280* dev, uint8_t addr, const FuriHalI2cBusHandle* i2c_handle) {
    UNUSED(i2c_handle); /* Хендл фиксирован: furi_hal_i2c_handle_external */

    dev->i2c_addr = addr;

    /* Программный сброс — гарантирует чистое начальное состояние.
     * Выполняется ДО чтения ID и калибровки.
     * После сброса датчик поднимается за ≤2 мс (берём 10 мс с запасом). */
    bme280_write_reg(dev, BME280_REG_RESET, 0xB6);
    furi_delay_ms(10);

    /* Проверка ID — убеждаемся, что на шине именно BME/BMP 280 */
    uint8_t id = 0;
    if(!bme280_read_reg(dev, BME280_REG_ID, &id, 1)) {
        FURI_LOG_E(TAG, "I2C read failed (addr=0x%02X)", addr);
        return false;
    }
    if(id != 0x60 && id != 0x58) {
        /* 0x60 = BME280 (с влажностью), 0x58 = BMP280 (без влажности) */
        FURI_LOG_E(TAG, "Unknown chip ID: 0x%02X (expected 0x60 or 0x58)", id);
        return false;
    }
    FURI_LOG_I(TAG, "Chip ID OK: 0x%02X at I2C addr 0x%02X", id, addr);

    /* Чтение калибровочных коэффициентов из NVM датчика */
    if(!bme280_read_calibration(dev)) {
        FURI_LOG_E(TAG, "Failed to read calibration data");
        return false;
    }

    return true;
}

bool bme280_set_mode(BME280* dev, uint8_t mode) {
    /* Биты [1:0] регистра CTRL_MEAS = режим работы */
    uint8_t val;
    if(!bme280_read_reg(dev, BME280_REG_CTRL_MEAS, &val, 1)) return false;
    val = (uint8_t)((val & 0xFC) | (mode & 0x03));
    return bme280_write_reg(dev, BME280_REG_CTRL_MEAS, val);
}

bool bme280_set_filter(BME280* dev, uint8_t filter) {
    /* Биты [4:2] регистра CONFIG = коэффициент IIR фильтра */
    uint8_t val;
    if(!bme280_read_reg(dev, BME280_REG_CONFIG, &val, 1)) return false;
    val = (uint8_t)((val & 0xE3) | ((filter & 0x07) << 2));
    return bme280_write_reg(dev, BME280_REG_CONFIG, val);
}

bool bme280_set_standby(BME280* dev, uint8_t standby) {
    /* Биты [7:5] регистра CONFIG = время standby */
    uint8_t val;
    if(!bme280_read_reg(dev, BME280_REG_CONFIG, &val, 1)) return false;
    val = (uint8_t)((val & 0x1F) | ((standby & 0x07) << 5));
    return bme280_write_reg(dev, BME280_REG_CONFIG, val);
}

bool bme280_set_temp_oversample(BME280* dev, uint8_t os) {
    /* Биты [7:5] регистра CTRL_MEAS = оверсэмплинг температуры */
    uint8_t val;
    if(!bme280_read_reg(dev, BME280_REG_CTRL_MEAS, &val, 1)) return false;
    val = (uint8_t)((val & 0x1F) | ((os & 0x07) << 5));
    return bme280_write_reg(dev, BME280_REG_CTRL_MEAS, val);
}

bool bme280_set_press_oversample(BME280* dev, uint8_t os) {
    /* Биты [4:2] регистра CTRL_MEAS = оверсэмплинг давления */
    uint8_t val;
    if(!bme280_read_reg(dev, BME280_REG_CTRL_MEAS, &val, 1)) return false;
    val = (uint8_t)((val & 0xE3) | ((os & 0x07) << 2));
    return bme280_write_reg(dev, BME280_REG_CTRL_MEAS, val);
}

bool bme280_set_humid_oversample(BME280* dev, uint8_t os) {
    /* Биты [2:0] регистра CTRL_HUM = оверсэмплинг влажности.
     * По datasheet: изменение CTRL_HUM вступает в силу только после
     * следующей записи в CTRL_MEAS — поэтому перезаписываем его. */
    if(!bme280_write_reg(dev, BME280_REG_CTRL_HUM, os & 0x07)) return false;
    uint8_t ctrl_meas;
    if(!bme280_read_reg(dev, BME280_REG_CTRL_MEAS, &ctrl_meas, 1)) return false;
    return bme280_write_reg(dev, BME280_REG_CTRL_MEAS, ctrl_meas);
}

/* ── Компенсационные функции (datasheet §4.2.3) ───────────────────────── */

/**
 * Компенсация температуры.
 * Входные данные: сырое 20-битное значение из ADC.
 * Результат: температура в единицах 0.01 °C (например, 2510 = 25.10 °C).
 * Побочный эффект: обновляет dev->t_fine — нужен для расчёта P и H.
 */
static int32_t bme280_compensate_temp(BME280* dev, int32_t adc_T) {
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)dev->calib.dig_T1 << 1))) *
                    ((int32_t)dev->calib.dig_T2)) >> 11;

    int32_t var2 = (((((adc_T >> 4) - (int32_t)dev->calib.dig_T1) *
                      ((adc_T >> 4) - (int32_t)dev->calib.dig_T1)) >> 12) *
                    (int32_t)dev->calib.dig_T3) >> 14;

    dev->t_fine = var1 + var2;
    return (dev->t_fine * 5 + 128) >> 8;
}

/**
 * Компенсация давления (64-битная целочисленная версия из datasheet).
 * Результат: давление в единицах Па × 256.
 * Делить на 256.0f для получения Паскалей.
 * Важно: вызывать ПОСЛЕ bme280_compensate_temp (нужен актуальный t_fine).
 */
static uint32_t bme280_compensate_press(BME280* dev, int32_t adc_P) {
    int64_t var1 = (int64_t)dev->t_fine - 128000;
    int64_t var2 = var1 * var1 * (int64_t)dev->calib.dig_P6;
    var2 += (var1 * (int64_t)dev->calib.dig_P5) << 17;
    var2 += (int64_t)dev->calib.dig_P4 << 35;
    var1  = ((var1 * var1 * (int64_t)dev->calib.dig_P3) >> 8) +
             ((var1 * (int64_t)dev->calib.dig_P2) << 12);
    var1  = (((int64_t)1 << 47) + var1) * (int64_t)dev->calib.dig_P1 >> 33;

    if(var1 == 0) return 0; /* Защита от деления на ноль */

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)dev->calib.dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)dev->calib.dig_P8 * p) >> 19;
    p    = ((p + var1 + var2) >> 8) + ((int64_t)dev->calib.dig_P7 << 4);

    return (uint32_t)p; /* Единицы: Па × 256 */
}

/**
 * Компенсация влажности.
 * Результат: влажность в единицах % × 1024.
 * Делить на 1024.0f для получения процентов.
 * Важно: вызывать ПОСЛЕ bme280_compensate_temp (нужен актуальный t_fine).
 */
static uint32_t bme280_compensate_hum(BME280* dev, int32_t adc_H) {
    int32_t v = dev->t_fine - (int32_t)76800;

    v = (((((adc_H << 14) -
            ((int32_t)dev->calib.dig_H4 << 20) -
            ((int32_t)dev->calib.dig_H5 * v)) +
           (int32_t)16384) >> 15) *
         (((((((v * (int32_t)dev->calib.dig_H6) >> 10) *
              (((v * (int32_t)dev->calib.dig_H3) >> 11) + (int32_t)32768)) >> 10) +
            (int32_t)2097152) *
           (int32_t)dev->calib.dig_H2 + 8192) >> 14));

    v -= (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)dev->calib.dig_H1) >> 4);

    /* Ограничиваем диапазон 0..100% */
    if(v < 0)         v = 0;
    if(v > 419430400) v = 419430400;

    return (uint32_t)(v >> 12); /* Единицы: % × 1024 */
}

/* ── Чтение данных с датчика ──────────────────────────────────────────── */

bool bme280_read_sensor(BME280* dev, float* temperature, float* pressure, float* humidity) {
    /* Ждём завершения текущего измерения.
     * Бит 3 регистра STATUS = 1 означает "идёт измерение".
     * Таймаут BME280_MEAS_TIMEOUT_MS защищает от зависания при сбое датчика. */
    uint8_t  status  = 0;
    uint32_t t_start = furi_get_tick();

    do {
        if(!bme280_read_reg(dev, BME280_REG_STATUS, &status, 1)) return false;

        if(furi_get_tick() - t_start > BME280_MEAS_TIMEOUT_MS) {
            FURI_LOG_W(TAG, "Measurement timeout (status=0x%02X)", status);
            return false;
        }

        if(status & 0x08) furi_delay_ms(2); /* Датчик занят — ждём */
    } while(status & 0x08);

    /* Читаем все 8 байт данных одной транзакцией начиная с 0xF7:
     *   [0..2]  = press_msb, press_lsb, press_xlsb
     *   [3..5]  = temp_msb,  temp_lsb,  temp_xlsb
     *   [6..7]  = hum_msb,   hum_lsb                */
    uint8_t buf[8];
    if(!bme280_read_reg(dev, BME280_REG_PRESS_MSB, buf, 8)) return false;

    /* Собираем 20-битные сырые значения давления и температуры,
     * 16-битное сырое значение влажности */
    int32_t adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    int32_t adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);
    int32_t adc_H = ((int32_t)buf[6] << 8)  |  buf[7];

    /* Порядок важен: температура обновляет t_fine, который нужен для P и H */
    int32_t  temp_raw  = bme280_compensate_temp(dev, adc_T);
    uint32_t press_raw = bme280_compensate_press(dev, adc_P);
    uint32_t hum_raw   = bme280_compensate_hum(dev, adc_H);

    /* Конвертируем в человекочитаемые единицы */
    if(temperature) {
        *temperature   = (float)temp_raw / 100.0f;   /* 0.01°C → °C */
        dev->temperature = *temperature;
    }
    if(pressure) {
        *pressure   = (float)press_raw / 256.0f;     /* Па×256 → Па */
        dev->pressure = *pressure;
    }
    if(humidity) {
        *humidity   = (float)hum_raw / 1024.0f;      /* %×1024 → %  */
        dev->humidity = *humidity;
    }

    return true;
}

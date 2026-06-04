/**
 * @file bme280.h
 * @brief Драйвер датчика BME280 (давление/температура/влажность) для Flipper Zero.
 *
 * Подключение к внешнему I2C (разъём GPIO Flipper Zero):
 *   VCC → 3.3V  (пин 9)
 *   GND → GND   (пин 11)
 *   SCL → C0    (пин 16)
 *   SDA → C1    (пин 15)
 *
 * I2C адрес задаётся пином SDO датчика:
 *   SDO → GND  → BME280_I2C_ADDRESS_1 (0x76)
 *   SDO → VCC  → BME280_I2C_ADDRESS_2 (0x77)
 *
 * Furi HAL ожидает адрес сдвинутый на 1 бит влево (7-bit << 1).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <furi_hal_i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── I2C адреса (сдвинуты << 1 для Furi HAL) ─────────────────────────── */
#define BME280_I2C_ADDRESS_1 (0x76 << 1)  /* SDO к GND */
#define BME280_I2C_ADDRESS_2 (0x77 << 1)  /* SDO к VCC */

/* ── Регистры датчика (datasheet BST-BME280-DS002) ───────────────────── */
#define BME280_REG_ID        0xD0  /* Chip ID: 0x60=BME280, 0x58=BMP280 */
#define BME280_REG_RESET     0xE0  /* Программный сброс: записать 0xB6  */
#define BME280_REG_CTRL_HUM  0xF2  /* Оверсэмплинг влажности            */
#define BME280_REG_STATUS    0xF3  /* Бит 3: 1 = идёт измерение         */
#define BME280_REG_CTRL_MEAS 0xF4  /* Оверсэмплинг темп./давл. + режим  */
#define BME280_REG_CONFIG    0xF5  /* IIR фильтр, standby, SPI          */
#define BME280_REG_PRESS_MSB 0xF7  /* Начало блока сырых данных         */

/* ── Оверсэмплинг (количество усредняемых измерений) ─────────────────── */
#define BME280_OS_SKIPPED 0x00  /* Измерение пропущено                 */
#define BME280_OS_1X      0x01  /* 1 измерение                         */
#define BME280_OS_2X      0x02  /* 2 измерения                         */
#define BME280_OS_4X      0x03  /* 4 измерения                         */
#define BME280_OS_8X      0x04  /* 8 измерений                         */
#define BME280_OS_16X     0x05  /* 16 измерений — максимальная точность */

/* ── Режимы работы ────────────────────────────────────────────────────── */
#define BME280_MODE_SLEEP  0x00  /* Спящий режим                       */
#define BME280_MODE_FORCED 0x01  /* Одно измерение → sleep             */
#define BME280_MODE_NORMAL 0x03  /* Непрерывные измерения              */

/* ── Коэффициент IIR фильтра (сглаживание резких скачков давления) ───── */
#define BME280_FILTER_OFF  0x00  /* Фильтр выключен     */
#define BME280_FILTER_2    0x01
#define BME280_FILTER_4    0x02
#define BME280_FILTER_8    0x03
#define BME280_FILTER_16   0x04  /* Максимальное сглаживание */

/* ── Время standby между измерениями (режим NORMAL) ──────────────────── */
#define BME280_STANDBY_0_5  0x00  /*   0.5 мс */
#define BME280_STANDBY_62_5 0x01  /*  62.5 мс */
#define BME280_STANDBY_125  0x02  /* 125   мс */
#define BME280_STANDBY_250  0x03  /* 250   мс */
#define BME280_STANDBY_500  0x04  /* 500   мс */
#define BME280_STANDBY_1000 0x05  /* 1000  мс */
#define BME280_STANDBY_10   0x06  /*  10   мс */
#define BME280_STANDBY_20   0x07  /*  20   мс */

/* ── Калибровочные коэффициенты из NVM датчика ───────────────────────── */
typedef struct {
    uint16_t dig_T1;  /* [0x88..0x89] */
    int16_t  dig_T2;  /* [0x8A..0x8B] */
    int16_t  dig_T3;  /* [0x8C..0x8D] */

    uint16_t dig_P1;  /* [0x8E..0x8F] */
    int16_t  dig_P2;  /* [0x90..0x91] */
    int16_t  dig_P3;  /* [0x92..0x93] */
    int16_t  dig_P4;  /* [0x94..0x95] */
    int16_t  dig_P5;  /* [0x96..0x97] */
    int16_t  dig_P6;  /* [0x98..0x99] */
    int16_t  dig_P7;  /* [0x9A..0x9B] */
    int16_t  dig_P8;  /* [0x9C..0x9D] */
    int16_t  dig_P9;  /* [0x9E..0x9F] */

    uint8_t  dig_H1;  /* [0xA1]                       */
    int16_t  dig_H2;  /* [0xE1..0xE2]                 */
    uint8_t  dig_H3;  /* [0xE3]                       */
    int16_t  dig_H4;  /* [0xE4[7:4] | 0xE5[3:0]]     */
    int16_t  dig_H5;  /* [0xE5[7:4] | 0xE6[7:0]]     */
    int8_t   dig_H6;  /* [0xE7]                       */
} BME280Calib;

/* ── Дескриптор устройства ────────────────────────────────────────────── */
typedef struct {
    uint8_t     i2c_addr;   /* I2C адрес (сдвинутый << 1)                   */
    BME280Calib calib;      /* Калибровочные коэффициенты                   */
    int32_t     t_fine;     /* Внутренняя переменная: нужна для расчёта P, H */
    float       temperature; /* °C  — заполняется bme280_read_sensor()       */
    float       pressure;    /* Па  — заполняется bme280_read_sensor()       */
    float       humidity;    /* %   — заполняется bme280_read_sensor()       */
} BME280;

/* ── API ──────────────────────────────────────────────────────────────── */

/**
 * Инициализация: программный сброс → проверка ID → чтение калибровки.
 * @param dev        Указатель на дескриптор (выделить перед вызовом).
 * @param addr       BME280_I2C_ADDRESS_1 или BME280_I2C_ADDRESS_2.
 * @param i2c_handle Игнорируется — хендл фиксирован (furi_hal_i2c_handle_external).
 * @return true при успехе.
 */
bool bme280_init(BME280* dev, uint8_t addr, const FuriHalI2cBusHandle* i2c_handle);

/** Установить режим работы датчика (BME280_MODE_*). */
bool bme280_set_mode(BME280* dev, uint8_t mode);

/** Установить коэффициент IIR фильтра (BME280_FILTER_*). */
bool bme280_set_filter(BME280* dev, uint8_t filter);

/** Установить время standby в режиме NORMAL (BME280_STANDBY_*). */
bool bme280_set_standby(BME280* dev, uint8_t standby);

/** Установить оверсэмплинг температуры (BME280_OS_*). */
bool bme280_set_temp_oversample(BME280* dev, uint8_t os);

/** Установить оверсэмплинг давления (BME280_OS_*). */
bool bme280_set_press_oversample(BME280* dev, uint8_t os);

/**
 * Установить оверсэмплинг влажности (BME280_OS_*).
 * По datasheet: изменение CTRL_HUM применяется только после следующей
 * записи в CTRL_MEAS. Эта функция делает перезапись автоматически.
 */
bool bme280_set_humid_oversample(BME280* dev, uint8_t os);

/**
 * Прочитать данные с датчика (ждёт завершения текущего измерения).
 * @param temperature Результат в °C  (NULL — не читать).
 * @param pressure    Результат в Па  (NULL — не читать).
 * @param humidity    Результат в %   (NULL — не читать).
 * @return true при успехе; false при ошибке I2C или таймауте 100 мс.
 */
bool bme280_read_sensor(BME280* dev, float* temperature, float* pressure, float* humidity);

#ifdef __cplusplus
}
#endif

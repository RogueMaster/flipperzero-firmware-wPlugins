/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk (https://github.com/quen0n)
    Adapted for flipper-air-stats CO2 monitor.
*/
#ifndef UNITEMP_SENSORS
#define UNITEMP_SENSORS

#include <furi.h>
#include <input/input.h>

/* Bit masks to define return types */
#define UT_TEMPERATURE 0b00000001
#define UT_HUMIDITY    0b00000010
#define UT_PRESSURE    0b00000100
#define UT_CO2         0b00001000
#define UT_CALIBRATION 0b10000000

/* Sensor data types */
typedef enum {
    UT_DATA_TYPE_TEMP          = UT_TEMPERATURE,
    UT_DATA_TYPE_TEMP_HUM      = UT_TEMPERATURE | UT_HUMIDITY,
    UT_DATA_TYPE_TEMP_PRESS    = UT_TEMPERATURE | UT_PRESSURE,
    UT_DATA_TYPE_TEMP_HUM_PRESS = UT_TEMPERATURE | UT_HUMIDITY | UT_PRESSURE,
    UT_DATA_TYPE_TEMP_HUM_CO2  = UT_TEMPERATURE | UT_HUMIDITY | UT_CO2,
    UT_DATA_TYPE_CO2           = UT_CO2,
} SensorDataType;

/* Sensor poll statuses */
typedef enum {
    UT_SENSORSTATUS_OK,
    UT_SENSORSTATUS_TIMEOUT,
    UT_SENSORSTATUS_EARLYPOOL,
    UT_SENSORSTATUS_BADCRC,
    UT_SENSORSTATUS_ERROR,
    UT_SENSORSTATUS_POLLING,
    UT_SENSORSTATUS_INACTIVE,
} UnitempStatus;

/* Flipper Zero I/O port descriptor */
typedef struct GPIO {
    const uint8_t num;
    const char* name;
    const GpioPin* pin;
} GPIO;

typedef struct Sensor Sensor;

/* Function pointer typedefs */
typedef bool(SensorAllocator)(Sensor* sensor, char* args);
typedef bool(SensorFree)(Sensor* sensor);
typedef bool(SensorInitializer)(Sensor* sensor);
typedef bool(SensorDeinitializer)(Sensor* sensor);
typedef UnitempStatus(SensorUpdater)(Sensor* sensor);
typedef UnitempStatus(Calibrate)(Sensor*, float);

/* Connection interface descriptor */
typedef struct Interface {
    const char* name;
    SensorAllocator* allocator;
    SensorFree* mem_releaser;
    SensorUpdater* updater;
} Interface;

/* Sensor type descriptor */
typedef struct {
    const char* typename;
    const char* altname;
    SensorDataType datatype;
    const Interface* interface;
    uint16_t pollingInterval;
    SensorAllocator* allocator;
    SensorFree* mem_releaser;
    SensorInitializer* initializer;
    SensorDeinitializer* deinitializer;
    SensorUpdater* updater;
} SensorType;

/* Sensor type with calibration support */
typedef struct {
    SensorType super;
    Calibrate* calibrate;
} SensorTypeWithCalibration;

/* Sensor instance */
typedef struct Sensor {
    char* name;
    float temp;
    float heat_index;
    float hum;
    float pressure;
    float co2;
    const SensorType* type;
    UnitempStatus status;
    uint32_t lastPollingTime;
    int8_t temp_offset;
    int16_t co2_offset;   /* CO2 correction, ppm; step 50 */
    uint8_t co2_avg;      /* PWM averaging window, 1..10; 1 = raw */
    void* instance;
    uint32_t last_valid_tick;  /* tick of last valid CO2 reading */
    bool     needs_reset;      /* eject: clear co2 + reset averaging */
    /* Debug: PWM internals (populated by mhz19c_update) */
    int32_t dbg_th;           /* HIGH duration ms */
    int32_t dbg_tl;           /* LOW duration ms */
    int32_t dbg_ppm_raw;      /* raw ppm before averaging */
    uint8_t dbg_buf_count;    /* averaging buffer fill */
    bool    dbg_disconnected; /* PWM disconnect flag */
} Sensor;

/* Interface constants */
extern const Interface SINGLE_WIRE;
extern const Interface ONE_WIRE;
extern const Interface I2C;
extern const Interface SPI;

/* ===== Sensor management API ===== */
Sensor* unitemp_sensor_alloc(char* name, const SensorType* type, char* args);
void unitemp_sensor_free(Sensor* sensor);
UnitempStatus unitemp_sensor_updateData(Sensor* sensor);
bool unitemp_sensor_isContains(Sensor* sensor);
Sensor* unitemp_sensor_getActive(uint8_t index);

bool unitemp_sensors_load(void);
void unitemp_sensors_reload(void);
bool unitemp_sensors_save(void);
bool unitemp_sensors_init(void);
bool unitemp_sensors_deInit(void);
void unitemp_sensors_free(void);
void unitemp_sensors_updateValues(void);
uint8_t unitemp_sensors_getCount(void);
void unitemp_sensors_add(Sensor* sensor);
const SensorType** unitemp_sensors_getTypes(void);
uint8_t unitemp_sensors_getTypesCount(void);
const SensorType* unitemp_sensors_getTypeFromStr(char* str);
uint8_t unitemp_sensors_getActiveCount(void);

/* ===== GPIO management API ===== */
const GPIO* unitemp_gpio_getFromInt(uint8_t name);
const GPIO* unitemp_gpio_getFromIndex(uint8_t index);
uint8_t unitemp_gpio_toInt(const GPIO* gpio);
void unitemp_gpio_lock(const GPIO* gpio, const Interface* interface);
void unitemp_gpio_unlock(const GPIO* gpio);
uint8_t unitemp_gpio_getAviablePortsCount(const Interface* interface, const GPIO* extraport);
const GPIO* unitemp_gpio_getAviablePort(const Interface* interface, uint8_t index, const GPIO* extraport);

/* ===== Sensor driver headers ===== */
#include "./interfaces/SingleWireSensor.h"
#include "./interfaces/OneWireSensor.h"
#include "./interfaces/SPISensor.h"
#include "./sensors/LM75.h"
#include "./sensors/BMx280.h"
#include "./sensors/BME680.h"
#include "./sensors/AM2320.h"
#include "./sensors/DHT20.h"
#include "./sensors/SHT30.h"
#include "./sensors/BMP180.h"
#include "./sensors/HTU21x.h"
#include "./sensors/HDC1080.h"
#include "./sensors/MAX31855.h"
#include "./sensors/MAX31725.h"
#include "./sensors/MAX6675.h"
#include "./sensors/SCD30.h"
#include "./sensors/SCD40.h"

#endif /* UNITEMP_SENSORS */

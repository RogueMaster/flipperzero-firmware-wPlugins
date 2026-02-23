/*
 * Minimal stub of unitemp Sensors.h for standalone BME280 use.
 * Contains only the types needed by I2CSensor.c and BMx280.c.
 */
#ifndef UNITEMP_SENSORS
#define UNITEMP_SENSORS

#include <furi.h>
#include <furi_hal.h>

/* Data type bitmasks */
#define UT_TEMPERATURE 0b00000001
#define UT_HUMIDITY    0b00000010
#define UT_PRESSURE    0b00000100
#define UT_CO2         0b00001000

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

/* Flipper Zero GPIO port descriptor */
typedef struct GPIO {
    const uint8_t num;
    const char*   name;
    const GpioPin* pin;
} GPIO;

typedef struct Sensor Sensor;

/* Function pointer typedefs */
typedef bool(SensorAllocator)(Sensor* sensor, char* args);
typedef bool(SensorFree)(Sensor* sensor);
typedef bool(SensorInitializer)(Sensor* sensor);
typedef bool(SensorDeinitializer)(Sensor* sensor);
typedef UnitempStatus(SensorUpdater)(Sensor* sensor);

/* Connection interface descriptor */
typedef struct Interface {
    const char*      name;
    SensorAllocator* allocator;
    SensorFree*      mem_releaser;
    SensorUpdater*   updater;
} Interface;

/* Sensor type descriptor */
typedef struct {
    const char*        typename;
    const char*        altname;
    uint8_t            datatype; /* bitmask: UT_TEMPERATURE | UT_HUMIDITY | UT_PRESSURE */
    const Interface*   interface;
    uint16_t           pollingInterval;
    SensorAllocator*   allocator;
    SensorFree*        mem_releaser;
    SensorInitializer* initializer;
    SensorDeinitializer* deinitializer;
    SensorUpdater*     updater;
} SensorType;

/* Sensor instance */
typedef struct Sensor {
    char*            name;
    float            temp;
    float            heat_index;
    float            hum;
    float            pressure;
    float            co2;
    const SensorType* type;
    UnitempStatus    status;
    uint32_t         lastPollingTime;
    int8_t           temp_offset;
    void*            instance;
} Sensor;

/* I2C interface constant — defined in gpio_stub.c */
extern const Interface I2C;

/* GPIO helpers — stubs in gpio_stub.c */
const GPIO* unitemp_gpio_getFromInt(uint8_t name);
void        unitemp_gpio_lock(const GPIO* gpio, const Interface* interface);
void        unitemp_gpio_unlock(const GPIO* gpio);

/* NOTE: unitemp_I2C_sensor_* are declared in interfaces/I2CSensor.h — not here
 * to avoid -Wredundant-decls errors when I2CSensor.h is included after Sensors.h. */

#endif /* UNITEMP_SENSORS */

/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk (https://github.com/quen0n)
    Adapted for flipper-air-stats CO2 monitor.
*/
#include "Sensors.h"
#include "../../co2_app_i.h"
#include <furi_hal_power.h>
#include <string.h>

/* MH-Z19C sensor type — defined in sensors/mhz19c_sensor.c */
#include "../../sensors/mhz19c_sensor.h"

/* Extra GPIO pins not in the standard list */
const GpioPin SWC_10      = {.pin = LL_GPIO_PIN_14, .port = GPIOA};
const GpioPin SIO_12      = {.pin = LL_GPIO_PIN_13, .port = GPIOA};
const GpioPin TX_13       = {.pin = LL_GPIO_PIN_6,  .port = GPIOB};
const GpioPin RX_14       = {.pin = LL_GPIO_PIN_7,  .port = GPIOB};
const GpioPin ibutton_gpio = {.pin = LL_GPIO_PIN_14, .port = GPIOB};

#define GPIO_ITEMS             (sizeof(GPIOList) / sizeof(GPIO))
#define INTERFACES_TYPES_COUNT (int)(sizeof(interfaces) / sizeof(const Interface*))
#define SENSOR_TYPES_COUNT     (int)(sizeof(sensorTypes) / sizeof(const SensorType*))

/* Available I/O ports */
static const GPIO GPIOList[] = {
    {2,  "2 (A7)",   &gpio_ext_pa7},
    {3,  "3 (A6)",   &gpio_ext_pa6},
    {4,  "4 (A4)",   &gpio_ext_pa4},
    {5,  "5 (B3)",   &gpio_ext_pb3},
    {6,  "6 (B2)",   &gpio_ext_pb2},
    {7,  "7 (C3)",   &gpio_ext_pc3},
    {10, " 10(SWC)", &SWC_10},
    {12, "12 (SIO)", &SIO_12},
    {13, "13 (TX)",  &TX_13},
    {14, "14 (RX)",  &RX_14},
    {15, "15 (C1)",  &gpio_ext_pc1},
    {16, "16 (C0)",  &gpio_ext_pc0},
    {17, "17 (1W)",  &ibutton_gpio}};

/* GPIO lock table: NULL = free, non-NULL = locked by interface */
static const Interface* gpio_interfaces_list[GPIO_ITEMS] = {0};

/* ---- Interface constants ---- */
const Interface SINGLE_WIRE = {
    .name         = "Single wire",
    .allocator    = unitemp_singlewire_alloc,
    .mem_releaser = unitemp_singlewire_free,
    .updater      = unitemp_singlewire_update};

const Interface I2C = {
    .name         = "I2C",
    .allocator    = unitemp_I2C_sensor_alloc,
    .mem_releaser = unitemp_I2C_sensor_free,
    .updater      = unitemp_I2C_sensor_update};

const Interface ONE_WIRE = {
    .name         = "One wire",
    .allocator    = unitemp_onewire_sensor_alloc,
    .mem_releaser = unitemp_onewire_sensor_free,
    .updater      = unitemp_onewire_sensor_update};

const Interface SPI = {
    .name         = "SPI",
    .allocator    = unitemp_spi_sensor_alloc,
    .mem_releaser = unitemp_spi_sensor_free,
    .updater      = unitemp_spi_sensor_update};

/* ---- Sensor type registry ---- */
static const SensorType* sensorTypes[] = {
    &DHT11,      &DHT12_SW,   &DHT20,      &DHT21,      &DHT22,
    &Dallas,     &AM2320_SW,  &AM2320_I2C, &HTU21x,     &AHT10,
    &SHT30,      &GXHT30,     &LM75,       &HDC1080,    &BMP180,
    &BMP280,     &BME280,     &BME680,     &MAX31855,   &MAX6675,
    &SCD30,      &SCD40.super,
    &MHZ19C  /* our PWM CO2 sensor */
};

/* ---- Type registry functions ---- */

const SensorType* unitemp_sensors_getTypeFromInt(uint8_t index) {
    if(index >= SENSOR_TYPES_COUNT) return NULL;
    return sensorTypes[index];
}

const SensorType* unitemp_sensors_getTypeFromStr(char* str) {
    if(str == NULL) return NULL;
    for(uint8_t i = 0; i < unitemp_sensors_getTypesCount(); i++) {
        if(!strcmp(str, sensorTypes[i]->typename)) {
            return sensorTypes[i];
        }
    }
    return NULL;
}

uint8_t unitemp_sensors_getTypesCount(void) {
    return SENSOR_TYPES_COUNT;
}

const SensorType** unitemp_sensors_getTypes(void) {
    return sensorTypes;
}

int unitemp_getIntFromType(const SensorType* type) {
    for(int i = 0; i < SENSOR_TYPES_COUNT; i++) {
        if(!strcmp(type->typename, sensorTypes[i]->typename)) {
            return i;
        }
    }
    return 255;
}

/* ---- GPIO helpers ---- */

const GPIO* unitemp_gpio_getFromInt(uint8_t name) {
    for(uint8_t i = 0; i < GPIO_ITEMS; i++) {
        if(GPIOList[i].num == name) {
            return &GPIOList[i];
        }
    }
    return NULL;
}

const GPIO* unitemp_gpio_getFromIndex(uint8_t index) {
    return &GPIOList[index];
}

uint8_t unitemp_gpio_toInt(const GPIO* gpio) {
    if(gpio == NULL) return 255;
    for(uint8_t i = 0; i < GPIO_ITEMS; i++) {
        if(GPIOList[i].pin->pin == gpio->pin->pin && GPIOList[i].pin->port == gpio->pin->port) {
            return GPIOList[i].num;
        }
    }
    return 255;
}

uint8_t unitemp_gpio_to_index(const GpioPin* gpio) {
    if(gpio == NULL) return 255;
    for(uint8_t i = 0; i < GPIO_ITEMS; i++) {
        if(GPIOList[i].pin->pin == gpio->pin && GPIOList[i].pin->port == gpio->port) {
            return i;
        }
    }
    return 255;
}

uint8_t unitemp_gpio_getAviablePortsCount(const Interface* interface, const GPIO* extraport) {
    uint8_t aviable_ports_count = 0;
    for(uint8_t i = 0; i < GPIO_ITEMS; i++) {
        if(interface == &ONE_WIRE) {
            if(((gpio_interfaces_list[i] == NULL || gpio_interfaces_list[i] == &ONE_WIRE) &&
                (i != 12)) ||
               (unitemp_gpio_getFromIndex(i) == extraport)) {
                aviable_ports_count++;
            }
        }
        if(interface == &SINGLE_WIRE || interface == &SPI) {
            if(gpio_interfaces_list[i] == NULL || (unitemp_gpio_getFromIndex(i) == extraport)) {
                aviable_ports_count++;
            }
        }
        if(interface == &I2C) {
            return 0;
        }
    }
    return aviable_ports_count;
}

void unitemp_gpio_lock(const GPIO* gpio, const Interface* interface) {
    uint8_t i = unitemp_gpio_to_index(gpio->pin);
    if(i == 255) return;
    gpio_interfaces_list[i] = interface;
}

void unitemp_gpio_unlock(const GPIO* gpio) {
    uint8_t i = unitemp_gpio_to_index(gpio->pin);
    if(i == 255) return;
    gpio_interfaces_list[i] = NULL;
}

const GPIO*
    unitemp_gpio_getAviablePort(const Interface* interface, uint8_t index, const GPIO* extraport) {
    if(interface == &I2C) {
        if((gpio_interfaces_list[10] == NULL || gpio_interfaces_list[10] == &I2C) &&
           (gpio_interfaces_list[11] == NULL || gpio_interfaces_list[11] == &I2C)) {
            return unitemp_gpio_getFromIndex(0);
        } else {
            return NULL;
        }
    }
    if(interface == &SPI) {
        if(!((gpio_interfaces_list[0] == NULL || gpio_interfaces_list[0] == &SPI) &&
             (gpio_interfaces_list[1] == NULL || gpio_interfaces_list[1] == &SPI) &&
             (gpio_interfaces_list[3] == NULL || gpio_interfaces_list[3] == &SPI))) {
            return NULL;
        }
    }

    uint8_t aviable_index = 0;
    for(uint8_t i = 0; i < GPIO_ITEMS; i++) {
        if(interface == &ONE_WIRE) {
            if(((gpio_interfaces_list[i] == NULL || gpio_interfaces_list[i] == &ONE_WIRE) &&
                (i != 12)) ||
               (unitemp_gpio_getFromIndex(i) == extraport)) {
                if(aviable_index == index) {
                    return unitemp_gpio_getFromIndex(i);
                } else {
                    aviable_index++;
                }
            }
        }
        if(interface == &SINGLE_WIRE || interface == &SPI) {
            if(gpio_interfaces_list[i] == NULL || unitemp_gpio_getFromIndex(i) == extraport) {
                if(aviable_index == index) {
                    return unitemp_gpio_getFromIndex(i);
                } else {
                    aviable_index++;
                }
            }
        }
    }
    return NULL;
}

/* ---- Sensor list management ---- */

void unitemp_sensor_delete(Sensor* sensor) {
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        if(app->sensors[i] == sensor) {
            app->sensors[i]->status = UT_SENSORSTATUS_INACTIVE;
            return;
        }
    }
}

Sensor* unitemp_sensor_getActive(uint8_t index) {
    uint8_t aviable_index = 0;
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        if(app->sensors[i]->status != UT_SENSORSTATUS_INACTIVE) {
            if(aviable_index == index) {
                return app->sensors[i];
            } else {
                aviable_index++;
            }
        }
    }
    return NULL;
}

uint8_t unitemp_sensors_getCount(void) {
    if(app->sensors == NULL) return 0;
    return app->sensors_count;
}

uint8_t unitemp_sensors_getActiveCount(void) {
    if(app->sensors == NULL) return 0;
    uint8_t counter = 0;
    for(uint8_t i = 0; i < unitemp_sensors_getCount(); i++) {
        if(app->sensors[i]->status != UT_SENSORSTATUS_INACTIVE) counter++;
    }
    return counter;
}

void unitemp_sensors_add(Sensor* sensor) {
    app->sensors =
        (Sensor**)realloc(app->sensors, (unitemp_sensors_getCount() + 1) * sizeof(Sensor*));
    app->sensors[unitemp_sensors_getCount()] = sensor;
    app->sensors_count++;
}

bool unitemp_sensor_isContains(Sensor* sensor) {
    for(uint8_t i = 0; i < unitemp_sensors_getCount(); i++) {
        if(app->sensors[i] == sensor) return true;
    }
    return false;
}

Sensor* unitemp_sensor_alloc(char* name, const SensorType* type, char* args) {
    if(name == NULL || type == NULL) return NULL;
    bool status = false;

    Sensor* sensor = malloc(sizeof(Sensor));
    if(sensor == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s allocation error", name);
        return NULL;
    }

    sensor->name = malloc(11);
    if(sensor->name == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s name allocation error", name);
        free(sensor);
        return NULL;
    }
    strcpy(sensor->name, name);
    sensor->type = type;
    sensor->status = UT_SENSORSTATUS_ERROR;
    sensor->lastPollingTime = furi_get_tick() - 10000;
    sensor->temp = -128.0f;
    sensor->hum = -128.0f;
    sensor->pressure = -128.0f;
    sensor->co2 = -1.0f;
    sensor->temp_offset = 0;

    status = sensor->type->interface->allocator(sensor, args);

    if(status) {
        UNITEMP_DEBUG("Sensor %s allocated", name);
        return sensor;
    }
    free(sensor->name);
    free(sensor);
    FURI_LOG_E(APP_NAME, "Sensor %s(%s) allocation error", name, type->typename);
    return NULL;
}

void unitemp_sensor_free(Sensor* sensor) {
    if(sensor == NULL) {
        FURI_LOG_E(APP_NAME, "Null pointer sensor releasing");
        return;
    }
    if(sensor->type == NULL || sensor->type->interface == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor type or interface is null");
        free(sensor->name);
        free(sensor);
        return;
    }
    bool status = sensor->type->interface->mem_releaser(sensor);
    if(status) {
        UNITEMP_DEBUG("Sensor %s memory successfully released", sensor->name);
    } else {
        FURI_LOG_E(APP_NAME, "Sensor %s memory is not released", sensor->name);
    }
    free(sensor->name);
    free(sensor);
}

void unitemp_sensors_free(void) {
    for(uint8_t i = 0; i < unitemp_sensors_getCount(); i++) {
        unitemp_sensor_free(app->sensors[i]);
    }
    free(app->sensors);
    app->sensors = NULL;
    app->sensors_count = 0;
}

bool unitemp_sensors_init(void) {
    bool result = true;
    app->sensors_ready = false;

    for(uint8_t i = 0; i < unitemp_sensors_getCount(); i++) {
        if(!(*app->sensors[i]->type->initializer)(app->sensors[i])) {
            FURI_LOG_E(APP_NAME, "Sensor init error: %s", app->sensors[i]->name);
            result = false;
        }
        FURI_LOG_I(APP_NAME, "Sensor %s initialized", app->sensors[i]->name);
    }

    app->sensors_ready = true;
    return result;
}

bool unitemp_sensors_deInit(void) {
    bool result = true;

    for(uint8_t i = 0; i < unitemp_sensors_getCount(); i++) {
        if(!(*app->sensors[i]->type->deinitializer)(app->sensors[i])) {
            FURI_LOG_E(APP_NAME, "Sensor deinit error: %s", app->sensors[i]->name);
            result = false;
        }
    }
    return result;
}

/* ---- Sensor update ---- */

UnitempStatus unitemp_sensor_updateData(Sensor* sensor) {
    if(sensor == NULL) return UT_SENSORSTATUS_ERROR;

    if(furi_get_tick() - sensor->lastPollingTime < sensor->type->pollingInterval) {
        if(sensor->status == UT_SENSORSTATUS_TIMEOUT) return UT_SENSORSTATUS_TIMEOUT;
        return UT_SENSORSTATUS_EARLYPOOL;
    }

    sensor->lastPollingTime = furi_get_tick();

    sensor->status = sensor->type->interface->updater(sensor);

    if(sensor->status != UT_SENSORSTATUS_OK && sensor->status != UT_SENSORSTATUS_POLLING) {
        UNITEMP_DEBUG("Sensor %s update status %d", sensor->name, sensor->status);
    }

    if(sensor->status == UT_SENSORSTATUS_OK) {
        if(app->settings.humidity_unit == UT_HUMIDITY_DEWPOINT &&
           app->settings.temp_unit == UT_TEMP_CELSIUS) {
            unitemp_rhToDewpointC(sensor);
        }
        if(app->settings.humidity_unit == UT_HUMIDITY_DEWPOINT &&
           app->settings.temp_unit == UT_TEMP_FAHRENHEIT) {
            unitemp_rhToDewpointF(sensor);
        }
        if(app->settings.heat_index &&
           ((sensor->type->datatype & (UT_TEMPERATURE | UT_HUMIDITY)) ==
            (UT_TEMPERATURE | UT_HUMIDITY))) {
            unitemp_calculate_heat_index(sensor);
        }
        if(app->settings.temp_unit == UT_TEMP_FAHRENHEIT) {
            unitemp_celsiusToFahrenheit(sensor);
        }
        sensor->temp += sensor->temp_offset / 10.f;
        if(app->settings.pressure_unit == UT_PRESSURE_MM_HG) {
            unitemp_pascalToMmHg(sensor);
        } else if(app->settings.pressure_unit == UT_PRESSURE_IN_HG) {
            unitemp_pascalToInHg(sensor);
        } else if(app->settings.pressure_unit == UT_PRESSURE_KPA) {
            unitemp_pascalToKPa(sensor);
        } else if(app->settings.pressure_unit == UT_PRESSURE_HPA) {
            unitemp_pascalToHPa(sensor);
        }
    }

    return sensor->status;
}

void unitemp_sensors_updateValues(void) {
    for(uint8_t i = 0; i < unitemp_sensors_getCount(); i++) {
        unitemp_sensor_updateData(unitemp_sensor_getActive(i));
    }
}

/* ---- SD card load/save (kept but not used in this app) ---- */

bool unitemp_sensors_load(void) {
    FURI_LOG_W(APP_NAME, "sensors_load: using hardcoded sensors, skipping SD load");
    return false;
}

bool unitemp_sensors_save(void) {
    return false;
}

void unitemp_sensors_reload(void) {
    unitemp_sensors_deInit();
    unitemp_sensors_free();
    unitemp_sensors_load();
    unitemp_sensors_init();
}

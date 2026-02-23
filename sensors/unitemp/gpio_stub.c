/*
 * GPIO stubs + I2C interface constant for standalone BME280 use.
 *
 * I2CSensor.c calls unitemp_gpio_lock/unlock/getFromInt to track which
 * GPIO pins are occupied by the I2C bus.  In our standalone app we don't
 * need that bookkeeping, so these are no-ops.
 *
 * The `I2C` Interface constant is required by BMx280.c (used in the
 * SensorType descriptor) and by I2CSensor.c (passed to gpio_lock).
 */
#include "Sensors.h"
#include "interfaces/I2CSensor.h"

/* I2C interface constant -------------------------------------------------- */
const Interface I2C = {
    .name        = "I2C",
    .allocator   = unitemp_I2C_sensor_alloc,
    .mem_releaser = unitemp_I2C_sensor_free,
    .updater     = unitemp_I2C_sensor_update,
};

/* GPIO stubs --------------------------------------------------------------- */
const GPIO* unitemp_gpio_getFromInt(uint8_t name) {
    UNUSED(name);
    return NULL;
}

void unitemp_gpio_lock(const GPIO* gpio, const Interface* interface) {
    UNUSED(gpio);
    UNUSED(interface);
}

void unitemp_gpio_unlock(const GPIO* gpio) {
    UNUSED(gpio);
}

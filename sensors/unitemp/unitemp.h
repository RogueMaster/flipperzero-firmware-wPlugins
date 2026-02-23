/*
 * Minimal stub of unitemp.h for standalone BME280 use.
 * Provides APP_NAME and UNITEMP_DEBUG macro required by BMx280.c / I2CSensor.c.
 */
#ifndef UNITEMP
#define UNITEMP

#include <furi.h>
#include "Sensors.h"

/* Application name used in FURI_LOG_* calls inside unitemp sources */
#ifndef APP_NAME
#define APP_NAME "CO2App"
#endif

/* Debug logging — active only when FURI_DEBUG is defined */
#ifdef FURI_DEBUG
#define UNITEMP_DEBUG(msg, ...) FURI_LOG_D(APP_NAME, msg, ##__VA_ARGS__)
#else
#define UNITEMP_DEBUG(msg, ...)
#endif

#endif /* UNITEMP */

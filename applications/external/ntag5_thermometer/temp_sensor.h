#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TEMP_SENSOR_ID_SIZE (24)

/** Types shared by all sensor backends, the UI only sees these */
typedef enum {
    TempSensorStatusWaiting, /**< No tag in the field */
    TempSensorStatusOk, /**< temperature is valid */
    TempSensorStatusNotSensor, /**< A tag answered, but it has no temperature sensor */
    TempSensorStatusNoPower, /**< NTAG 5: energy harvesting is disabled, the sensor gets no power */
    TempSensorStatusNoValue, /**< NTAG 5: the sensor returned its reset value, no conversion */
} TempSensorStatus;

typedef struct {
    TempSensorStatus status;
    float temperature; /**< Skin temperature in degrees Celsius, valid with TempSensorStatusOk */
    char id[TEMP_SENSOR_ID_SIZE]; /**< Tag number for display, empty if unknown */
} TempSensorReading;

/** Called from the backend's worker thread */
typedef void (*TempSensorCallback)(const TempSensorReading* reading, void* context);

#ifdef __cplusplus
}
#endif

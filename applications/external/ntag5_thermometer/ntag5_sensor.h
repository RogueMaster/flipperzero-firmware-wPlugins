#pragma once

#include "temp_sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/** NTAG 5 link patch with a TMP117 behind the tag's I2C master (ISO 15693) */
typedef struct Ntag5Sensor Ntag5Sensor;

Ntag5Sensor* ntag5_sensor_alloc(void);

void ntag5_sensor_free(Ntag5Sensor* instance);

/** Start polling for a sensor tag and measuring about once per second */
void ntag5_sensor_start(Ntag5Sensor* instance, TempSensorCallback callback, void* context);

/** Stop polling, no callbacks after this returns */
void ntag5_sensor_stop(Ntag5Sensor* instance);

#ifdef __cplusplus
}
#endif

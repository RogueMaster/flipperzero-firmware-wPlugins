#pragma once

#include "temp_sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ISO 11784/11785 FDX-B animal chip with temperature in the extended data block (134 kHz) */
typedef struct FdxbSensor FdxbSensor;

FdxbSensor* fdxb_sensor_alloc(void);

void fdxb_sensor_free(FdxbSensor* instance);

/** Start reading the chip continuously, about every 0.6 seconds */
void fdxb_sensor_start(FdxbSensor* instance, TempSensorCallback callback, void* context);

/** Stop reading and release the LF RFID hardware, no callbacks after this returns */
void fdxb_sensor_stop(FdxbSensor* instance);

#ifdef __cplusplus
}
#endif

#pragma once

#include "tpms_protocol.h"

/** How many sensors we remember. A car has four wheels, but sensors from
 * other cars show up on air too — keep headroom so that our own ones are
 * not evicted. */
#define TPMS_STORE_CAPACITY 8

/** A sensor silent for longer than this has stale readings. */
#define TPMS_STORE_STALE_MS 60000UL

/** How long the signal level "peak" is held. It is there to find a wheel:
 * you walk along the car and the peak shows the best level from a moment
 * ago rather than from the whole session. */
#define TPMS_STORE_PEAK_HOLD_MS 10000UL

typedef struct {
    uint8_t protocol; /**< index into tpms_protocols[] */
    uint32_t id;

    int32_t pressure_kpa_x100;
    int16_t temperature_c;
    uint8_t have; /**< TPMS_HAS_*, accumulated over frames */
    uint8_t flags;

    uint8_t raw[TPMS_RAW_MAX];
    uint8_t raw_len;

    int16_t rssi_x10; /**< latest, tenths of a dBm */
    int16_t peak_rssi_x10;
    uint32_t peak_tick;

    uint32_t frames;
    uint32_t first_tick;
    uint32_t last_tick;
} TpmsSensor;

/** Sensor table. Entry order is stable: new sensors are appended, so rows
 * never jump under the user's finger. */
typedef struct {
    TpmsSensor items[TPMS_STORE_CAPACITY];
    uint8_t count;
    uint32_t total_frames;
} TpmsStore;

void tpms_store_reset(TpmsStore* store);

/** Store a frame. Returns the index of the sensor in the table.
 *
 * A sensor is a protocol and an id together: two protocols may well use
 * the same id, and a few carry no id at all. Fields the frame does not
 * bring are left as they were — SmarTire sends pressure and temperature
 * in separate transmissions, and a row should hold both.
 */
uint8_t
    tpms_store_update(TpmsStore* store, const TpmsFrame* frame, int16_t rssi_x10, uint32_t tick);

/** True if the sensor has been silent for longer than TPMS_STORE_STALE_MS. */
bool tpms_sensor_is_stale(const TpmsSensor* sensor, uint32_t now_tick);

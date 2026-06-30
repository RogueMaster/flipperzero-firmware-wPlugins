#pragma once

#include <furi.h>
#include <lfrfid/lfrfid_worker.h>

#define FLIPPER_WEDGE_RFID_UID_MAX_LEN 8

typedef struct FlipperWedgeRfid FlipperWedgeRfid;

typedef struct {
    uint8_t uid[FLIPPER_WEDGE_RFID_UID_MAX_LEN];
    uint8_t uid_len;
    char protocol_name[32];
} FlipperWedgeRfidData;

typedef void (*FlipperWedgeRfidCallback)(FlipperWedgeRfidData* data, void* context);

/** Allocate RFID reader
 *
 * @return FlipperWedgeRfid instance
 */
FlipperWedgeRfid* flipper_wedge_rfid_alloc(void);

/** Free RFID reader
 *
 * @param instance FlipperWedgeRfid instance
 */
void flipper_wedge_rfid_free(FlipperWedgeRfid* instance);

/** Set callback for tag detection
 *
 * @param instance FlipperWedgeRfid instance
 * @param callback Callback function
 * @param context Callback context
 */
void flipper_wedge_rfid_set_callback(
    FlipperWedgeRfid* instance,
    FlipperWedgeRfidCallback callback,
    void* context);

/** Start RFID scanning
 *
 * @param instance FlipperWedgeRfid instance
 */
void flipper_wedge_rfid_start(FlipperWedgeRfid* instance);

/** Stop RFID scanning
 *
 * @param instance FlipperWedgeRfid instance
 */
void flipper_wedge_rfid_stop(FlipperWedgeRfid* instance);

/** Check if RFID is currently scanning
 *
 * @param instance FlipperWedgeRfid instance
 * @return true if scanning
 */
bool flipper_wedge_rfid_is_scanning(FlipperWedgeRfid* instance);

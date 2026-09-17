#ifndef __hunt_hw_h__
#define __hunt_hw_h__
#include <stdint.h>
/* Sense the real RF environment: sweep the ISM bands, read RSSI, and reduce to
 * an activity score (0-100) plus the hottest band index (0..HUNT_BANDS-1).
 * Receive-only (no transmit). Device impl uses the sub-GHz radio; the host test
 * build links a stub instead. */
void hunt_sense(uint8_t *activity_out, uint8_t *band_out);
#endif

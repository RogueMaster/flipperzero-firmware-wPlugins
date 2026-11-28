#pragma once

#include <stdint.h>

/* Compact signed RAW capture stored for each decoded sensor.
 * TPMS frames are normally well below this limit. A longer multi-frame burst
 * is safely truncated while keeping the beginning of the received signal. */
#define TPMS_CAPTURE_MAX_SAMPLES 768U

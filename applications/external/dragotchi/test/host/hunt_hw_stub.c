#include "hunt_hw.h"
static uint8_t g_activity = 50, g_band = 1;
void hunt_sense_set_for_test(uint8_t activity, uint8_t band) { g_activity = activity; g_band = band; }
void hunt_sense(uint8_t *activity_out, uint8_t *band_out) { *activity_out = g_activity; *band_out = g_band; }

#pragma once

#include <stdint.h>

/* A TPMS sensor at rest stays silent: it wakes up either from wheel
 * rotation or from a 125 kHz low-frequency field — the same way factory
 * activation tools do it. The field is emitted by the RFID coil on the
 * back of the Flipper, so the sensor has to be held right against it.
 */

#define TPMS_LF_FREQUENCY_HZ 125000.0f
#define TPMS_LF_DUTY_CYCLE   0.5f

/** Default duration of a single field pulse, ms. */
#define TPMS_LF_PULSE_MS 700

/** How often the pulse repeats in auto-wake mode, ms. */
#define TPMS_LF_PERIOD_MS 5000

void tpms_lf_field_start(void);
void tpms_lf_field_stop(void);

/** Field pulse, blocking for its whole duration. */
void tpms_lf_wake(uint32_t duration_ms);

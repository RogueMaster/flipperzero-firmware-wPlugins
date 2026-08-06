#pragma once

#include <stdint.h>

/* Датчик TPMS в покое молчит: он просыпается либо от вращения колеса, либо
 * от низкочастотного поля 125 кГц — тем же способом, что и штатные
 * приборы активации. Поле излучается катушкой RFID на задней стороне
 * Flipper, датчик надо держать вплотную к ней.
 */

#define TPMS_LF_FREQUENCY_HZ 125000.0f
#define TPMS_LF_DUTY_CYCLE   0.5f

/** Длительность одного импульса поля по умолчанию, мс. */
#define TPMS_LF_PULSE_MS 700

/** Как часто повторять импульс в режиме автопробуждения, мс. */
#define TPMS_LF_PERIOD_MS 5000

void tpms_lf_field_start(void);
void tpms_lf_field_stop(void);

/** Импульс поля с блокировкой на всё время. */
void tpms_lf_wake(uint32_t duration_ms);

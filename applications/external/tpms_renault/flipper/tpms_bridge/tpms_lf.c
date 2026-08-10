#include "tpms_lf.h"

#include <furi.h>
#include <furi_hal.h>

void tpms_lf_field_start(void) {
    /* tim_read_start switches the pins into carrier mode by itself. */
    furi_hal_rfid_tim_read_start(TPMS_LF_FREQUENCY_HZ, TPMS_LF_DUTY_CYCLE);
}

void tpms_lf_field_stop(void) {
    furi_hal_rfid_tim_read_stop();
    furi_hal_rfid_pins_reset();
}

void tpms_lf_wake(uint32_t duration_ms) {
    tpms_lf_field_start();
    furi_delay_ms(duration_ms);
    tpms_lf_field_stop();
}

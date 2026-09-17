#include "clock.h"
#include <furi_hal.h>
#include <datetime/datetime.h>
uint32_t game_now(void) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    return datetime_datetime_to_timestamp(&dt);
}

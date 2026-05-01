#include "include/domain/habit_date.h"
#include "include/ports/hf_clock_port.h"
#include <furi_hal_rtc.h>

uint32_t hf_clock_today_packed(void) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    return hf_date_pack(dt.year, dt.month, dt.day);
}

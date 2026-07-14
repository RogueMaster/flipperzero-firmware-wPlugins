
#ifndef MODULE_DATE_HEADERS
#define MODULE_DATE_HEADERS

#include "flipper.h"
#include "logic_general.h"

void draw_decoded_date(
    Canvas* canvas,
    uint16_t top,
    DecodingDatePhase selection,
    int8_t century,
    int8_t year_10s,
    int8_t year_1s,
    int8_t month_10s,
    int8_t month_1s,
    int8_t day_in_month_10s,
    int8_t day_in_month_1s,
    int8_t day_in_week);

#endif

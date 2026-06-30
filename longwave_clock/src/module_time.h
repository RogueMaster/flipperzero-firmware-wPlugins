
#ifndef MODULE_TIME_HEADERS
#define MODULE_TIME_HEADERS

#include "flipper.h"
#include "logic_general.h"

void draw_decoded_time(
    Canvas* canvas,
    uint16_t top,
    DecodingTimePhase selection,
    int8_t hours_10s,
    int8_t hours_1s,
    int8_t minutes_10s,
    int8_t minutes_1s,
    int8_t seconds,
    Timezone timezone,
    bool for_next_minute);

#endif

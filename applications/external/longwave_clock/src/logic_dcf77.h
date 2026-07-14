#ifndef LOGIC_DCF77_HEADERS
#define LOGIC_DCF77_HEADERS

#include "logic_general.h"

DecodingPhase dcf77_get_decoding_phase(MinuteData* minute_data);
DecodingTimePhase dcf77_get_decoding_time_phase(MinuteData* minute_data);
DecodingDatePhase dcf77_get_decoding_date_phase(MinuteData* minute_data);

Timezone dcf77_decode_timezone(MinuteData* minute_data);
int dcf77_decode_minutes_1s(MinuteData* minute_data);
int dcf77_decode_minutes_10s(MinuteData* minute_data);
int dcf77_get_minutes_checksum(MinuteData* minute_data);
int dcf77_decode_hours_1s(MinuteData* minute_data);
int dcf77_decode_hours_10s(MinuteData* minute_data);
int dcf77_get_hours_checksum(MinuteData* minute_data);
int dcf77_decode_year_1s(MinuteData* minute_data);
int dcf77_decode_year_10s(MinuteData* minute_data);
int dcf77_decode_month_1s(MinuteData* minute_data);
int dcf77_decode_month_10s(MinuteData* minute_data);
int dcf77_decode_day_of_month_1s(MinuteData* minute_data);
int dcf77_decode_day_of_month_10s(MinuteData* minute_data);
int dcf77_decode_day_of_week(MinuteData* minute_data);
int dcf77_get_date_checksum(MinuteData* minute_data);

void dcf77_set_simulated_minute_data(
    MinuteData* simulated_data,
    int seconds,
    int minutes,
    int hours,
    int day_of_week,
    int day_of_month,
    int month,
    int year);

#endif

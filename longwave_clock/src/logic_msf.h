#ifndef LOGIC_MSF_HEADERS
#define LOGIC_MSF_HEADERS

#include "logic_general.h"

DecodingPhase msf_get_decoding_phase(MinuteData* minute_data);
DecodingTimePhase msf_get_decoding_time_phase(MinuteData* minute_data);
DecodingDatePhase msf_get_decoding_date_phase(MinuteData* minute_data);

int msf_decode_year_10s(MinuteData* minute_data);
int msf_decode_year_1s(MinuteData* minute_data);
int msf_decode_month_10s(MinuteData* minute_data);
int msf_decode_month_1s(MinuteData* minute_data);
int msf_decode_day_of_month_10s(MinuteData* minute_data);
int msf_decode_day_of_month_1s(MinuteData* minute_data);
int msf_decode_day_of_week(MinuteData* minute_data);
int msf_decode_hours_10s(MinuteData* minute_data);
int msf_decode_hours_1s(MinuteData* minute_data);
int msf_decode_minutes_10s(MinuteData* minute_data);
int msf_decode_minutes_1s(MinuteData* minute_data);
Timezone msf_decode_timezone(MinuteData* minute_data);
int msf_get_year_checksum(MinuteData* minute_data);
int msf_get_inyear_checksum(MinuteData* minute_data);
int msf_get_dow_checksum(MinuteData* minute_data);
int msf_get_time_checksum(MinuteData* minute_data);

void msf_set_simulated_minute_data(
    MinuteData* simulated_data,
    int seconds,
    int minutes,
    int hours,
    int day_of_week,
    int day_of_month,
    int month,
    int year);

#endif

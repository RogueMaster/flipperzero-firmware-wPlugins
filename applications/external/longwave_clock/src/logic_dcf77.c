#include "logic_dcf77.h"
#include "longwave_clock_app.h"
#include <math.h>
#include <stdlib.h>

#define BIT_START             0
#define BIT_WEATHER           1
#define BIT_CALL              15
#define BIT_TZCHANGE          16
#define BIT_CEST              17
#define BIT_CET               18
#define BIT_LEAP              19
#define BIT_TIME_CONSTANT     20
#define BIT_MINUTES_1s_START  21
#define BIT_MINUTES_10s_START 25
#define BIT_MINUTES_CHECKSUM  28
#define BIT_HOURS_1s_START    29
#define BIT_HOURS_10s_START   33
#define BIT_HOURS_CHECKSUM    35
#define BIT_DOM_1s_START      36
#define BIT_DOM_10s_START     40
#define BIT_DOW_START         42
#define BIT_MONTH_1s_START    45
#define BIT_MONTH_10s_START   49
#define BIT_YEAR_1s_START     50
#define BIT_YEAR_10s_START    54
#define BIT_DATE_CHECKSUM     58

DecodingPhase dcf77_get_decoding_phase(MinuteData* minute_data) {
    if(minute_data->index == -1) {
        return DecodingUnknown;
    } else if(minute_data->index <= BIT_WEATHER) {
        return DecodingTime;
    } else if(minute_data->index <= BIT_CALL) {
        return DecodingWeather;
    } else if(minute_data->index == BIT_TZCHANGE) {
        return DecodingMeta;
    } else if(minute_data->index <= BIT_DOM_1s_START) {
        return DecodingTime;
    } else {
        return DecodingDate;
    }
}

DecodingTimePhase dcf77_get_decoding_time_phase(MinuteData* minute_data) {
    if(minute_data->index <= BIT_WEATHER) {
        return DecodingTimeSeconds;
    } else if(minute_data->index <= BIT_CET + 1) {
        return DecodingTimeTimezone;
    } else if(minute_data->index <= BIT_TIME_CONSTANT) {
        return DecodingNoTime;
    } else if(minute_data->index == BIT_TIME_CONSTANT + 1) {
        return DecodingTimeConstant;
    } else if(minute_data->index <= BIT_MINUTES_10s_START) {
        return DecodingTimeMinutes1s;
    } else if(minute_data->index <= BIT_MINUTES_CHECKSUM) {
        return DecodingTimeMinutes10s;
    } else if(minute_data->index == BIT_MINUTES_CHECKSUM + 1) {
        return DecodingTimeMinutes;
    } else if(minute_data->index <= BIT_HOURS_10s_START) {
        return DecodingTimeHours1s;
    } else if(minute_data->index <= BIT_HOURS_CHECKSUM) {
        return DecodingTimeHours10s;
    } else {
        return DecodingTimeHours;
    }
}

DecodingDatePhase dcf77_get_decoding_date_phase(MinuteData* minute_data) {
    if(minute_data->index <= BIT_DOM_10s_START) {
        return DecodingDateDayOfMonth1s;
    } else if(minute_data->index <= BIT_DOW_START) {
        return DecodingDateDayOfMonth10s;
    } else if(minute_data->index <= BIT_MONTH_1s_START) {
        return DecodingDateDayOfWeek;
    } else if(minute_data->index <= BIT_MONTH_10s_START) {
        return DecodingDateMonth1s;
    } else if(minute_data->index <= BIT_YEAR_1s_START) {
        return DecodingDateMonth10s;
    } else if(minute_data->index <= BIT_YEAR_10s_START) {
        return DecodingDateYear1s;
    } else if(minute_data->index <= BIT_DATE_CHECKSUM) {
        return DecodingDateYear10s;
    } else if(minute_data->index == BIT_DATE_CHECKSUM + 1) {
        return DecodingDateDate;
    } else {
        return DecodingNoDate;
    }
}

Timezone dcf77_decode_timezone(MinuteData* minute_data) {
    int cest = minute_data->buffer[BIT_CEST];
    int cet = minute_data->buffer[BIT_CET];

    if(cet == 1 && cest == 0) {
        return CETTimezone;
    } else if(cet == 0 && cest == 1) {
        return CESTTimezone;
    } else {
        return UnknownTimezone;
    }
}

int dcf77_decode_minutes_1s(MinuteData* minute_data) {
    return get_if_single_digit(
        get_power2_number(minute_data, BIT_MINUTES_1s_START, BIT_MINUTES_10s_START, 1));
}

int dcf77_decode_minutes_10s(MinuteData* minute_data) {
    return get_if_single_digit(
        get_power2_number(minute_data, BIT_MINUTES_10s_START, BIT_MINUTES_CHECKSUM, 1));
}

int dcf77_get_minutes_checksum(MinuteData* minute_data) {
    return get_checksum(minute_data, BIT_MINUTES_1s_START, BIT_MINUTES_CHECKSUM + 1, 1);
}

int dcf77_decode_hours_1s(MinuteData* minute_data) {
    return get_if_single_digit(
        get_power2_number(minute_data, BIT_HOURS_1s_START, BIT_HOURS_10s_START, 1));
}

int dcf77_decode_hours_10s(MinuteData* minute_data) {
    return get_if_single_digit(
        get_power2_number(minute_data, BIT_HOURS_10s_START, BIT_HOURS_CHECKSUM, 1));
}

int dcf77_get_hours_checksum(MinuteData* minute_data) {
    return get_checksum(minute_data, BIT_HOURS_1s_START, BIT_HOURS_CHECKSUM + 1, 1);
}

int dcf77_decode_day_of_month_1s(MinuteData* minute_data) {
    return get_if_single_digit(
        get_power2_number(minute_data, BIT_DOM_1s_START, BIT_DOM_10s_START, 1));
}

int dcf77_decode_day_of_month_10s(MinuteData* minute_data) {
    return get_if_single_digit(
        get_power2_number(minute_data, BIT_DOM_10s_START, BIT_DOW_START, 1));
}

int dcf77_decode_day_of_week(MinuteData* minute_data) {
    return get_power2_number(minute_data, BIT_DOW_START, BIT_MONTH_1s_START, 1);
}

int dcf77_decode_month_1s(MinuteData* minute_data) {
    return get_if_single_digit(
        get_power2_number(minute_data, BIT_MONTH_1s_START, BIT_MONTH_10s_START, 1));
}

int dcf77_decode_month_10s(MinuteData* minute_data) {
    return get_if_single_digit(
        get_power2_number(minute_data, BIT_MONTH_10s_START, BIT_YEAR_1s_START, 1));
}

int dcf77_decode_year_1s(MinuteData* minute_data) {
    return get_if_single_digit(
        get_power2_number(minute_data, BIT_YEAR_1s_START, BIT_YEAR_10s_START, 1));
}

int dcf77_decode_year_10s(MinuteData* minute_data) {
    return get_if_single_digit(
        get_power2_number(minute_data, BIT_YEAR_10s_START, BIT_DATE_CHECKSUM, 1));
}

int dcf77_get_date_checksum(MinuteData* minute_data) {
    return get_checksum(minute_data, BIT_DOM_1s_START, BIT_DATE_CHECKSUM + 1, 1);
}

static void set_simulated_dcf77_encoded_number(
    MinuteData* simulated_data,
    int start_index,
    int count_1s,
    int count_10s,
    int number) {
    int number_1s = number % 10;
    int number_10s = (number - number_1s) / 10;

    int index = start_index;

    for(int i = 0; i < count_1s; i++) {
        simulated_data->buffer[index] = number_1s & 1;
        index++;
        number_1s >>= 1;
    }

    for(int i = 0; i < count_10s; i++) {
        simulated_data->buffer[index] = number_10s & 1;
        index++;
        number_10s >>= 1;
    }
}

void dcf77_set_simulated_minute_data(
    MinuteData* simulated_data,
    int seconds,
    int minutes,
    int hours,
    int day_of_week,
    int day_of_month,
    int month,
    int year) {
    simulated_data->index = seconds;
    simulated_data->buffer[BIT_START] = 0;
    for(int i = BIT_START + 1; i < BIT_CALL; i++) {
        simulated_data->buffer[i] = random() % 2;
    }
    simulated_data->buffer[BIT_CALL] = 0;
    simulated_data->buffer[BIT_TZCHANGE] = 0;

    if(month > 3 && month < 11) {
        simulated_data->buffer[BIT_CEST] = 1;
        simulated_data->buffer[BIT_CET] = 0;
    } else {
        simulated_data->buffer[BIT_CEST] = 0;
        simulated_data->buffer[BIT_CET] = 1;
    }
    simulated_data->buffer[BIT_LEAP] = 0;
    simulated_data->buffer[BIT_TIME_CONSTANT] = 1;

    set_simulated_dcf77_encoded_number(simulated_data, BIT_MINUTES_1s_START, 4, 3, minutes);

    simulated_data->buffer[BIT_MINUTES_CHECKSUM] =
        get_checksum(simulated_data, BIT_MINUTES_1s_START, BIT_MINUTES_CHECKSUM, 1);

    set_simulated_dcf77_encoded_number(simulated_data, BIT_HOURS_1s_START, 4, 2, hours);

    simulated_data->buffer[BIT_HOURS_CHECKSUM] =
        get_checksum(simulated_data, BIT_HOURS_1s_START, BIT_HOURS_CHECKSUM, 1);

    set_simulated_dcf77_encoded_number(simulated_data, BIT_DOM_1s_START, 4, 2, day_of_month);
    set_simulated_dcf77_encoded_number(simulated_data, BIT_DOW_START, 3, 0, day_of_week);
    set_simulated_dcf77_encoded_number(simulated_data, BIT_MONTH_1s_START, 4, 1, month);
    set_simulated_dcf77_encoded_number(simulated_data, BIT_YEAR_1s_START, 4, 4, year % 100);

    simulated_data->buffer[BIT_DATE_CHECKSUM] =
        get_checksum(simulated_data, BIT_DOM_1s_START, BIT_DATE_CHECKSUM, 1);
}

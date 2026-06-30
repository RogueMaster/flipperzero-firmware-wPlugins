#include "logic_msf.h"

#define BIT_START             0
#define BIT_DUT1_PLUS         2
#define BIT_DUT1_MINUS        18
#define BIT_YEAR_10s_START    34
#define BIT_YEAR_1s_START     42
#define BIT_MONTH_10s_START   50
#define BIT_MONTH_1s_START    52
#define BIT_DOM_10s_START     60
#define BIT_DOM_1s_START      64
#define BIT_DOW_START         72
#define BIT_HOURS_10s_START   78
#define BIT_HOURS_1s_START    82
#define BIT_MINUTES_10s_START 90
#define BIT_MINUTES_1s_START  96
#define BIT_MINUTE_MARKER     104
#define BIT_STW               106
#define BIT_YEAR_CHECKSUM     109
#define BIT_INYEAR_CHECKSUM   111
#define BIT_DOW_CHECKSUM      113
#define BIT_TIME_CHECKSUM     115
#define BIT_SUMMERTIME        117
#define BIT_END               118

DecodingPhase msf_get_decoding_phase(MinuteData* minute_data) {
    if(minute_data->index == -1) {
        return DecodingUnknown;
    } else {
        int last_index = minute_data->index - 1;

        if(last_index < BIT_DUT1_PLUS) {
            return DecodingTime;
        } else if(last_index < BIT_YEAR_10s_START) {
            return DecodingDUT;
        } else if(last_index < BIT_HOURS_10s_START) {
            return DecodingDate;
        } else if(last_index < BIT_YEAR_CHECKSUM - 1) {
            return DecodingTime;
        } else if(last_index < BIT_TIME_CHECKSUM - 1) {
            return DecodingDate;
        } else {
            return DecodingTime;
        }
    }
}

DecodingTimePhase msf_get_decoding_time_phase(MinuteData* minute_data) {
    int last_index = minute_data->index - 1;

    if(last_index <= BIT_DUT1_PLUS) {
        return DecodingTimeSeconds;
    } else if(last_index < BIT_HOURS_1s_START) {
        return DecodingTimeHours10s;
    } else if(last_index < BIT_MINUTES_10s_START) {
        return DecodingTimeHours1s;
    } else if(last_index < BIT_MINUTES_1s_START) {
        return DecodingTimeMinutes10s;
    } else if(last_index < BIT_MINUTE_MARKER) {
        return DecodingTimeMinutes1s;
    } else if(last_index < BIT_YEAR_CHECKSUM - 1) {
        return DecodingTimeConstant;
    } else if(last_index < BIT_SUMMERTIME - 1) {
        return DecodingTimeChecksum;
    } else if(last_index < BIT_END) {
        return DecodingTimeTimezone;
    } else {
        return DecodingTimeConstant;
    }
}

DecodingDatePhase msf_get_decoding_date_phase(MinuteData* minute_data) {
    int last_index = minute_data->index - 1;

    if(last_index < BIT_YEAR_1s_START) {
        return DecodingDateYear10s;
    } else if(last_index < BIT_MONTH_10s_START) {
        return DecodingDateYear1s;
    } else if(last_index < BIT_MONTH_1s_START) {
        return DecodingDateMonth10s;
    } else if(last_index < BIT_DOM_10s_START) {
        return DecodingDateMonth1s;
    } else if(last_index < BIT_DOM_1s_START) {
        return DecodingDateDayOfMonth10s;
    } else if(last_index < BIT_DOW_START) {
        return DecodingDateDayOfMonth1s;
    } else if(last_index < BIT_HOURS_10s_START) {
        return DecodingDateDayOfWeek;
    } else if(last_index < BIT_INYEAR_CHECKSUM - 1) {
        return DecodingDateYearChecksum;
    } else if(last_index < BIT_DOW_CHECKSUM - 1) {
        return DecodingDateInYearChecksum;
    } else {
        return DecodingDateDayOfWeekChecksum;
    }
}

static int
    get_if_single_digit_and_0_interlude(MinuteData* minute_data, int start_index, int stop_index) {
    for(int i = start_index - 1; i > stop_index - 1; i -= 2) {
        if(minute_data_get_bit(minute_data, i) != 0) {
            return -1;
        }
    }
    return get_if_single_digit(
        get_power2_number(minute_data, start_index - 2, stop_index - 2, -2));
}

int msf_decode_year_10s(MinuteData* minute_data) {
    return get_if_single_digit_and_0_interlude(minute_data, BIT_YEAR_1s_START, BIT_YEAR_10s_START);
}

int msf_decode_year_1s(MinuteData* minute_data) {
    return get_if_single_digit_and_0_interlude(
        minute_data, BIT_MONTH_10s_START, BIT_YEAR_1s_START);
}

int msf_decode_month_10s(MinuteData* minute_data) {
    return get_if_single_digit_and_0_interlude(
        minute_data, BIT_MONTH_1s_START, BIT_MONTH_10s_START);
}

int msf_decode_month_1s(MinuteData* minute_data) {
    return get_if_single_digit_and_0_interlude(minute_data, BIT_DOM_10s_START, BIT_MONTH_1s_START);
}

int msf_decode_day_of_month_10s(MinuteData* minute_data) {
    return get_if_single_digit_and_0_interlude(minute_data, BIT_DOM_1s_START, BIT_DOM_10s_START);
}

int msf_decode_day_of_month_1s(MinuteData* minute_data) {
    return get_if_single_digit_and_0_interlude(minute_data, BIT_DOW_START, BIT_DOM_1s_START);
}

int msf_decode_day_of_week(MinuteData* minute_data) {
    int result =
        get_if_single_digit_and_0_interlude(minute_data, BIT_HOURS_10s_START, BIT_DOW_START);

    if(result == 0) {
        result = 7;
    }

    return result;
}

int msf_decode_hours_10s(MinuteData* minute_data) {
    return get_if_single_digit_and_0_interlude(
        minute_data, BIT_HOURS_1s_START, BIT_HOURS_10s_START);
}

int msf_decode_hours_1s(MinuteData* minute_data) {
    return get_if_single_digit_and_0_interlude(
        minute_data, BIT_MINUTES_10s_START, BIT_HOURS_1s_START);
}

int msf_decode_minutes_10s(MinuteData* minute_data) {
    return get_if_single_digit_and_0_interlude(
        minute_data, BIT_MINUTES_1s_START, BIT_MINUTES_10s_START);
}

int msf_decode_minutes_1s(MinuteData* minute_data) {
    return get_if_single_digit_and_0_interlude(
        minute_data, BIT_MINUTE_MARKER, BIT_MINUTES_1s_START);
}

Timezone msf_decode_timezone(MinuteData* minute_data) {
    if(minute_data->buffer[BIT_SUMMERTIME - 1] != 1) {
        return UnknownTimezone;
    } else if(minute_data->buffer[BIT_SUMMERTIME] == 1) {
        return CETTimezone;
    } else if(minute_data->buffer[BIT_SUMMERTIME] == 0) {
        return UTCTimezone;
    } else {
        return UnknownTimezone;
    }
}

int msf_get_year_checksum(MinuteData* minute_data) {
    if(minute_data->buffer[BIT_YEAR_CHECKSUM - 1] != 1) {
        return -1;
    }
    return add_to_checksum(
        minute_data,
        BIT_YEAR_CHECKSUM,
        get_checksum(minute_data, BIT_YEAR_10s_START, BIT_MONTH_10s_START, 2));
}

int msf_get_inyear_checksum(MinuteData* minute_data) {
    if(minute_data->buffer[BIT_INYEAR_CHECKSUM - 1] != 1) {
        return -1;
    }
    return add_to_checksum(
        minute_data,
        BIT_INYEAR_CHECKSUM,
        get_checksum(minute_data, BIT_MONTH_10s_START, BIT_DOW_START, 2));
}

int msf_get_dow_checksum(MinuteData* minute_data) {
    if(minute_data->buffer[BIT_DOW_CHECKSUM - 1] != 1) {
        return -1;
    }
    return add_to_checksum(
        minute_data,
        BIT_DOW_CHECKSUM,
        get_checksum(minute_data, BIT_DOW_START, BIT_HOURS_10s_START, 2));
}

int msf_get_time_checksum(MinuteData* minute_data) {
    if(minute_data->buffer[BIT_TIME_CHECKSUM - 1] != 1) {
        return -1;
    }
    return add_to_checksum(
        minute_data,
        BIT_TIME_CHECKSUM,
        get_checksum(minute_data, BIT_HOURS_10s_START, BIT_MINUTE_MARKER, 2));
}

static void set_simulated_msf_encoded_number(
    MinuteData* simulated_data,
    int last_index,
    int count_1s,
    int count_10s,
    int number) {
    int number_1s = number % 10;
    int number_10s = (number - number_1s) / 10;

    int index = last_index;

    for(int i = 0; i < count_1s; i++) {
        simulated_data->buffer[index] = number_1s & 1;
        simulated_data->buffer[index + 1] = 0;
        index -= 2;
        number_1s >>= 1;
    }

    for(int i = 0; i < count_10s; i++) {
        simulated_data->buffer[index] = number_10s & 1;
        simulated_data->buffer[index + 1] = 0;
        index -= 2;
        number_10s >>= 1;
    }
}

void msf_set_simulated_minute_data(
    MinuteData* simulated_data,
    int seconds,
    int minutes,
    int hours,
    int day_of_week,
    int day_of_month,
    int month,
    int year) {
    simulated_data->index = seconds * 2;

    simulated_data->buffer[BIT_START] = 1;
    simulated_data->buffer[BIT_START + 1] = 1;
    for(int i = BIT_DUT1_PLUS; i < BIT_YEAR_10s_START; i++) {
        simulated_data->buffer[i] = 0;
    }
    for(int i = BIT_YEAR_10s_START + 1; i < BIT_YEAR_10s_START; i += 2) {
        simulated_data->buffer[i] = 0;
    }

    set_simulated_msf_encoded_number(simulated_data, BIT_MONTH_10s_START - 2, 4, 4, year);
    set_simulated_msf_encoded_number(simulated_data, BIT_DOM_10s_START - 2, 4, 1, month);
    set_simulated_msf_encoded_number(simulated_data, BIT_DOW_START - 2, 4, 2, day_of_month);
    if(day_of_week == 7) {
        day_of_week = 0;
    }
    set_simulated_msf_encoded_number(simulated_data, BIT_HOURS_10s_START - 2, 3, 0, day_of_week);

    set_simulated_msf_encoded_number(simulated_data, BIT_MINUTES_10s_START - 2, 4, 2, hours);
    set_simulated_msf_encoded_number(simulated_data, BIT_MINUTE_MARKER - 2, 4, 3, minutes);

    simulated_data->buffer[BIT_MINUTE_MARKER] = 0;
    simulated_data->buffer[BIT_MINUTE_MARKER + 1] = 0;
    simulated_data->buffer[BIT_STW] = 0;

    for(int i = BIT_STW; i < BIT_END; i += 2) {
        simulated_data->buffer[i] = 1;
    }

    simulated_data->buffer[BIT_YEAR_CHECKSUM] =
        get_checksum(simulated_data, BIT_YEAR_10s_START, BIT_MONTH_10s_START, 2) == 1 ? 0 : 1;
    simulated_data->buffer[BIT_INYEAR_CHECKSUM] =
        get_checksum(simulated_data, BIT_MONTH_10s_START, BIT_DOW_START, 2) == 1 ? 0 : 1;
    simulated_data->buffer[BIT_DOW_CHECKSUM] =
        get_checksum(simulated_data, BIT_DOW_START, BIT_HOURS_10s_START, 2) == 1 ? 0 : 1;
    simulated_data->buffer[BIT_TIME_CHECKSUM] =
        get_checksum(simulated_data, BIT_HOURS_10s_START, BIT_MINUTE_MARKER, 2) == 1 ? 0 : 1;

    if(month > 3 && month < 11) {
        simulated_data->buffer[BIT_SUMMERTIME] = 1;
    } else {
        simulated_data->buffer[BIT_SUMMERTIME] = 0;
    }

    simulated_data->buffer[BIT_END] = 0;
    simulated_data->buffer[BIT_END + 1] = 0;
}

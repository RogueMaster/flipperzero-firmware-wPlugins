#ifndef LOGIC_HEADERS
#define LOGIC_HEADERS

#include <stdint.h>
#define MINUTE        60
#define BUFFER        32
#define MIN_INTERRUPT 3

typedef enum {
    BitZero,
    BitOne,
    BitChecksum,
    BitChecksumError,
    BitConstant,
    BitConstantError,
    BitEndMinute,
    BitUnknown,
    BitEndSync,
    BitStartEmpty,
    BitStartWeather,
    BitStartDUT,
    BitStartTimezone,
    BitStartMinute,
    BitStartHour,
    BitStartDayOfMonth,
    BitStartDayOfWeek,
    BitStartMonth,
    BitStartYear,
} Bit;

typedef enum {
    DecodingUnknown,
    DecodingIrrelevant,
    DecodingWeather,
    DecodingDUT,
    DecodingTime,
    DecodingDate,
    DecodingMeta
} DecodingPhase;

typedef enum {
    DecodingNoTime,
    DecodingTimeTimezone,
    DecodingTimeMinutes,
    DecodingTimeMinutes10s,
    DecodingTimeMinutes1s,
    DecodingTimeHours,
    DecodingTimeHours10s,
    DecodingTimeHours1s,
    DecodingTimeSeconds,
    DecodingTimeConstant,
    DecodingTimeChecksum
} DecodingTimePhase;

typedef enum {
    DecodingNoDate,
    DecodingDateDayOfMonth1s,
    DecodingDateDayOfMonth10s,
    DecodingDateDayOfMonth,
    DecodingDateDayOfWeek,
    DecodingDateMonth1s,
    DecodingDateMonth10s,
    DecodingDateMonth,
    DecodingDateYear1s,
    DecodingDateYear10s,
    DecodingDateYear,
    DecodingDateCentury,
    DecodingDateDate,
    DecodingDateConstant,
    DecodingDateInYearChecksum,
    DecodingDateYearChecksum,
    DecodingDateDayOfWeekChecksum
} DecodingDatePhase;

typedef enum {
    UnknownTimezone,
    UTCTimezone,
    CETTimezone,
    CESTTimezone
} Timezone;

typedef struct {
    int index;
    int half_seconds;
    int error_count;
    int buffer[MINUTE * 2];
    int max;
} MinuteData;

typedef enum {
    MinuteDataErrorNone,
    MinuteDataErrorUnknownBit,
    MinuteDataErrorDesync
} MinuteDataError;

void minute_data_start_minute(MinuteData* minute_data);
MinuteDataError minute_data_500ms_passed(MinuteData* minute_data);
void minute_data_add_bit(MinuteData* minute_data, int bit);
int minute_data_get_length(MinuteData* minute_data);
int minute_data_get_bit(MinuteData* minute_data, int position);
int get_power2_number(MinuteData* minute_data, int start_index, int stop_index, int increment);
int get_checksum(MinuteData* minute_data, int start_index, int stop_index, int increment);
int add_to_checksum(MinuteData* minute_data, int index, int current_checksum);
void minute_data_reset(MinuteData* minute_data);
int get_if_single_digit(int value);

MinuteData* minute_data_alloc(int max);
void minute_data_free(MinuteData* minute_data);

typedef struct {
    MinuteData* minute_data;
    DecodingPhase decoding;
    DecodingTimePhase decoding_time;
    DecodingDatePhase decoding_date;
    int8_t year_10s;
    int8_t year_1s;
    int8_t month_10s;
    int8_t month_1s;
    int8_t day_of_month_1s;
    int8_t day_of_month_10s;
    int8_t day_of_week;
    int8_t hours_10s;
    int8_t hours_1s;
    int8_t minutes_10s;
    int8_t minutes_1s;
    int8_t seconds;
    Timezone timezone;
    uint8_t received_interrupt;
    uint8_t received_count;
    uint8_t last_received;
    Bit buffer[BUFFER];
} LWCViewModel;

#endif

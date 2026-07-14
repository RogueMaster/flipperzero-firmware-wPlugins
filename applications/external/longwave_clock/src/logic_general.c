#include "logic_general.h"
#include <stdlib.h>

static void minute_data_up_index(MinuteData* minute_data) {
    if(minute_data->index < minute_data->max - 1) {
        minute_data->index++;
    } else {
        minute_data_reset(minute_data);
    }
}

void minute_data_start_minute(MinuteData* minute_data) {
    minute_data->index = 0;
    minute_data->half_seconds = 0;
    minute_data->error_count = 0;
}

MinuteDataError minute_data_500ms_passed(MinuteData* minute_data) {
    if(minute_data->index >= 0 && minute_data->index < 59) {
        minute_data->half_seconds++;
        if(minute_data->half_seconds > minute_data->index * 2 + 1) {
            minute_data->buffer[minute_data->index] = -1;
            minute_data_up_index(minute_data);
            minute_data->error_count++;
            if(minute_data->error_count > 2) {
                minute_data_reset(minute_data);
                return MinuteDataErrorDesync;
            } else {
                return MinuteDataErrorUnknownBit;
            }
        }
    }
    return MinuteDataErrorNone;
}

void minute_data_add_bit(MinuteData* minute_data, int bit) {
    if(minute_data->index >= 0) {
        minute_data->buffer[minute_data->index] = bit;
        if((bit == 0 || bit == 1) && minute_data->error_count > 0) {
            minute_data->error_count--;
        }
        minute_data_up_index(minute_data);
    }
}

int minute_data_get_length(MinuteData* minute_data) {
    return minute_data->index;
}

int minute_data_get_bit(MinuteData* minute_data, int position) {
    return minute_data->buffer[position];
}

int get_if_single_digit(int value) {
    if(value < 10) {
        return value;
    } else {
        return -1;
    }
}

int get_power2_number(MinuteData* minute_data, int start_index, int stop_index, int increment) {
    int power2 = 1;
    int result = 0;
    int index = start_index;

    do {
        int value = minute_data->buffer[index];
        if(value == 0 || value == 1) {
            result += value * power2;
        } else {
            return -1;
        }
        index = index + increment;
        power2 = power2 * 2;
    } while(index != stop_index);

    return result;
}

int get_checksum(MinuteData* minute_data, int start_index, int stop_index, int increment) {
    int result = 0;
    int index = start_index;

    do {
        int value = minute_data->buffer[index];
        if(value == 1) {
            result = 1 - result;
        } else if(value == 0) {
        } else {
            return -1;
        }
        index = index + increment;
    } while(index != stop_index);
    return result;
}

int add_to_checksum(MinuteData* minute_data, int index, int current_checksum) {
    if(current_checksum == -1) {
        return -1;
    } else {
        int value = minute_data->buffer[index];
        if(value == 1) {
            return 1 - current_checksum;
        } else if(value == 0) {
            return current_checksum;
        } else {
            return -1;
        }
    }
}

void minute_data_reset(MinuteData* minute_data) {
    minute_data->index = -1;
}

MinuteData* minute_data_alloc(int max) {
    MinuteData* minute_data = malloc(sizeof(MinuteData));
    minute_data_reset(minute_data);
    minute_data->max = max;
    return minute_data;
}

void minute_data_free(MinuteData* minute_data) {
    free(minute_data);
}

#include "fujitsu_ac_utils.h"
#include "fujitsu_general_remote.h"
#include "ac_fujitsu_general_indoor_ir_remote_icons.h"

uint8_t fuji_AC_temp_to_celsius(uint8_t temp, bool* and_a_half) {
    *and_a_half = (temp % 2 != 0);
    return 18 + (temp / 2);
}

uint8_t fuji_AC_temp_up(uint8_t current_temp) {
    if(current_temp >= FujiAir_C_MAX_TEMP) return FujiAir_C_MAX_TEMP;
    return ++current_temp;
}

uint8_t fuji_AC_temp_down(uint8_t current_temp) {
    if(current_temp <= FujiAir_C_MIN_TEMP) return FujiAir_C_MIN_TEMP;
    return --current_temp;
}

uint8_t fuji_AC_fan_next(uint8_t current_speed) {
    if(current_speed >= FujiAir_C_MAX_FAN_SPEED) return 0;
    return ++current_speed;
}

uint8_t fuji_AC_fan_prev(uint8_t current_speed) {
    if(current_speed == 0) return FujiAir_C_MAX_FAN_SPEED;
    return --current_speed;
}

const char* fuji_AC_mode_str(FujiAirMode mode) {
    switch(mode) {
    case FujiAirMode_Auto:
        return "Auto";
    case FujiAirMode_Cool:
        return "Cool";
    case FujiAirMode_Dry:
        return "Dry";
    case FujiAirMode_Fan:
        return "Fan";
    case FujiAirMode_Heat:
        return "Heat";
    default:
        furi_assert(false);
        return "";
    }
}

const Icon* fuji_AC_fan_to_icon(uint8_t speed) {
    switch(speed) {
    case 0:
        return &I_asset_fuji_fan_power_bars_22x15_auto;
    case 1:
        return &I_asset_fuji_fan_power_bars_22x15_1;
    case 2:
        return &I_asset_fuji_fan_power_bars_22x15_2;
    case 3:
        return &I_asset_fuji_fan_power_bars_22x15_3;
    case 4:
        return &I_asset_fuji_fan_power_bars_22x15_4;
    default:
        furi_assert(false);
        return NULL;
    }
}

FujiAirMode fuji_AC_mode_next(FujiAirMode curr_mode) {
    if(curr_mode == FujiAirMode_Heat) return FujiAirMode_Auto;

    return ++curr_mode;
}

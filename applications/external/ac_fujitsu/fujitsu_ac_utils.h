#pragma once
#include "fujitsu_general_remote.h"
#include <stdbool.h>
#include <stdint.h>

/** @brief Convert FujitsuAirState::temperature to celsius
 *
 * @param [in] temp Temperature value as expected by the air conditioner
 * @param [out] and_a_half If true, add 0.5 to the returned value to get the exact temperature
 *
 * @return uint8_t The temperature in celsius
 */
uint8_t fuji_AC_temp_to_celsius(uint8_t temp, bool* and_a_half);

/**
 * @brief Get the new temperature, increased by one step.
 *   If @p current_temp >= max temp, return max temp
 * @param current_temp Current temperature
 * @return uint8_t Temperature increased by one step in the range allowed by the AC
 */
uint8_t fuji_AC_temp_up(uint8_t current_temp);

/**
 * @brief Get the new temperature, decreased by one step.
 *   If @p current_temp <= min temp, return min temp
 * @param current_temp Current temperature
 * @return uint8_t Temperature decreased by one step in the range allowed by the AC
 */
uint8_t fuji_AC_temp_down(uint8_t current_temp);

/**
 * @brief Cycle to the next speed of the fan
 * 
 * @param current_speed Current speed of the fan
 * @return uint8_t Next speed of the fan
 */
uint8_t fuji_AC_fan_next(uint8_t current_speed);

/**
 * @brief Cycle to the prev speed of the fan
 * 
 * @param current_speed Current speed of the fan
 * @return uint8_t Prev speed of the fan
 */
uint8_t fuji_AC_fan_prev(uint8_t current_speed);

/**
 * @brief Convert AC Mode to str
 * 
 * @param mode 
 * @return const char* 
 */
const char* fuji_AC_mode_str(FujiAirMode mode);

/**
 * @brief Get the correct power icon, depending on the fan speed
 * 
 * @param speed 
 * @return const Icon* 
 */
const Icon* fuji_AC_fan_to_icon(uint8_t speed);

/**
 * @brief Cycle to the next AC mode
 * 
 * @param curr_mode 
 * @return FujiAirMode 
 */
FujiAirMode fuji_AC_mode_next(FujiAirMode curr_mode);

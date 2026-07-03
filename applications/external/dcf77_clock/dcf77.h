#pragma once

#include <datetime/datetime.h>
#include <furi.h>

/**
 * Set the current time and convert it into a dcf77 signal
 * @param datetime The time to set
 * @param is_dst If daylight saving time is active
 */
void set_dcf77_time(DateTime* datetime, bool is_dst);

/**
 * Get the signal bit for a second
 * @param sec The second to get the bit for
 */
bool get_dcf77_bit(int sec);

/**
 * Get the visual signal string to display in UI for a second (cropped to the last 25 bits because more cannot be displayed in one line)
 * @param sec The second to get the bit for
 */
char* get_dcf77_data(int sec);

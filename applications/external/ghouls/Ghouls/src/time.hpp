#pragma once
#include "general.hpp"

class Time
{
public:
    Time();
    ~Time();

    uint16_t getTime() const;                                           // Get the current time in ticks (modulo TICKS_PER_DAY)
    uint32_t getTimeInTicks() const;                                    // Get the current time value
    bool getTimeIn24HourFormat(uint8_t &hours, uint8_t &minutes) const; // Get the current time in 24-hour format (hours and minutes)
    TimeOfDay getTimeOfDay() const;                                     // Get the current time of day (day or night)
    void set(uint32_t time);                                            // Set time to a specific value
    void reset();                                                       // Reset time to 0
    void tick();                                                        // Advance time by one tick

private:
    uint32_t time;
};
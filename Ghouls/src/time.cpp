#include "time.hpp"

Time::Time() : time(0) {}

Time::~Time() {}

uint16_t Time::getTime() const
{
    return time % TICKS_PER_DAY;
}

uint32_t Time::getTimeInTicks() const
{
    return time;
}

bool Time::getTimeIn24HourFormat(uint8_t &hours, uint8_t &minutes) const
{
    uint16_t ticks = getTime();
    hours = (ticks * 24) / TICKS_PER_DAY;            // Scale ticks to 24 hours
    minutes = ((ticks * 1440) / TICKS_PER_DAY) % 60; // Scale ticks to 1440 minutes and get the remainder for minutes
    return true;
}

TimeOfDay Time::getTimeOfDay() const
{
    // will come back to this
    // I think I want every minute
    // flipper runs at 60fps, so 60 seconds is 3600 ticks
    // so we can say if time % 3600 < 1800 it's day, otherwise it's night
    return (time % TICKS_PER_DAY) < (TICKS_PER_DAY / 2) ? TIME_DAY : TIME_NIGHT;
}

void Time::set(uint32_t newTime)
{
    time = newTime;
}

void Time::reset()
{
    time = 0;
}

void Time::tick()
{
    time++;
}
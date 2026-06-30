#pragma once

#include "PagerDecoder.hpp"

#define TD157_ACTION_RING         0b0010
#define TD157_ACTION_TURN_OFF_ALL 0b1111
#define TD157_PAGER_TURN_OFF_ALL  999

// Retekess TD157
class Td157Decoder : public PagerDecoder {
private:
    const uint32_t stationMask = 0b111111111100000000000000; // leading 10 bits (of 24) are station
    const uint32_t pagerMask = 0b11111111110000; // next 10 bits are pager
    const uint32_t actionMask = 0b1111; // and the last 4 bits is action

    const uint8_t stationOffset = 14;
    const uint8_t pagerOffset = 4;

public:
    const char* GetShortName() {
        return "TD157";
    }

    uint16_t GetStation(uint32_t data) {
        uint32_t station = (data & stationMask) >> stationOffset;
        return (uint16_t)station;
    }

    uint16_t GetPager(uint32_t data) {
        uint32_t pager = (data & pagerMask) >> pagerOffset;
        return (uint16_t)pager;
    }

    uint32_t SetPager(uint32_t data, uint16_t pagerNum) {
        uint32_t pagerClearedData = data & ~pagerMask;
        return pagerClearedData | (pagerNum << pagerOffset);
    }

    uint8_t GetActionValue(uint32_t data) {
        return data & actionMask;
    }

    PagerAction GetAction(uint32_t data) {
        switch(GetActionValue(data)) {
        case TD157_ACTION_RING:
            return RING;
        case TD157_ACTION_TURN_OFF_ALL:
            if(GetPager(data) == TD157_PAGER_TURN_OFF_ALL) {
                return TURN_OFF_ALL;
            }
            return UNKNOWN;
        default:
            return UNKNOWN;
        }
    }

    uint32_t SetAction(uint32_t data, PagerAction action) {
        switch(action) {
        case RING:
            return SetActionValue(data, TD157_ACTION_RING);
        case TURN_OFF_ALL:
            return SetActionValue(SetPager(data, TD157_PAGER_TURN_OFF_ALL), TD157_ACTION_TURN_OFF_ALL);
        default:
            return data;
        }
    }

    virtual uint32_t SetActionValue(uint32_t data, uint8_t action) {
        return (data & ~actionMask) | action;
    }

    bool IsSupported(PagerAction action) {
        switch(action) {
        case RING:
        case TURN_OFF_ALL:
            return true;

        default:
            return false;
        }
        return false;
    }

    uint8_t GetActionsCount() {
        return actionMask + 1;
    }
};

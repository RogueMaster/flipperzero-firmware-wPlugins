#pragma once

#include "PagerDecoder.hpp"

#define TD165_ACTION_RING        0
#define TD165_ACTION_MUTE        1
#define TD165_PAGER_TURN_OFF_ALL 1005

// Retekess TD165/T119
class Td165Decoder : public PagerDecoder {
private:
    const uint32_t stationMask = 0b111111111111100000000000; // leading 13 bits (of 24) are station
    const uint32_t pagerMask = 0b11111111110; // next 10 bits are pager
    const uint32_t actionMask = 0b1; // and the last 1 bit is action

    const uint8_t stationBitCount = 13;
    const uint8_t stationOffset = 11;

    const uint8_t pagerBitCount = 10;
    const uint8_t pagerOffset = 1;

public:
    const char* GetShortName() {
        return "TD165";
    }

    uint16_t GetStation(uint32_t data) {
        uint32_t stationReversed = (data & stationMask) >> stationOffset;
        return reverseBits(stationReversed, stationBitCount);
    }

    uint16_t GetPager(uint32_t data) {
        uint32_t pagerReversed = (data & pagerMask) >> pagerOffset;
        return reverseBits(pagerReversed, pagerBitCount);
    }

    uint8_t GetActionValue(uint32_t data) {
        return data & actionMask;
    }

    PagerAction GetAction(uint32_t data) {
        switch(GetActionValue(data)) {
        case TD165_ACTION_RING:
            if(GetPager(data) == TD165_PAGER_TURN_OFF_ALL) {
                return TURN_OFF_ALL;
            }
            return RING;

        case TD165_ACTION_MUTE:
            return MUTE;

        default:
            return UNKNOWN;
        }
    }

    uint32_t SetPager(uint32_t data, uint16_t pagerNum) {
        uint32_t pagerCleared = data & ~pagerMask;
        return pagerCleared | (reverseBits(pagerNum, pagerBitCount) << pagerOffset);
    }

    uint32_t SetActionValue(uint32_t data, uint8_t actionValue) {
        return (data & ~actionMask) | actionValue;
    }

    uint32_t SetAction(uint32_t data, PagerAction action) {
        switch(action) {
        case RING:
            return SetActionValue(data, TD165_ACTION_RING);

        case MUTE:
            return SetActionValue(data, TD165_ACTION_MUTE);

        case TURN_OFF_ALL:
            return SetActionValue(SetPager(data, TD165_PAGER_TURN_OFF_ALL), TD165_ACTION_RING);

        default:
            return data;
        }
    }

    bool IsSupported(PagerAction action) {
        switch(action) {
        case RING:
        case MUTE:
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

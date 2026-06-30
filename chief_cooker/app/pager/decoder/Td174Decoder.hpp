#pragma once

#include "PagerDecoder.hpp"

#define TD174_ACTION_RING   0
#define TD174_ACTION_DESYNC 3
#define TD174_PAGER_DESYNC  237

// Retekess TD174
class Td174Decoder : public PagerDecoder {
private:
    const uint32_t stationMask = 0b111111111111100000000000; // leading 13 bits (of 24) are station
    const uint32_t actionMask = 0b11000000000; // next 2 bits are action
    const uint32_t pagerMask = 0b111111111; // and the last 9 bits is pager

    const uint8_t stationBitCount = 13;
    const uint8_t stationOffset = 11;

    const uint8_t actionBitCount = 2;
    const uint8_t actionOffset = 9;

    const uint8_t pagerBitCount = 9;

public:
    const char* GetShortName() {
        return "TD174";
    }

    uint16_t GetStation(uint32_t data) {
        uint32_t stationReversed = (data & stationMask) >> stationOffset;
        return reverseBits(stationReversed, stationBitCount);
    }

    uint16_t GetPager(uint32_t data) {
        uint32_t pagerReversed = data & pagerMask;
        return reverseBits(pagerReversed, pagerBitCount);
    }

    uint8_t GetActionValue(uint32_t data) {
        uint32_t actionReversed = (data & actionMask) >> actionOffset;
        return reverseBits(actionReversed, actionBitCount);
    }

    PagerAction GetAction(uint32_t data) {
        switch(GetActionValue(data)) {
        case TD174_ACTION_RING:
            return RING;

        case TD174_ACTION_DESYNC:
            if(GetPager(data) == TD174_PAGER_DESYNC) {
                return DESYNC;
            }
            return UNKNOWN;

        default:
            return UNKNOWN;
        }
    }

    uint32_t SetPager(uint32_t data, uint16_t pagerNum) {
        return (data & ~pagerMask) | reverseBits(pagerNum, pagerBitCount);
    }

    uint32_t SetActionValue(uint32_t data, uint8_t actionValue) {
        uint32_t actionCleared = data & ~actionMask;
        return actionCleared | (reverseBits(actionValue, actionBitCount) << actionOffset);
    }

    uint32_t SetAction(uint32_t data, PagerAction action) {
        switch(action) {
        case RING:
            return SetActionValue(data, TD174_ACTION_RING);

        case DESYNC:
            return SetActionValue(SetPager(data, TD174_PAGER_DESYNC), TD174_ACTION_DESYNC);

        default:
            return data;
        }
    }

    bool IsSupported(PagerAction action) {
        switch(action) {
        case RING:
        case DESYNC:
            return true;

        default:
            return false;
        }
        return false;
    }

    uint8_t GetActionsCount() {
        return 4;
    }
};

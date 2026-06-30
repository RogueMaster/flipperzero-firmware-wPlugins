#pragma once

#include "PagerDecoder.hpp"

#define T111_ACTION_RING 0

// Retekess T111 / L8R
// L8R — (L)ast (8) bits (R)eversed order (for pager number)
// seems to be Retekess T111 encoding, but cannot check it due to lack of information
// So I decided to keep it's name as L8R
class L8RDecoder : public PagerDecoder {
private:
    const uint32_t stationMask = 0b111111111111100000000000; // leading 13 bits (of 24) are station (any maybe more)
    const uint32_t actionMask = 0b11100000000; // next 3 bits are action (possibly, just my guess, may be they are also station)
    const uint32_t pagerMask = 0b11111111; // and the last 8 bits seem to be pager number

    const uint8_t stationBitCount = 13;
    const uint8_t stationOffset = 11;

    const uint8_t actionBitCount = 3;
    const uint8_t actionOffset = 8;

    const uint8_t pagerBitCount = 8;

public:
    const char* GetShortName() {
        return "L8R";
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
        case T111_ACTION_RING:
            return RING;

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
            return SetActionValue(data, T111_ACTION_RING);

        default:
            return data;
        }
    }

    bool IsSupported(PagerAction) {
        return false;
    }

    uint8_t GetActionsCount() {
        return 8;
    }
};

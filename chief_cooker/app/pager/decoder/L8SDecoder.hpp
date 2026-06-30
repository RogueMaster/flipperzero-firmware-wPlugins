#pragma once

#include "core/core_defines.h"

#include "PagerDecoder.hpp"

// iBells ZJ-68 / L8S
// L8S — (L)ast (8) bits (S)traight order (non-reversed) (for pager number)
class L8SDecoder : public PagerDecoder {
private:
    const uint32_t stationMask = 0b111111111111100000000000; // leading 13 bits (of 24) are station (let it be)
    const uint32_t actionMask = 0b11100000000; // next 3 bits are action (possibly, just my guess, may be they are also station)
    const uint32_t pagerMask = 0b11111111; // and the last 8 bits should be enough for pager number

    const uint8_t stationOffset = 11;
    const uint8_t actionOffset = 8;

public:
    const char* GetShortName() {
        return "L8S";
    }

    uint16_t GetStation(uint32_t data) {
        return (data & stationMask) >> stationOffset;
    }

    uint16_t GetPager(uint32_t data) {
        return data & pagerMask;
    }

    uint8_t GetActionValue(uint32_t data) {
        return (data & actionMask) >> actionOffset;
    }

    PagerAction GetAction(uint32_t data) {
        UNUSED(data);
        return UNKNOWN;
    }

    uint32_t SetPager(uint32_t data, uint16_t pagerNum) {
        return (data & ~pagerMask) | pagerNum;
    }

    uint32_t SetActionValue(uint32_t data, uint8_t actionValue) {
        uint32_t actionCleared = data & ~actionMask;
        return actionCleared | (actionValue << actionOffset);
    }

    uint32_t SetAction(uint32_t data, PagerAction action) {
        UNUSED(action);
        return data;
    }

    bool IsSupported(PagerAction) {
        return false;
    }

    uint8_t GetActionsCount() {
        return 8;
    }
};

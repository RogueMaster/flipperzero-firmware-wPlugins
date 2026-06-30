#pragma once

#include <lib/Arduboy2.h>

class Arduboy2Ext : public Arduboy2Base {
public:
    Arduboy2Ext();

    uint8_t justPressedButtons() const;
    uint8_t pressedButtons() const;
    uint16_t getFrameCount() const;
    void setFrameCount(uint16_t val) const;

    uint8_t getFrameCount(uint8_t mod, int8_t offset = 0) const;
    bool getFrameCountHalf(uint8_t mod) const;
    bool isFrameCount(uint8_t mod) const;
    bool isFrameCount(uint8_t mod, uint8_t val) const;

    void clearButtonState();
    void resetFrameCount();
    uint8_t randomLFSR(uint8_t min, uint8_t max);

private:
    uint8_t sampleButtonsMask() const;
    uint8_t getSampledButtons() const;

    mutable uint32_t sampled_frame = UINT32_MAX;
    mutable uint8_t previous_buttons = 0;
    mutable uint8_t current_buttons = 0;
    mutable int32_t frame_offset = 0;
};

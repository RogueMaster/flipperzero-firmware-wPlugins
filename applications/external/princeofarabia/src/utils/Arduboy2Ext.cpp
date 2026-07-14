#include "Arduboy2Ext.h"

Arduboy2Ext::Arduboy2Ext()
    : Arduboy2Base() {
}

uint8_t Arduboy2Ext::sampleButtonsMask() const {
    uint8_t out = 0;
    if(pressed(UP_BUTTON)) out |= UP_BUTTON;
    if(pressed(DOWN_BUTTON)) out |= DOWN_BUTTON;
    if(pressed(LEFT_BUTTON)) out |= LEFT_BUTTON;
    if(pressed(RIGHT_BUTTON)) out |= RIGHT_BUTTON;
    if(pressed(A_BUTTON)) out |= A_BUTTON;
    if(pressed(B_BUTTON)) out |= B_BUTTON;
    return out;
}

uint8_t Arduboy2Ext::getSampledButtons() const {
    const uint32_t frame = Arduboy2Base::frameCount();
    if(sampled_frame != frame) {
        previous_buttons = current_buttons;
        current_buttons = sampleButtonsMask();
        sampled_frame = frame;
    }
    return current_buttons;
}

uint8_t Arduboy2Ext::justPressedButtons() const {
    uint8_t out = 0;
    if(justPressed(UP_BUTTON)) out |= UP_BUTTON;
    if(justPressed(DOWN_BUTTON)) out |= DOWN_BUTTON;
    if(justPressed(LEFT_BUTTON)) out |= LEFT_BUTTON;
    if(justPressed(RIGHT_BUTTON)) out |= RIGHT_BUTTON;
    if(justPressed(A_BUTTON)) out |= A_BUTTON;
    if(justPressed(B_BUTTON)) out |= B_BUTTON;
    return out;
}

uint8_t Arduboy2Ext::pressedButtons() const {
    return getSampledButtons();
}

void Arduboy2Ext::clearButtonState() {
    Arduboy2Base::clearButtonState();
    sampled_frame = UINT32_MAX;
    previous_buttons = 0;
    current_buttons = 0;
}

void Arduboy2Ext::resetFrameCount() {
    frame_offset = -(int32_t)Arduboy2Base::frameCount();
}

uint16_t Arduboy2Ext::getFrameCount() const {
    return (uint16_t)((int32_t)Arduboy2Base::frameCount() + frame_offset);
}

void Arduboy2Ext::setFrameCount(uint16_t val) const {
    frame_offset = (int32_t)val - (int32_t)Arduboy2Base::frameCount();
}

uint8_t Arduboy2Ext::getFrameCount(uint8_t mod, int8_t offset) const {
    if(mod == 0) return 0;
    return (uint8_t)((getFrameCount() + offset) % mod);
}

bool Arduboy2Ext::getFrameCountHalf(uint8_t mod) const {
    return getFrameCount(mod) > (mod / 2);
}

bool Arduboy2Ext::isFrameCount(uint8_t mod) const {
    if(mod == 0) return false;
    return (getFrameCount() % mod) == 0;
}

bool Arduboy2Ext::isFrameCount(uint8_t mod, uint8_t val) const {
    if(mod == 0) return false;
    return (getFrameCount() % mod) == val;
}

static uint16_t rnd = 0xACE1;

uint8_t Arduboy2Ext::randomLFSR(uint8_t min, uint8_t max) {
    if(max <= min) return min;
    uint16_t r = rnd;
    r ^= (uint16_t)millis();
    (r & 1) ? r = (r >> 1) ^ 0xB400 : r >>= 1;
    rnd = r;
    return (uint8_t)(r % (max - min) + min);
}

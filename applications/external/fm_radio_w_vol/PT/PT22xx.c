#include "PT22xx.h"

#include "PT2257.h"
#include "PT2259.h"

static PT22xxChip pt22xx_chip = PT22xxChipPT2257;

void pt22xx_set_chip(PT22xxChip chip) {
    pt22xx_chip = chip;
}

PT22xxChip pt22xx_get_chip(void) {
    return pt22xx_chip;
}

const char* pt22xx_get_chip_name(void) {
    return (pt22xx_chip == PT22xxChipPT2259) ? "PT2259-S" : "PT2257";
}

void pt22xx_set_i2c_addr(uint8_t addr) {
    if(pt22xx_chip == PT22xxChipPT2259) {
        pt2259_set_i2c_addr(addr);
    } else {
        pt2257_set_i2c_addr(addr);
    }
}

uint8_t pt22xx_get_i2c_addr(void) {
    if(pt22xx_chip == PT22xxChipPT2259) {
        return pt2259_get_i2c_addr();
    } else {
        return pt2257_get_i2c_addr();
    }
}

bool pt22xx_is_device_ready(void) {
    if(pt22xx_chip == PT22xxChipPT2259) {
        return pt2259_is_device_ready();
    } else {
        return pt2257_is_device_ready();
    }
}

bool pt22xx_init(void) {
    if(pt22xx_chip == PT22xxChipPT2259) {
        return pt2259_init();
    } else {
        return true;
    }
}

bool pt22xx_apply_state(const PT22xxState* state) {
    if(!state) return false;

    if(pt22xx_chip == PT22xxChipPT2259) {
        bool ok = pt2259_set_mute(false);
        ok = ok && pt2259_set_attenuation_db(state->muted ? 79 : state->attenuation_db);
        ok = ok && pt2259_set_mute(state->muted);
        return ok;
    } else {
        bool ok = pt2257_set_attenuation_db(state->muted ? 79 : state->attenuation_db);
        ok = ok && pt2257_mute(state->muted);
        return ok;
    }
}

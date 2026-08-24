#include "tpms_radio.h"

const uint32_t tpms_frequencies[TPMS_FREQUENCY_COUNT] = {
    433920000UL, /* Europe */
    315000000UL, /* North America, Japan */
};

uint32_t tpms_slot_frequency(uint8_t slot) {
    return tpms_frequencies[(slot / 2) % TPMS_FREQUENCY_COUNT];
}

TpmsModulation tpms_slot_modulation(uint8_t slot) {
    return (slot & 1) ? TpmsModulationOok : TpmsModulationFsk;
}

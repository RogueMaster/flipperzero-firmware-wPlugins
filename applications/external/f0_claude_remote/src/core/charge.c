#include "charge.h"

static const uint8_t charge_full = 100;

/* Rounds up, as the firmware's own bar does (power.c:23): at 1 % the nearest would draw none. */
uint8_t charge_cells(uint8_t charge, uint8_t cells) {
    if(charge >= charge_full) {
        return cells;
    }
    return (charge * cells + charge_full - 1) / charge_full;
}

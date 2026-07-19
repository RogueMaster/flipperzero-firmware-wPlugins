#include "wyrm.h"

bool AddInnerPlate(Wyrm* wyrm, Plate plate) {
    if(wyrm->length >= MAX_PLATES) {
        return false;
    }

    for(int plateIndex = wyrm->length; plateIndex > 0; plateIndex--) {
        wyrm->plates[plateIndex] = wyrm->plates[plateIndex - 1];
    }

    wyrm->plates[0] = plate;
    wyrm->length++;

    return true;
}

bool AddOuterPlate(Wyrm* wyrm, Plate plate) {
    if(wyrm->length >= MAX_PLATES) {
        return false;
    }

    wyrm->plates[wyrm->length] = plate;
    wyrm->length++;

    return true;
}

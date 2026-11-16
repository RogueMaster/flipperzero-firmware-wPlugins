#include "charging.h"

static const uint8_t charging_full = 100;

/* The power service's own test for its charging state (power.c:91-104). */
bool charging_now(bool charger, uint8_t charge, bool done) {
    return charger && charge < charging_full && !done;
}

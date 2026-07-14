/***
 * Noptel LRF rangefinder sampler for the Flipper Zero
 * Version: 2.4
 *
 * Power control
***/

/*** Routines ***/

/** Turn the LRF on or off
    Control the LRF using the C1 pin, and the +5V pin if compiled in
    If a pointer to a uint32_t variable is passed, store the power change
    timestamp in that variable **/
void power_lrf(bool, uint32_t*);

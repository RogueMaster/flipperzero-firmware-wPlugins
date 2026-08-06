#ifndef GENERALS_H_
#define GENERALS_H_

/**
 * This files only works to set the default values to
 * MAC ADDRESS and IP ADDRESS just for help.
 */

#include <furi.h>
#include <furi_hal.h>

// MAC address
extern uint8_t MAC_ZEROS[6];
extern uint8_t MAC_BROADCAST[6];
extern uint8_t MAC_SPOOF[6];

// IP address
extern uint8_t IP_ZEROS[4];
extern uint8_t IP_BROADCAST[4];

#endif

/*
 * USB side of the Disney Infinity base emulator for the Flipper Zero.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once

#include <furi.h>

#include "infinity_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Counters the UI shows, so a failed run says how far the console got. */
typedef struct {
    volatile uint32_t enum_configs; /* times the host selected our configuration */
    volatile uint32_t out_packets; /* 32-byte commands received */
    volatile uint32_t in_packets; /* 32-byte replies/events sent */
    volatile uint32_t in_busy; /* sends that found the IN endpoint busy */
    volatile uint32_t ctrl_requests; /* control transfers seen */
    volatile uint32_t ctrl_rejected; /* control transfers we answered with a stall */
    volatile uint32_t suspends;
    volatile bool suspended; /* console put the bus to sleep or the cable was pulled */
    volatile uint32_t last_rx_tick; /* tick of the last command from the console */
    volatile uint8_t last_rejected_type;
    volatile uint8_t last_rejected_req;
} UsbInfStats;

extern UsbInfStats usb_inf_stats;
extern InfBase usb_inf_base; /* shared with the UI; wrap UI access in FURI_CRITICAL_ENTER/EXIT */

/* Switch the Flipper's USB to the Infinity base (remembers the previous mode). */
void usb_infinity_start(void);
/* Restore the previous USB mode. */
void usb_infinity_stop(void);
/* Push any queued packet to the host. Call after changing figures from the UI. */
void usb_infinity_kick(void);

#ifdef __cplusplus
}
#endif

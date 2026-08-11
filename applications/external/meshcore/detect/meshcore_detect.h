/*
 * DETECT lines — telling the node the Flipper is here.
 *
 * While this app runs it holds two GPIO pins HIGH. A node samples its detect
 * input **at boot** and picks a transport from it:
 *
 *   HIGH -> serial mode. The Flipper drives the node, its radio link stays down.
 *   LOW  -> the node brings up its own radio (V4: WiFi AP, T114: BLE) and you
 *           work from a phone.
 *
 * So the rule for a human is "plug in and reboot the node" / "unplug and reboot
 * the node". There is no hot switching, by design — the node only asks once.
 *
 * Two independent 4-pin blocks, one per UART, mirrored so the connectors do not
 * collide:
 *
 *   block A (USART)   11 GND | 12 DETECT | 13 TX | 14 RX
 *   block B (LPUART)  15 TX  | 16 RX     | 17 DETECT | 18 GND
 *
 * ⚠ Pin 12 is PA13 — also SWDIO. It is the only free pin in block A (11 is
 * ground, 13/14 are the USART), so there is no alternative. With the Flipper's
 * Debug mode enabled the debug peripheral owns that pin, and the detect level
 * cannot be trusted. meshcore_detect_conflicts_with_debug() reports that, and
 * the app says so on screen rather than lying about it.
 *
 * Pin 17 is PB14, otherwise the 1-Wire pin, and is free.
 */
#pragma once

#include <furi.h>

#include <stdbool.h>

typedef struct {
    /* What the pin actually read back after being driven. On a push-pull
     * output this is the real pin level, so a mismatch means something else is
     * holding the line. */
    bool block_a_high;
    bool block_b_high;
    bool debug_mode; /* Flipper Debug setting, which claims PA13 */
} MeshCoreDetectState;

/** Drive both detect lines HIGH. Call once when the app starts. */
void meshcore_detect_init(void);

/** Release both lines. Call once when the app exits, so a node rebooted later
 *  sees LOW and comes up on its own radio. */
void meshcore_detect_deinit(void);

/** Read back what the lines are actually doing. */
MeshCoreDetectState meshcore_detect_state(void);

/** True when Debug mode is on, which makes the block A line unreliable. */
bool meshcore_detect_conflicts_with_debug(void);

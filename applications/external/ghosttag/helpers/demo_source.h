#pragma once

#include <furi.h>
#include "ble_signatures.h"

/**
 * Demo mode: a scripted stalking scenario, synthesised on the Flipper itself.
 *
 * GhostTag cannot see a single Bluetooth advertisement without the ESP32
 * companion board, because the Flipper's own BLE radio is advertising-only and
 * physically cannot scan. That left the app a brick for everybody who had not
 * bought the board yet - they installed it, opened it, saw NO ESP, and had no
 * way to find out what the app even does.
 *
 * Demo mode replays a scripted scenario instead: a handful of ambient devices,
 * a Tile that walks past and leaves, and an AirTag that stays with you and
 * trips a real alert. It drives the genuine tracker database, the genuine
 * follow heuristic and the genuine UI, so what you see is what a real hunt
 * looks like - only the radio traffic is invented.
 *
 * HONESTY: this is simulated data and it must never be mistakable for a
 * measurement. Every screen painted while the demo is running carries a DEMO
 * stamp, the alert says SIMULATED on its face, and nothing from a demo session
 * is ever written to the session log.
 */

/** Same shape as the UART link's callback, so the app is source-agnostic. */
typedef void (*DemoSourceRxCallback)(
    void* context,
    const uint8_t mac[6],
    TrackerType type,
    int8_t rssi,
    const char* name);

typedef struct DemoSource DemoSource;

/**
 * The dwell a demo tracker must survive before it is called a follower.
 *
 * Deliberately not the user's "Alert after" setting: nobody is going to sit and
 * stare at a simulation for three minutes to find out what the alert looks
 * like. The scenario is scaled so the AirTag trips this at roughly 25 s in.
 */
#define DEMO_FOLLOW_MS 18000UL

DemoSource* demo_source_alloc(void);
void demo_source_free(DemoSource* demo);

void demo_source_set_callback(DemoSource* demo, DemoSourceRxCallback cb, void* context);

void demo_source_start(DemoSource* demo);
void demo_source_stop(DemoSource* demo);
bool demo_source_is_running(DemoSource* demo);

/** Milliseconds since the scenario started, for the on-screen progress hint. */
uint32_t demo_source_elapsed_ms(DemoSource* demo);

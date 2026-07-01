#pragma once
/*
 * capability.h — tap capability checker (ESP32, #125).
 *
 * Listens passively for a few seconds, counts capability-relevant CAN ids per
 * id per bus, then calls the pure verdict logic (fsd_logic/fsd_capability.h) to
 * answer per FEATURE — not per bus name — "will the nag killer / AP-First / FSD
 * activation / Soft Engage work on the tap this device is plugged into?".
 *
 * Pure RX / read-only: this module never transmits. It only reads the same RX
 * frames process_frame() already handles. An id counts as "present" once it has
 * been seen >= CAP_MIN_FRAMES times in the window (ignores a single spurious
 * frame). A check runs automatically when the dashboard connects and on demand
 * via the "capability_recheck" WS command.
 *
 * Single-threaded counters: capability_record() is called only from the CAN
 * path; capability_status_json() reads from the web task. Counters are small and
 * monotonic within a window, so a torn read at the >= CAP_MIN_FRAMES threshold
 * is harmless.
 */

#include <Arduino.h>
#include "can_driver.h"   // CanBusId, CAN_BUS_COUNT
#include "fsd_handler.h"  // FSDState, CanFrame

// Listen window and presence threshold (see fsd_logic/fsd_capability.h).
#define CAP_WINDOW_MS    4000u
#define CAP_MIN_FRAMES   3u

// Wire up the shared state (for hw_version) + its mux. Call once from setup().
void capability_init(FSDState* state, portMUX_TYPE* state_mux);

// Count one RX frame toward the active window. Cheap; no-op when not running.
void capability_record(CanBusId bus, const CanFrame& frame, uint32_t now_ms);

// Begin (or restart) a listen window: zero the counters and arm the deadline.
void capability_start(uint32_t now_ms);

// Finalize the window once CAP_WINDOW_MS has elapsed. Call once per loop().
void capability_tick(uint32_t now_ms);

// Dashboard payload: {"state":..,"ms_left":..,"hw":..,"buses":[{...}]}.
String capability_status_json();

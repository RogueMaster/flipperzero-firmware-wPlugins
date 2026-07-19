#pragma once

#include "glitch_config.h"

/* --------------------------------------------------------------------------
 * Glitch pulse engine.
 *
 * Turns a GlitchParams into a real waveform on a GPIO header pin with
 * cycle-accurate timing (busy-wait on the Cortex-M4 DWT cycle counter, run
 * inside a critical section so nothing else on the MCU can jitter the pulse).
 *
 * Three ways to fire:
 *   - fire()          : one shot, now, from the UI thread (Manual / Repeat).
 *   - arm_external()  : a GPIO interrupt fires the shot the instant an edge
 *                       arrives on the trigger-in pin (lowest latency).
 *
 * This is a bench instrument for fault-injection research on YOUR OWN boards.
 * The Flipper pin is 3V3 push-pull and cannot switch a target rail by itself -
 * it drives a gate driver / MOSFET crowbar. See the in-app Wiring screen.
 * ------------------------------------------------------------------------ */

typedef struct GlitchEngine GlitchEngine;

GlitchEngine* glitch_engine_alloc(void);
void glitch_engine_free(GlitchEngine* e);

/* Claim the output (and, for external mode, input) pins from params and drive
 * the output to its idle level. Call on entering a firing screen. */
void glitch_engine_configure(GlitchEngine* e, const GlitchParams* p);

/* Return the pins to high-impedance (analog). Call on leaving a firing screen. */
void glitch_engine_release(GlitchEngine* e);

/* Fire exactly one shot synchronously (UI thread). Safe for Manual & Repeat. */
void glitch_engine_fire(GlitchEngine* e, const GlitchParams* p);

/* Arm/disarm the hardware external trigger (edge -> shot, in interrupt ctx). */
void glitch_engine_arm_external(GlitchEngine* e, const GlitchParams* p);
void glitch_engine_disarm(GlitchEngine* e);
bool glitch_engine_is_armed(const GlitchEngine* e);

/* Live counters (updated from both thread and interrupt context). */
uint32_t glitch_engine_shots(const GlitchEngine* e);
void glitch_engine_reset_shots(GlitchEngine* e);
uint32_t glitch_engine_last_fire_tick(const GlitchEngine* e);

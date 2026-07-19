#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* --------------------------------------------------------------------------
 * The glitch parameter model.
 *
 * A "shot" is one arm-to-glitch sequence:
 *
 *      trigger ──► [ delay ] ──► ┌pulse┐ [gap] ┌pulse┐ ... ──► idle
 *
 * Everything the pulse engine needs lives in GlitchParams. Values are kept in
 * their natural units (ns / us / ms) so the engine can convert straight to CPU
 * cycles; the UI only ever nudges them along a "nice number" ladder so a single
 * knob covers a huge dynamic range without endless scrolling.
 * ------------------------------------------------------------------------ */

typedef enum {
    GlitchPolarityActiveHigh, // idle LOW, pulse drives HIGH (drives an N-FET high-side gate driver)
    GlitchPolarityActiveLow, // idle HIGH, pulse drives LOW  (active-low crowbar / open-drain gate)
    GlitchPolarityCount,
} GlitchPolarity;

typedef enum {
    GlitchTriggerManual, // OK button fires each shot
    GlitchTriggerExternal, // an edge on the trigger-in pin fires a shot
    GlitchTriggerRepeat, // free-running: fire a shot every repeat interval
    GlitchTriggerCount,
} GlitchTriggerMode;

typedef enum {
    GlitchEdgeRising,
    GlitchEdgeFalling,
    GlitchEdgeCount,
} GlitchEdge;

typedef struct {
    uint32_t delay_us; // trigger -> first pulse offset
    uint32_t width_ns; // pulse width (the glitch itself)
    uint32_t gap_us; // spacing between pulses in a burst
    uint16_t pulses; // pulses per shot (1 = single glitch)
    GlitchPolarity polarity;
    GlitchTriggerMode trigger;
    GlitchEdge ext_edge; // which edge arms an external shot
    uint32_t repeat_ms; // interval for GlitchTriggerRepeat

    /* Parameter sweep: walk width_ns from `sweep_from` to `sweep_to` in
     * `sweep_step` increments, firing at each point. The classic way to hunt
     * for the width that faults a given target. */
    bool sweep_enabled;
    uint32_t sweep_from_ns;
    uint32_t sweep_to_ns;
    uint32_t sweep_step_ns;

    uint8_t out_pin; // index into glitch_pins[]  (trigger output)
    uint8_t in_pin; // index into glitch_pins[]  (external trigger input)
} GlitchParams;

/* Fill p with safe, sensible defaults (100 us delay, 500 ns single pulse). */
void glitch_params_default(GlitchParams* p);

/* ---- "nice number" ladders (1-2-5 decades) -----------------------------
 * Each field the UI edits cycles through one of these tables. The helpers
 * return the ladder, its length, and the nearest index to an arbitrary value
 * so config screens can seed themselves from the live params. */

extern const uint32_t glitch_ladder_delay_us[];
extern const size_t glitch_ladder_delay_len;
extern const uint32_t glitch_ladder_width_ns[];
extern const size_t glitch_ladder_width_len;
extern const uint32_t glitch_ladder_gap_us[];
extern const size_t glitch_ladder_gap_len;
extern const uint16_t glitch_ladder_pulses[];
extern const size_t glitch_ladder_pulses_len;
extern const uint32_t glitch_ladder_repeat_ms[];
extern const size_t glitch_ladder_repeat_len;

size_t glitch_ladder_nearest_u32(const uint32_t* tbl, size_t len, uint32_t v);
size_t glitch_ladder_nearest_u16(const uint16_t* tbl, size_t len, uint16_t v);

/* ---- formatters --------------------------------------------------------
 * Auto-scale to ns / us / ms with two significant decimals, e.g.
 *   500      -> "500 ns"      2000 -> "2.0 us"    1500000 -> "1.5 ms" */
void glitch_fmt_ns(uint32_t ns, char* buf, size_t n);
void glitch_fmt_us(uint32_t us, char* buf, size_t n);
void glitch_fmt_ms(uint32_t ms, char* buf, size_t n);

const char* glitch_polarity_label(GlitchPolarity p);
const char* glitch_trigger_label(GlitchTriggerMode m);
const char* glitch_edge_label(GlitchEdge e);

/* ---- output/input pin catalogue ---------------------------------------- */
typedef struct {
    const char* name; // GPIO name, e.g. "PA7"
    uint8_t header; // Flipper GPIO header pin number, e.g. 2
} GlitchPinInfo;

extern const GlitchPinInfo glitch_pins[];
extern const size_t glitch_pins_len;

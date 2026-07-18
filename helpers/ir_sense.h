#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <stdbool.h>
#include <stdint.h>

/* Nyx senses infrared two different ways, because the Flipper's own IR receiver
 * physically cannot do the whole job.
 *
 *   ONBOARD — the built-in TSOP-75338 on PA0. It is a *demodulating* receiver:
 *     a band-pass filter centred on 38 kHz plus AGC, designed to pull remote
 *     control codes out of a sunlit room. That filter throws away anything that
 *     is not modulated near its centre frequency, so a covert camera whose
 *     illuminator runs at steady DC current is INVISIBLE to it, no matter how
 *     bright. What it does catch is anything pulsed: remotes, IR beacons and
 *     link ports, PIR floodlights, and PWM-driven illuminators whose switching
 *     edges carry enough harmonic energy into the passband.
 *
 *   PROBE — an external IR phototransistor on a GPIO ADC pin. A bare junction
 *     has no filter and no AGC, so it responds to steady light. This is the
 *     mode that actually finds a night-vision camera, and it can also tell a
 *     DC illuminator apart from a mains-flickering lamp by looking at ripple.
 *
 * Both paths are listen-only. Nyx never emits IR.
 */

#define NYX_TRACE_LEN 64u // samples kept for the on-screen sweep trace

typedef enum {
    IrSenseModeAuto, // probe if one is plugged in, else onboard
    IrSenseModeOnboard,
    IrSenseModeProbe,
} IrSenseMode;

typedef enum {
    IrSenseErrorNone,
    IrSenseErrorIrBusy, // another IR app holds the receiver
    IrSenseErrorNoProbe, // probe mode forced, but nothing is on the pin
    IrSenseErrorAdcBusy, // could not take the ADC
} IrSenseError;

/* What the emitter looks like, once we can see it well enough to say. */
typedef enum {
    IrSourceNone,
    IrSourceSteady, /**< flat DC light — the night-vision illuminator signature */
    IrSourceFlicker, /**< ~100/120 Hz — mains-driven lamp, almost never a camera */
    IrSourcePulsed, /**< fast modulation — remote, beacon, or a PWM'd illuminator */
} IrSourceKind;

typedef struct {
    bool armed;
    IrSenseError error;
    IrSenseMode active_mode; // never Auto — what Auto actually resolved to
    bool probe_present; // a probe was detected on the pin

    bool present; // something is emitting IR right now
    uint8_t level; // 0..100 smoothed signal strength
    uint8_t peak; // 0..100 strongest since reset
    int8_t trend; // -1 falling, 0 flat, +1 rising — the "getting warmer" cue
    IrSourceKind kind;
    uint32_t hits; // distinct detections since reset
    uint32_t last_seen_tick;

    /* Probe telemetry (0 in onboard mode) */
    uint16_t baseline_mv; // ambient null point captured when the sweep armed
    uint16_t raw_mv; // what the pin reads right now
    uint16_t ripple_mv; // peak-to-peak within the last window
    uint16_t freq_hz; // dominant ripple frequency, when there is one

    /* Onboard telemetry (0 in probe mode) */
    uint32_t edges_per_sec; // IR receiver output transitions — the activity metric

    uint8_t trace[NYX_TRACE_LEN]; // ring buffer of recent level
    uint8_t trace_head; // index of the newest sample
} IrStats;

typedef struct IrSense IrSense;

IrSense* ir_sense_alloc(void);
void ir_sense_free(IrSense* s);

/* Which path to use. Takes effect on the next start(). */
void ir_sense_set_mode(IrSense* s, IrSenseMode mode);

/* Sensitivity 0 = High, 1 = Medium, 2 = Low. Sets the noise floor a window must
 * clear to count, and the full-scale point of the meter. */
void ir_sense_set_sensitivity(IrSense* s, uint8_t index);

/* ADC pin for the probe, as an index into ir_sense_probe_pins(). */
void ir_sense_set_probe_pin(IrSense* s, uint8_t pin_index);

void ir_sense_start(IrSense* s);
void ir_sense_stop(IrSense* s);
bool ir_sense_is_running(IrSense* s);

/* Clear peak / hits / trace without dropping the sensor. */
void ir_sense_reset(IrSense* s);

/* Re-capture the ambient baseline (probe mode). Use after walking into a room
 * with different lighting, or to null out a lamp you have already cleared. */
void ir_sense_renull(IrSense* s);

/* Atomically copy the latest stats out for the UI. */
void ir_sense_get(IrSense* s, IrStats* out);

/* ---- probe pin table, built from the SDK's own GPIO records ---- */

typedef struct {
    const GpioPin* pin;
    const char* name; // "PC0"
    uint8_t header_number; // silkscreened pin number on the Flipper
    FuriHalAdcChannel channel;
} IrProbePin;

/* Every external GPIO the SDK says is wired to an ADC channel. */
const IrProbePin* ir_sense_probe_pins(void);
uint8_t ir_sense_probe_pin_count(void);

/* Is a probe wired to this pin right now?
 *
 * The documented wiring puts a 10k load resistor from the pin to GND. Driving
 * the pin as a digital input with the internal pull-up (~40k) turns that into a
 * divider: a floating pin is pulled to 3V3 and reads high, whereas a loaded pin
 * settles near 0.7 V and reads low. So a low reading means something is
 * connected. Leaves the pin back in analog mode.
 */
bool ir_sense_probe_detect(uint8_t pin_index);

const char* ir_sense_source_kind_str(IrSourceKind kind);

/* ---- one-shot probe meter, for the setup screen ----
 *
 * The sweep worker owns the ADC while it runs, so the setup screen gets its own
 * lightweight handle instead. Only ever alloc'd while the sweep is stopped.
 */

typedef struct IrProbeMeter IrProbeMeter;

IrProbeMeter* ir_probe_meter_alloc(uint8_t pin_index);
void ir_probe_meter_free(IrProbeMeter* m);

/* Current pin voltage in mV. */
uint16_t ir_probe_meter_read_mv(IrProbeMeter* m);

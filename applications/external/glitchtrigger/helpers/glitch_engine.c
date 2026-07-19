#include "glitch_engine.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <stm32wbxx.h> // DWT->CYCCNT, CoreDebug, SystemCoreClock

/* Longest stretch we are willing to spend with interrupts disabled in one go.
 * The bulk of a long arm-delay is done with interrupts ON; only the final,
 * precision-critical slice (fine delay + a pulse edge) runs masked. Keeping
 * this ~1 ms stops long delays / bursts from ever freezing BLE, USB or the
 * system tick. */
#define GLITCH_CRIT_CAP_US 1000u
/* From an ISR we cannot furi_delay for the coarse part, so cap the whole
 * in-interrupt fine delay. External-trigger campaigns use small delays anyway. */
#define GLITCH_ISR_MAX_US 1000u

/* Index -> GpioPin*. Order MUST match glitch_pins[] in glitch_config.c. */
static const GpioPin* const glitch_gpio[] = {
    &gpio_ext_pa7, // PA7  pin 2
    &gpio_ext_pa6, // PA6  pin 3
    &gpio_ext_pa4, // PA4  pin 4
    &gpio_ext_pb3, // PB3  pin 5
    &gpio_ext_pb2, // PB2  pin 6
    &gpio_ext_pc3, // PC3  pin 7
};

struct GlitchEngine {
    const GpioPin* out;
    const GpioPin* in;
    bool out_configured;
    bool armed_external;
    GlitchParams armed; // snapshot the ISR fires from

    volatile uint32_t shots;
    volatile uint32_t last_fire_tick;
};

/* ---- cycle-accurate primitives ----------------------------------------- */

static inline uint32_t glitch_ns_to_cycles(uint32_t ns) {
    return (uint32_t)(((uint64_t)ns * SystemCoreClock) / 1000000000ULL);
}
static inline uint32_t glitch_us_to_cycles(uint32_t us) {
    return (uint32_t)(((uint64_t)us * SystemCoreClock) / 1000000ULL);
}

/* Busy-wait `cycles` CPU cycles. Wrap-safe: unsigned subtraction. If `cycles`
 * is smaller than the loop's own overhead this returns at once - which is
 * exactly the shortest pulse the hardware can produce. */
static inline void glitch_delay_cycles(uint32_t cycles) {
    uint32_t start = DWT->CYCCNT;
    while((uint32_t)(DWT->CYCCNT - start) < cycles) {
    }
}

static inline void glitch_dwt_enable(void) {
    /* furi already runs the cycle counter for its own timers; make sure the
     * enable bits are set without ever resetting CYCCNT (that would disturb
     * in-flight cortex timers). Both writes are idempotent. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ---- the shot ---------------------------------------------------------- */

static void glitch_do_shot(GlitchEngine* e, const GlitchParams* p, bool allow_coarse) {
    const GpioPin* out = e->out;
    if(!out) return;

    const bool active = (p->polarity == GlitchPolarityActiveHigh);
    const bool idle = !active;

    const uint32_t width_cy = glitch_ns_to_cycles(p->width_ns);
    const uint16_t pulses = p->pulses ? p->pulses : 1;

    /* Split the arm delay: coarse part with interrupts ON (so a long delay never
     * freezes the system), fine remainder masked for a jitter-free edge. */
    uint32_t fine_us = p->delay_us;
    if(allow_coarse) {
        if(fine_us > GLITCH_CRIT_CAP_US) {
            furi_delay_us(fine_us - GLITCH_CRIT_CAP_US); // interrupts ON
            fine_us = GLITCH_CRIT_CAP_US;
        }
    } else {
        if(fine_us > GLITCH_ISR_MAX_US) fine_us = GLITCH_ISR_MAX_US;
    }
    const uint32_t fine_cy = glitch_us_to_cycles(fine_us);

    furi_hal_gpio_write(out, idle);

    /* First pulse: fine delay + the pulse in one masked window, so the
     * trigger-to-glitch edge carries no interrupt jitter. */
    FURI_CRITICAL_ENTER();
    glitch_delay_cycles(fine_cy);
    furi_hal_gpio_write(out, active);
    glitch_delay_cycles(width_cy);
    furi_hal_gpio_write(out, idle);
    FURI_CRITICAL_EXIT();

    /* Remaining pulses of a burst: the inter-pulse gap runs with interrupts ON
     * (its exact spacing is far less critical than the edge), and only each
     * short pulse is masked. This bounds every masked window to ~1 ms + width. */
    for(uint16_t i = 1; i < pulses; i++) {
        if(p->gap_us) furi_delay_us(p->gap_us);
        FURI_CRITICAL_ENTER();
        furi_hal_gpio_write(out, active);
        glitch_delay_cycles(width_cy);
        furi_hal_gpio_write(out, idle);
        FURI_CRITICAL_EXIT();
    }

    e->shots++;
    e->last_fire_tick = furi_get_tick();
}

static void glitch_isr(void* ctx) {
    GlitchEngine* e = ctx;
    if(!e->armed_external) return;
    glitch_do_shot(e, &e->armed, false);
}

/* ---- lifecycle --------------------------------------------------------- */

GlitchEngine* glitch_engine_alloc(void) {
    GlitchEngine* e = malloc(sizeof(GlitchEngine));
    e->out = NULL;
    e->in = NULL;
    e->out_configured = false;
    e->armed_external = false;
    e->shots = 0;
    e->last_fire_tick = 0;
    glitch_dwt_enable();
    return e;
}

void glitch_engine_free(GlitchEngine* e) {
    furi_assert(e);
    glitch_engine_disarm(e);
    glitch_engine_release(e);
    free(e);
}

static const GpioPin* glitch_pin_for(uint8_t idx) {
    if(idx >= glitch_pins_len) idx = 0;
    return glitch_gpio[idx];
}

void glitch_engine_configure(GlitchEngine* e, const GlitchParams* p) {
    furi_assert(e);
    e->out = glitch_pin_for(p->out_pin);
    e->in = glitch_pin_for(p->in_pin);

    const bool active = (p->polarity == GlitchPolarityActiveHigh);
    furi_hal_gpio_init(e->out, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_write(e->out, !active); // sit at idle level
    e->out_configured = true;
}

void glitch_engine_release(GlitchEngine* e) {
    furi_assert(e);
    glitch_engine_disarm(e);
    if(e->out_configured && e->out) {
        furi_hal_gpio_init_simple(e->out, GpioModeAnalog); // high-Z, safe
        e->out_configured = false;
    }
}

void glitch_engine_fire(GlitchEngine* e, const GlitchParams* p) {
    furi_assert(e);
    glitch_do_shot(e, p, true);
}

void glitch_engine_arm_external(GlitchEngine* e, const GlitchParams* p) {
    furi_assert(e);
    if(e->armed_external) return;
    e->armed = *p;

    // idle the output before the trigger can arrive
    const bool active = (p->polarity == GlitchPolarityActiveHigh);
    if(e->out) furi_hal_gpio_write(e->out, !active);

    const GpioMode mode =
        (p->ext_edge == GlitchEdgeRising) ? GpioModeInterruptRise : GpioModeInterruptFall;
    const GpioPull pull = (p->ext_edge == GlitchEdgeRising) ? GpioPullDown : GpioPullUp;

    furi_hal_gpio_init(e->in, mode, pull, GpioSpeedVeryHigh);
    furi_hal_gpio_add_int_callback(e->in, glitch_isr, e);
    furi_hal_gpio_enable_int_callback(e->in);
    e->armed_external = true;
}

void glitch_engine_disarm(GlitchEngine* e) {
    furi_assert(e);
    if(!e->armed_external) return;
    furi_hal_gpio_remove_int_callback(e->in);
    furi_hal_gpio_init_simple(e->in, GpioModeAnalog);
    e->armed_external = false;
}

bool glitch_engine_is_armed(const GlitchEngine* e) {
    return e->armed_external;
}

uint32_t glitch_engine_shots(const GlitchEngine* e) {
    return e->shots;
}

void glitch_engine_reset_shots(GlitchEngine* e) {
    e->shots = 0;
}

uint32_t glitch_engine_last_fire_tick(const GlitchEngine* e) {
    return e->last_fire_tick;
}

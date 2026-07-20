#include "glitch_config.h"
#include <stdio.h>

/* Ladders: 1-2-5 per decade so one knob spans a wide range with round stops. */
const uint32_t glitch_ladder_delay_us[] =
    {0, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000};
const size_t glitch_ladder_delay_len =
    sizeof(glitch_ladder_delay_us) / sizeof(glitch_ladder_delay_us[0]);

/* 62 ns ~= 4 CPU cycles @ 64 MHz: the shortest pulse we can shape cleanly. */
const uint32_t glitch_ladder_width_ns[] =
    {62, 125, 250, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000};
const size_t glitch_ladder_width_len =
    sizeof(glitch_ladder_width_ns) / sizeof(glitch_ladder_width_ns[0]);

const uint32_t glitch_ladder_gap_us[] =
    {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000};
const size_t glitch_ladder_gap_len =
    sizeof(glitch_ladder_gap_us) / sizeof(glitch_ladder_gap_us[0]);

const uint16_t glitch_ladder_pulses[] = {1, 2, 3, 4, 5, 8, 10, 16, 20, 32, 50, 64};
const size_t glitch_ladder_pulses_len =
    sizeof(glitch_ladder_pulses) / sizeof(glitch_ladder_pulses[0]);

const uint32_t glitch_ladder_repeat_ms[] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000};
const size_t glitch_ladder_repeat_len =
    sizeof(glitch_ladder_repeat_ms) / sizeof(glitch_ladder_repeat_ms[0]);

const uint16_t glitch_ladder_dwell[] = {1, 2, 3, 5, 10, 20, 50, 100};
const size_t glitch_ladder_dwell_len =
    sizeof(glitch_ladder_dwell) / sizeof(glitch_ladder_dwell[0]);

/* Output pins are 3V3-tolerant GPIO on the top header. These four avoid the
 * SPI/UART/I2C lines apps commonly want, so they are safe to bit-bang. */
const GlitchPinInfo glitch_pins[] = {
    {"PA7", 2},
    {"PA6", 3},
    {"PA4", 4},
    {"PB3", 5},
    {"PB2", 6},
    {"PC3", 7},
};
const size_t glitch_pins_len = sizeof(glitch_pins) / sizeof(glitch_pins[0]);

void glitch_params_default(GlitchParams* p) {
    p->delay_us = 100;
    p->width_ns = 500;
    p->gap_us = 10;
    p->pulses = 1;
    p->polarity = GlitchPolarityActiveHigh;
    p->trigger = GlitchTriggerManual;
    p->ext_edge = GlitchEdgeRising;
    p->repeat_ms = 200;
    p->sweep_enabled = false;
    p->sweep_from_ns = 125;
    p->sweep_to_ns = 5000;
    p->sweep_step_ns = 125;
    p->sweep_2d = false;
    p->sweep_delay_from_us = 0;
    p->sweep_delay_to_us = 500;
    p->sweep_delay_step_us = 50;
    p->sweep_dwell = 1;
    p->fb_pin = 5; // PC3 / header pin 7 (distinct from out & trig-in)
    p->fb_active_high = true;
    p->auto_hit = false;
    p->log_hits = false;
    p->out_pin = 0; // PA7 / header pin 2
    p->in_pin = 4; // PB2 / header pin 6
}

size_t glitch_ladder_nearest_u32(const uint32_t* tbl, size_t len, uint32_t v) {
    size_t best = 0;
    uint32_t best_d = UINT32_MAX;
    for(size_t i = 0; i < len; i++) {
        uint32_t d = tbl[i] > v ? tbl[i] - v : v - tbl[i];
        if(d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

size_t glitch_ladder_nearest_u16(const uint16_t* tbl, size_t len, uint16_t v) {
    size_t best = 0;
    uint16_t best_d = UINT16_MAX;
    for(size_t i = 0; i < len; i++) {
        uint16_t d = tbl[i] > v ? tbl[i] - v : v - tbl[i];
        if(d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

/* Pick the friendliest unit and show up to one decimal (e.g. "2.0 us"). */
static void fmt_scaled(uint32_t value_ns, char* buf, size_t n) {
    if(value_ns < 1000u) {
        snprintf(buf, n, "%lu ns", (unsigned long)value_ns);
    } else if(value_ns < 1000000u) {
        uint32_t whole = value_ns / 1000u;
        uint32_t frac = (value_ns % 1000u) / 100u;
        snprintf(buf, n, "%lu.%lu us", (unsigned long)whole, (unsigned long)frac);
    } else {
        uint32_t whole = value_ns / 1000000u;
        uint32_t frac = (value_ns % 1000000u) / 100000u;
        snprintf(buf, n, "%lu.%lu ms", (unsigned long)whole, (unsigned long)frac);
    }
}

void glitch_fmt_ns(uint32_t ns, char* buf, size_t n) {
    fmt_scaled(ns, buf, n);
}

void glitch_fmt_us(uint32_t us, char* buf, size_t n) {
    if(us == 0) {
        snprintf(buf, n, "0 us");
        return;
    }
    fmt_scaled(us * 1000u, buf, n);
}

void glitch_fmt_ms(uint32_t ms, char* buf, size_t n) {
    fmt_scaled(ms * 1000000u, buf, n);
}

const char* glitch_polarity_label(GlitchPolarity p) {
    switch(p) {
    case GlitchPolarityActiveHigh:
        return "Active-High";
    case GlitchPolarityActiveLow:
        return "Active-Low";
    default:
        return "?";
    }
}

const char* glitch_trigger_label(GlitchTriggerMode m) {
    switch(m) {
    case GlitchTriggerManual:
        return "Manual";
    case GlitchTriggerExternal:
        return "External";
    case GlitchTriggerRepeat:
        return "Repeat";
    default:
        return "?";
    }
}

const char* glitch_edge_label(GlitchEdge e) {
    switch(e) {
    case GlitchEdgeRising:
        return "Rising";
    case GlitchEdgeFalling:
        return "Falling";
    default:
        return "?";
    }
}

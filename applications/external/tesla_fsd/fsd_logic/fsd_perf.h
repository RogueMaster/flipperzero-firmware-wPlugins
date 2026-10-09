#pragma once
/*
 * fsd_perf.h — read-only performance read-out logic, shared header-only.
 *
 * Pure, host-testable math for the ESP32 web dashboard "Performance" card. It
 * derives timing/acceleration/slip/temperature read-outs from CAN signals the
 * firmware ALREADY receives (vehicle speed 0x257, wheel speeds 0x175, BMS temp
 * 0x312). It transmits nothing and adds no TX path — every value here is a
 * function of frames already parsed into FSDState.
 *
 * Kept free of FSDState / Arduino deps (plain floats + a POD struct) so the
 * timer state machine and dv/dt estimate unit-test on the host without the web
 * layer. The ESP32 build stores one FSDPerf in FSDState and calls
 * fsd_perf_update() from the 0x257 RX path with millis(); the Flipper build
 * leaves it inert (no web dashboard).
 */

#include <stdbool.h>
#include <stdint.h>

// ── Thresholds / constants ──────────────────────────────────────────────────
#define PERF_STANDSTILL_KPH   1.0f // at or below = stopped (arm/reset point)
#define PERF_T_50_KPH         50.0f
#define PERF_T_100_KPH        100.0f
#define PERF_T_60MPH_KPH      96.56064f // 60 mph exactly (0-60 mph timer)
#define PERF_BRAKE_FROM_KPH   100.0f // braking test starts crossing down through here
#define PERF_BRAKE_CANCEL_KPH 2.0f // re-accel by this much before stop -> cancel run
#define PERF_SLIP_MIN_KPH     3.0f // wheel slip undefined below this front-axle speed
#define PERF_G_MS2            9.80665f // 1 g in m/s^2

// dv/dt is only trusted for sample gaps in this window (ms): too short = noise /
// timer jitter divides to a huge number; too long = a stale baseline.
#define PERF_DT_MIN_MS 20u
#define PERF_DT_MAX_MS 2000u

// Battery-pack temperature UI advisory bands (deg C). Heuristic display
// thresholds, NOT from a DBC — regen/power are limited cold, and the pack runs
// warm under sustained load. Tune on-car if desired.
#define PERF_BATT_COLD_C 5
#define PERF_BATT_WARM_C 35
#define PERF_BATT_HOT_C  45

typedef enum {
    PERF_TEMP_COLD = -1,
    PERF_TEMP_NORMAL = 0,
    PERF_TEMP_WARM = 1,
    PERF_TEMP_HOT = 2,
} PerfTempBand;

// ── Performance state (per session) ─────────────────────────────────────────
// All-zero is the valid reset state (memset(0) in fsd_state_init). A time of 0
// means "not captured this run"; best_* of 0 means "no best yet this session".
typedef struct FSDPerf {
    // Acceleration run (auto-armed leaving standstill, reset at next standstill).
    bool accel_armed;
    uint32_t accel_start_ms;
    uint32_t t_0_50_ms, t_0_100_ms, t_0_60mph_ms; // current run, locked at target
    bool lock_0_50, lock_0_100, lock_0_60mph;
    uint32_t best_0_50_ms, best_0_100_ms, best_0_60mph_ms;

    // Braking run (100 -> 0 km/h).
    bool brake_armed;
    uint32_t brake_start_ms;
    uint32_t t_100_0_ms;
    bool lock_100_0;
    uint32_t best_100_0_ms;

    // Estimated longitudinal G from dv/dt of vehicle speed (NO real IMU).
    bool have_prev;
    float prev_speed_kph;
    uint32_t prev_ms;
    float g_est; // signed: + = accelerating, - = braking
    float g_peak; // rolling peak positive g (launch)
    float g_peak_brake; // rolling peak negative g (braking), stays <= 0

    bool any_speed; // at least one speed sample fed
} FSDPerf;

static inline void fsd_perf_reset(FSDPerf* p) {
    for(unsigned i = 0; i < sizeof(*p); i++)
        ((unsigned char*)p)[i] = 0;
}

// Feed one vehicle-speed sample (km/h, clamped >= 0) at now_ms. Runs the timer
// state machine and the dv/dt G estimate. Pure: mutates only *p.
static inline void fsd_perf_update(FSDPerf* p, float speed_kph, uint32_t now_ms) {
    bool had_prev = p->have_prev;
    float prev_speed = p->prev_speed_kph;
    uint32_t prev_ms = p->prev_ms;

    // --- estimated longitudinal G (dv/dt) ---
    if(had_prev) {
        uint32_t dt = now_ms - prev_ms;
        if(dt >= PERF_DT_MIN_MS && dt <= PERF_DT_MAX_MS) {
            float dv = (speed_kph - prev_speed) / 3.6f; // m/s
            float a = dv / ((float)dt / 1000.0f); // m/s^2
            p->g_est = a / PERF_G_MS2;
            if(p->g_est > p->g_peak) p->g_peak = p->g_est;
            if(p->g_est < p->g_peak_brake) p->g_peak_brake = p->g_est;
        }
    }

    bool standstill = speed_kph <= PERF_STANDSTILL_KPH;
    bool prev_standstill = had_prev && prev_speed <= PERF_STANDSTILL_KPH;

    // --- acceleration run state machine ---
    if(standstill) {
        p->accel_armed = false; // reset at standstill; next move re-arms
    } else if(prev_standstill) {
        // left standstill -> start a fresh run; clock from the standstill sample
        p->accel_armed = true;
        p->accel_start_ms = prev_ms;
        p->t_0_50_ms = p->t_0_100_ms = p->t_0_60mph_ms = 0;
        p->lock_0_50 = p->lock_0_100 = p->lock_0_60mph = false;
    }
    if(p->accel_armed) {
        uint32_t el = now_ms - p->accel_start_ms;
        if(!p->lock_0_50 && speed_kph >= PERF_T_50_KPH) {
            p->t_0_50_ms = el;
            p->lock_0_50 = true;
            if(!p->best_0_50_ms || el < p->best_0_50_ms) p->best_0_50_ms = el;
        }
        if(!p->lock_0_60mph && speed_kph >= PERF_T_60MPH_KPH) {
            p->t_0_60mph_ms = el;
            p->lock_0_60mph = true;
            if(!p->best_0_60mph_ms || el < p->best_0_60mph_ms) p->best_0_60mph_ms = el;
        }
        if(!p->lock_0_100 && speed_kph >= PERF_T_100_KPH) {
            p->t_0_100_ms = el;
            p->lock_0_100 = true;
            if(!p->best_0_100_ms || el < p->best_0_100_ms) p->best_0_100_ms = el;
        }
    }

    // --- braking run 100 -> 0 km/h ---
    if(!p->brake_armed) {
        if(had_prev && prev_speed >= PERF_BRAKE_FROM_KPH && speed_kph < PERF_BRAKE_FROM_KPH) {
            p->brake_armed = true;
            p->brake_start_ms = now_ms;
            p->t_100_0_ms = 0;
            p->lock_100_0 = false;
        }
    } else {
        if(standstill) {
            uint32_t el = now_ms - p->brake_start_ms;
            p->t_100_0_ms = el;
            p->lock_100_0 = true;
            if(!p->best_100_0_ms || el < p->best_100_0_ms) p->best_100_0_ms = el;
            p->brake_armed = false;
        } else if(had_prev && speed_kph > prev_speed + PERF_BRAKE_CANCEL_KPH) {
            p->brake_armed = false; // re-accelerated: not a clean stop
        }
    }

    p->prev_speed_kph = speed_kph;
    p->prev_ms = now_ms;
    p->have_prev = true;
    p->any_speed = true;
}

// Live elapsed (ms) of the current acceleration run, 0 when not armed.
static inline uint32_t fsd_perf_accel_running_ms(const FSDPerf* p, uint32_t now_ms) {
    return p->accel_armed ? (now_ms - p->accel_start_ms) : 0u;
}

// Wheel slip %: (rear avg - front avg) / front avg * 100. Positive = rear
// spinning faster (power oversteer / wheelspin). Undefined near standstill;
// returns false and leaves *out_pct untouched then.
static inline bool
    fsd_perf_wheel_slip_pct(float fl, float fr, float rl, float rr, float* out_pct) {
    float front = (fl + fr) * 0.5f;
    float rear = (rl + rr) * 0.5f;
    if(front < PERF_SLIP_MIN_KPH) return false;
    *out_pct = (rear - front) / front * 100.0f;
    return true;
}

// Temperature band for coloring. cold <= cold_c; hot >= hot_c; warm >= warm_c.
static inline int fsd_perf_temp_band(int c, int cold_c, int warm_c, int hot_c) {
    if(c <= cold_c) return PERF_TEMP_COLD;
    if(c >= hot_c) return PERF_TEMP_HOT;
    if(c >= warm_c) return PERF_TEMP_WARM;
    return PERF_TEMP_NORMAL;
}

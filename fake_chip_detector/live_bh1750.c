#include "live_bh1750.h"
#include "i2c_worker.h"

#include <furi.h>
#include <string.h>
#include <stdio.h>

// From the ROHM BH1750FVI technical note (No.10046ECT01, 2010.04 Rev.C).
//
// This part has no register index at all: a write is one bare opcode and a
// read is two bare bytes (page 10). It also has no ID register, so unlike the
// parts with a WHO_AM_I there is no way to confirm what answered before
// talking to it. What makes that acceptable here is that the only thing sent
// is a measurement trigger, not a configuration change, and One-Time mode puts
// the part back into power-down by itself once the reading is taken (page 5) —
// so there is nothing left switched on behind us either.
#define BH1750_CMD_ONE_TIME_H_RES 0x20

// Page 2 gives the H-resolution measurement time as 120 ms typical, 180 ms
// maximum, and the worked sequences on page 7 wait the maximum rather than the
// typical. Waiting the typical would sometimes read the previous measurement.
#define BH1750_MEASURE_MS 180

// Page 7 and page 10 both print the conversion as raw / 1.2, where 1.2 is the
// part's counts-per-lux gain. In integer terms that is raw * 5 / 6.
#define BH1750_LUX_NUM 5
#define BH1750_LUX_DEN 6

// The one hard, printed authenticity criterion in the whole document. Page 2
// specifies the dark output S0 at 0 lx as minimum 0, typical 0, maximum 3
// counts in H-resolution mode. A real BH1750 under a hand MUST fall to three
// counts or fewer; a part improvising numbers has no reason to.
#define BH1750_DARK_MAX_COUNTS 3

// The other half of the proof. Reading zero forever would satisfy the dark
// floor on its own, so the test also insists on having seen real light first —
// a stuck-at-zero part passes neither half. 30 counts is about 25 lx, well
// under any lit room and well over the documented dark maximum.
#define BH1750_LIGHT_MIN_COUNTS 30

#define BH1750_STEP_SAW_LIGHT 1
#define BH1750_STEP_SAW_DARK 2

static void bh1750_delay(const volatile bool* stop, uint32_t ms) {
    while(ms && !*stop) {
        uint32_t chunk = ms > 50 ? 50 : ms;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

// One One-Time H-resolution measurement. Trigger and read are separate
// transactions with a STOP between: page 10 says the part cannot accept
// several commands without one.
static bool bh1750_measure(uint8_t addr7, const volatile bool* stop, uint16_t* raw) {
    const uint8_t opcode = BH1750_CMD_ONE_TIME_H_RES;
    if(!i2c_worker_write_raw(addr7, &opcode, 1, I2C_REG_TIMEOUT_MS)) return false;

    bh1750_delay(stop, BH1750_MEASURE_MS);
    if(*stop) return false;

    uint8_t buf[2] = {0};
    if(!i2c_worker_read_raw(addr7, buf, sizeof(buf), I2C_REG_TIMEOUT_MS)) return false;

    *raw = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    return true;
}

static void
    bh1750_run(uint8_t addr7, const volatile bool* stop, LiveTestPublish publish, void* ctx) {
    while(!*stop) {
        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseStarting;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Waking the light sensor");
        publish(ctx, &st);

        uint8_t steps = 0;
        uint8_t errors = 0;

        while(!*stop && errors < 3) {
            uint16_t raw = 0;
            if(!bh1750_measure(addr7, stop, &raw)) {
                if(*stop) break;
                errors++;
                continue;
            }
            errors = 0;

            if(raw >= BH1750_LIGHT_MIN_COUNTS && steps < BH1750_STEP_SAW_LIGHT) {
                steps = BH1750_STEP_SAW_LIGHT;
            } else if(raw <= BH1750_DARK_MAX_COUNTS && steps >= BH1750_STEP_SAW_LIGHT) {
                steps = BH1750_STEP_SAW_DARK;
            }

            memset(&st, 0, sizeof(st));
            st.phase = (steps >= BH1750_STEP_SAW_DARK) ? LiveTestPhasePassed :
                                                         LiveTestPhaseRunning;
            st.progress = steps;
            st.progress_max = BH1750_STEP_SAW_DARK;
            st.value = (float)raw;
            snprintf(
                st.heading,
                sizeof(st.heading),
                "%lu",
                (unsigned long)((uint32_t)raw * BH1750_LUX_NUM / BH1750_LUX_DEN));
            snprintf(st.unit, sizeof(st.unit), "lx");

            if(steps >= BH1750_STEP_SAW_DARK) {
                snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Dark floor reached: real");
            } else if(steps >= BH1750_STEP_SAW_LIGHT) {
                snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Now cover it with a hand");
            } else {
                snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Hold it up to the light");
            }
            publish(ctx, &st);
        }

        if(*stop) break;

        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseLost;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "It replied, then stopped.");
        snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "Check 3V3 and the wires.");
        publish(ctx, &st);
        bh1750_delay(stop, 500);
    }
    // Nothing to tear down: One-Time mode powers the part down on its own once
    // the measurement is read (page 5).
}

const LiveTest live_test_bh1750 = {
    .chip = "BH1750",
    .title = "BH1750 test",
    .offer = "Cover it with your hand",
    .run = bh1750_run,
    .draw = NULL,
};

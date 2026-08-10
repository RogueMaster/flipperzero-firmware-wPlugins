#include "live_adxl345.h"
#include "i2c_worker.h"

#include <furi.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

// From the Analog Devices ADXL345 data sheet (Rev. E). This part is worth a
// live test more than most: its ID register is the only thing distinguishing
// it from a relabel, and a die that answers 0xE5 has told you nothing about
// whether the MEMS structure behind it actually moves.
#define ADXL345_REG_DEVID 0x00
#define ADXL345_REG_POWER_CTL 0x2D
#define ADXL345_REG_DATAX0 0x32

// Page 24: "a fixed device ID code of 0xE5".
#define ADXL345_DEVID_VALUE 0xE5

// Page 26: the part powers up in standby with every sensor function off, and
// bit 3 of POWER_CTL is what starts it measuring. That is the only write this
// test needs — the reset values already select +/-2 g, 10-bit, right-justified
// output at a 100 Hz data rate.
#define ADXL345_POWER_CTL_MEASURE 0x08
#define ADXL345_POWER_CTL_STANDBY 0x00

// Table 1, note 7: at the default 100 Hz data rate the turn-on time is about
// 11.1 ms.
#define ADXL345_TURNON_MS 12
#define ADXL345_POLL_MS 100

// Table 1: 256 LSB/g typical at the default +/-2 g, 10-bit setting.
#define ADXL345_LSB_PER_G 256

// Comfortably past the guaranteed +/-250 mg zero-g limit on Z, so a resting
// offset can never be mistaken for the axis carrying the board's weight.
#define ADXL345_DOMINANT_MG 700
#define ADXL345_PROOF_AXES 2

static void adxl345_delay(const volatile bool* stop, uint32_t ms) {
    while(ms && !*stop) {
        uint32_t chunk = ms > 40 ? 40 : ms;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

static bool adxl345_present(uint8_t addr7) {
    uint8_t id = 0;
    if(!i2c_worker_read_reg(addr7, ADXL345_REG_DEVID, &id, I2C_REG_TIMEOUT_MS)) return false;
    return id == ADXL345_DEVID_VALUE;
}

static bool adxl345_read_accel(uint8_t addr7, int32_t out_mg[3]) {
    uint8_t buf[6] = {0};
    if(!i2c_worker_read_mem(addr7, ADXL345_REG_DATAX0, buf, sizeof(buf), I2C_REG_TIMEOUT_MS))
        return false;

    for(uint8_t axis = 0; axis < 3; axis++) {
        // Little-endian: DATAx0 holds the low byte. This is the opposite of
        // the MPU parts, and the output is already sign-extended two's
        // complement, so the pair casts straight to int16 with no masking —
        // masking the top bits here would destroy the sign of a negative g.
        int16_t raw = (int16_t)(((uint16_t)buf[axis * 2 + 1] << 8) | buf[axis * 2]);
        out_mg[axis] = (int32_t)raw * 1000 / ADXL345_LSB_PER_G;
    }
    return true;
}

static void
    adxl345_run(uint8_t addr7, const volatile bool* stop, LiveTestPublish publish, void* ctx) {
    while(!*stop) {
        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseStarting;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Leaving standby");
        publish(ctx, &st);

        bool measuring = false;
        if(adxl345_present(addr7)) {
            measuring = i2c_worker_write_reg(
                addr7, ADXL345_REG_POWER_CTL, ADXL345_POWER_CTL_MEASURE, I2C_REG_TIMEOUT_MS);
            if(measuring) adxl345_delay(stop, ADXL345_TURNON_MS);
        }

        uint8_t axes_seen = 0;
        uint8_t errors = 0;

        while(measuring && !*stop && errors < 3) {
            int32_t mg[3] = {0};
            if(!adxl345_read_accel(addr7, mg)) {
                errors++;
                adxl345_delay(stop, ADXL345_POLL_MS);
                continue;
            }
            errors = 0;

            uint8_t dominant = 0;
            int32_t best = 0;
            for(uint8_t axis = 0; axis < 3; axis++) {
                int32_t magnitude = mg[axis] < 0 ? -mg[axis] : mg[axis];
                if(magnitude > best) {
                    best = magnitude;
                    dominant = axis;
                }
            }
            if(best >= ADXL345_DOMINANT_MG) axes_seen |= (uint8_t)(1u << dominant);

            uint8_t count = 0;
            for(uint8_t axis = 0; axis < 3; axis++) {
                if(axes_seen & (1u << axis)) count++;
            }

            float total = sqrtf(
                (float)mg[0] * mg[0] + (float)mg[1] * mg[1] + (float)mg[2] * mg[2]);

            memset(&st, 0, sizeof(st));
            st.phase = (count >= ADXL345_PROOF_AXES) ? LiveTestPhasePassed :
                                                       LiveTestPhaseRunning;
            st.value = total / 1000.0f;
            st.progress = count;
            st.progress_max = ADXL345_PROOF_AXES;
            snprintf(
                st.heading,
                sizeof(st.heading),
                "%u.%02u",
                (unsigned)((uint32_t)total / 1000u),
                (unsigned)((uint32_t)total % 1000u / 10u));
            snprintf(st.unit, sizeof(st.unit), "g");

            if(count >= ADXL345_PROOF_AXES) {
                snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Gravity followed the tilt");
            } else if(best >= ADXL345_DOMINANT_MG) {
                snprintf(
                    st.lines[0], LIVE_TEST_LINE_LEN, "Gravity on %c - tip it", 'X' + dominant);
            } else {
                snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Hold it still and flat");
            }
            publish(ctx, &st);

            adxl345_delay(stop, ADXL345_POLL_MS);
        }

        // Park only what we started. Standby is where the part powers up, so
        // this puts it back exactly as it was found. Nothing is written if the
        // device ID never matched.
        if(measuring) {
            i2c_worker_write_reg(
                addr7, ADXL345_REG_POWER_CTL, ADXL345_POWER_CTL_STANDBY, I2C_REG_TIMEOUT_MS);
        }

        if(*stop) break;

        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseLost;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "It replied, then stopped.");
        snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "Check 3V3 and the wires.");
        publish(ctx, &st);
        adxl345_delay(stop, 500);
    }
}

const LiveTest live_test_adxl345 = {
    .chip = "ADXL345/343",
    .title = "ADXL345 test",
    .offer = "Tip it and watch gravity",
    .run = adxl345_run,
    .draw = NULL,
};

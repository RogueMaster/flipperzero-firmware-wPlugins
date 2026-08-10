#include "live_aht.h"

#include <furi.h>
#include <string.h>
#include <stdio.h>

// From the ASAIR/Aosong AHT10 datasheet (V1.2) and AHT20 product manual (V1.0),
// section 5.4 in both.
//
// One test covers both parts because the database cannot tell them apart —
// they share address 0x38 and neither has an ID register. Happily the only
// command this test sends is identical on both: trigger measurement is
// 0xAC 0x33 0x00 either way.
//
// What is NOT identical is initialisation: the AHT10 uses opcode 0xE1 and the
// AHT20 uses 0xBE, and sending one to the other is undefined. So this test
// never initialises. It does not need to — a working part leaves the factory
// with its calibration bit already set, and the status byte says so. If that
// bit is clear the test reports it rather than guessing which opcode to send
// into a part it cannot identify.
#define AHT_CMD_TRIGGER_0 0xAC
#define AHT_CMD_TRIGGER_1 0x33
#define AHT_CMD_TRIGGER_2 0x00

// Status byte, table 10: bit 7 busy, bit 3 calibration enabled.
#define AHT_STATUS_BUSY 0x80
#define AHT_STATUS_CALIBRATED 0x08

// Section 5.4 gives 80 ms for a measurement, after which the busy bit should
// have cleared.
#define AHT_MEASURE_MS 80
#define AHT_POLL_MS 120

// Six bytes: status, then twenty bits of humidity and twenty of temperature
// sharing the middle byte. The AHT20 appends a seventh CRC byte the AHT10 does
// not document, so only six are read — legal on both, since the master simply
// stops early.
#define AHT_READ_LEN 6

// Humidity is a fraction of 2^20 scaled to 100%, temperature the same scaled to
// 200 and offset by -50 (sections 6.1 and 6.2). Dropping the low four bits
// first keeps all of this inside 32-bit arithmetic; it costs a hundredth of a
// percent, far below the part's +/-2% accuracy.
#define AHT_RAW_SHIFT 4
#define AHT_RAW_BITS 16

// Exhaled breath is near saturation and drives a nearby sensor from a normal
// indoor 30-50% up towards 90%. Fifteen points is a swing room air cannot
// produce on its own but a single breath clears easily.
#define AHT_PROOF_RISE_CENTI 1500

static void aht_delay(const volatile bool* stop, uint32_t ms) {
    while(ms && !*stop) {
        uint32_t chunk = ms > 40 ? 40 : ms;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

typedef enum {
    AhtReadOk,
    AhtReadNoAnswer,
    AhtReadBusy, // still measuring after the documented wait
    AhtReadUncalibrated, // the part says its own calibration is not loaded
} AhtReadResult;

static AhtReadResult aht_measure(
    const LiveTestI2c* i2c,
    uint8_t addr7,
    const volatile bool* stop,
    int32_t* rh_centi,
    int32_t* t_centi) {
    const uint8_t trigger[3] = {AHT_CMD_TRIGGER_0, AHT_CMD_TRIGGER_1, AHT_CMD_TRIGGER_2};
    if(!i2c->write_raw(addr7, trigger, sizeof(trigger), LIVE_TEST_TIMEOUT_MS))
        return AhtReadNoAnswer;

    aht_delay(stop, AHT_MEASURE_MS);
    if(*stop) return AhtReadNoAnswer;

    uint8_t buf[AHT_READ_LEN] = {0};
    if(!i2c->read_raw(addr7, buf, sizeof(buf), LIVE_TEST_TIMEOUT_MS)) return AhtReadNoAnswer;

    if(buf[0] & AHT_STATUS_BUSY) return AhtReadBusy;
    if(!(buf[0] & AHT_STATUS_CALIBRATED)) return AhtReadUncalibrated;

    uint32_t raw_rh = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | (buf[3] >> 4);
    uint32_t raw_t = ((uint32_t)(buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | buf[5];

    *rh_centi = (int32_t)(((raw_rh >> AHT_RAW_SHIFT) * 10000u) >> AHT_RAW_BITS);
    *t_centi = (int32_t)(((raw_t >> AHT_RAW_SHIFT) * 20000u) >> AHT_RAW_BITS) - 5000;
    return AhtReadOk;
}

// Hundredths to one decimal place. The magnitude is split off first so a value
// between 0 and -1 keeps its sign, which plain integer division would drop,
// and clamped so the widest possible result is "-999.9".
static void aht_format(char* out, size_t len, int32_t centi) {
    bool negative = centi < 0;
    uint32_t magnitude = (uint32_t)(negative ? -centi : centi);
    if(magnitude > 99999u) magnitude = 99999u;
    snprintf(
        out,
        len,
        "%s%u.%u",
        negative ? "-" : "",
        (unsigned)(magnitude / 100u),
        (unsigned)(magnitude % 100u / 10u));
}

static void aht_run(const LiveTestEnv* env) {
    const uint8_t addr7 = env->addr7;
    const volatile bool* stop = env->stop;
    const LiveTestI2c* i2c = env->i2c;
    const LiveTestPublish publish = env->publish;
    void* const ctx = env->ctx;

    while(!*stop) {
        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseStarting;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Asking for a reading");
        publish(ctx, &st);

        int32_t baseline = INT32_MAX;
        uint8_t errors = 0;

        while(!*stop && errors < 3) {
            int32_t rh = 0, temp = 0;
            AhtReadResult result = aht_measure(i2c, addr7, stop, &rh, &temp);
            if(*stop) break;
            if(result == AhtReadNoAnswer) {
                errors++;
                aht_delay(stop, AHT_POLL_MS);
                continue;
            }
            errors = 0;

            memset(&st, 0, sizeof(st));
            st.phase = LiveTestPhaseRunning;

            if(result != AhtReadOk) {
                snprintf(st.heading, sizeof(st.heading), "--");
                snprintf(
                    st.lines[0],
                    LIVE_TEST_LINE_LEN,
                    "%s",
                    result == AhtReadBusy ? "Still busy measuring" :
                                            "It reports no calibration");
                publish(ctx, &st);
                aht_delay(stop, AHT_POLL_MS);
                continue;
            }

            // The lowest humidity seen is the room; everything is measured as a
            // rise above it. Taking the running minimum rather than the first
            // sample means a test started with the sensor already breathed on
            // still finds its baseline as the reading settles back down.
            if(rh < baseline) baseline = rh;

            aht_format(st.heading, sizeof(st.heading), rh);
            snprintf(st.unit, sizeof(st.unit), "%%RH");
            st.value = (float)rh / 100.0f;
            st.bar = (uint8_t)(rh < 0 ? 0 : (rh > 10000 ? 100 : rh / 100));
            st.bar_max = 100;

            // Eight bytes covers the widest the formatter can produce and
            // keeps the composed line provably inside 26 characters.
            char temp_str[8];
            aht_format(temp_str, sizeof(temp_str), temp);
            if(rh - baseline >= AHT_PROOF_RISE_CENTI) {
                st.phase = LiveTestPhasePassed;
                snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Your breath moved it");
            } else {
                snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "%s C - breathe on it", temp_str);
            }
            publish(ctx, &st);

            aht_delay(stop, AHT_POLL_MS);
        }

        if(*stop) break;

        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseLost;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "It replied, then stopped.");
        snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "Check 3V3 and the wires.");
        publish(ctx, &st);
        aht_delay(stop, 500);
    }
    // Nothing to tear down: the part idles between single measurements and no
    // mode was ever changed.
}

const LiveTest live_test_aht = {
    .chip = "AHT10/AHT20",
    .title = "AHT10/20 test",
    .offer = "Breathe on it",
    .addrs = {0x38},
    .run = aht_run,
    .draw = NULL,
};

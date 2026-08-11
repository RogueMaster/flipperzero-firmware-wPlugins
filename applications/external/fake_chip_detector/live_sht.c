#include "live_sht.h"

#include <furi.h>
#include <string.h>
#include <stdio.h>

// From the Sensirion SHT3x-DIS datasheet (version 7, December 2022) and the
// SHT4x datasheet (document D1, version 7.1, March 2025).
//
// These two families share addresses 0x44/0x45 and neither has an ID register,
// so the database lists them as one entry and this test has to cope with
// either. They are genuinely different parts on the wire: an SHT4x takes
// single-byte commands, an SHT3x takes sixteen-bit ones, and the opcodes are
// unrelated. Worse, 0xE0 means "measure, low precision" on an SHT4x and is the
// first half of SHT3x's fetch-data command.
//
// What rescues this is the checksum. Both families protect every two data
// bytes with the same CRC-8, so the test simply asks an SHT4x question first
// and believes the answer only if the CRC verifies. If it does not, it asks
// the SHT3x question instead. Nothing is ever decoded on a guess.
#define SHT4X_CMD_MEASURE_HIGH     0xFD // page 12, table 8
#define SHT3X_CMD_MEASURE_HIGH_MSB 0x2C // page 10, table 9: 0x2C06,
#define SHT3X_CMD_MEASURE_HIGH_LSB 0x06 // high repeatability, clock stretching

// SHT4x high-precision needs 8.3 ms worst case (page 10) and its own quick
// start pseudo-code waits 10. SHT3x high repeatability needs 15 ms (page 7).
#define SHT4X_MEASURE_MS 10
#define SHT3X_MEASURE_MS 15
#define SHT_POLL_MS      120

// Six bytes both ways: temperature, its CRC, humidity, its CRC. Temperature
// comes first on both parts (SHT3x page 10, SHT4x page 11).
#define SHT_READ_LEN 6

// CRC-8, polynomial 0x31, initialised to 0xFF, no reflection, no final xor,
// covering the two bytes before it. Identical in both datasheets, and both
// print the same test vector: CRC(0xBEEF) = 0x92, which is checked once at
// startup so a broken implementation can never silently reject a good part.
#define SHT_CRC_POLY 0x31
#define SHT_CRC_INIT 0xFF

// Conversion, both datasheets. Temperature is the same equation on both parts;
// humidity is not, which is exactly why knowing the variant matters.
//   T  = -45 + 175 * S / 65535            (SHT3x page 14, SHT4x page 12)
//   RH = 100 * S / 65535                  (SHT3x page 14)
//   RH = -6 + 125 * S / 65535, clamped    (SHT4x page 12 and its note on 13)
#define SHT_T_SPAN_CENTI      17500
#define SHT_T_OFFSET_CENTI    4500
#define SHT3X_RH_SPAN_CENTI   10000
#define SHT4X_RH_SPAN_CENTI   12500
#define SHT4X_RH_OFFSET_CENTI 600
#define SHT_FULL_SCALE        65535

// Exhaled breath is near saturation; indoor air sits at 30-50%. Fifteen points
// is beyond anything room air does by itself and beyond the part's own +/-2%.
#define SHT_PROOF_RISE_CENTI 1500

typedef enum {
    ShtVariantUnknown,
    ShtVariant4x,
    ShtVariant3x,
} ShtVariant;

static void sht_delay(const volatile bool* stop, uint32_t ms) {
    while(ms && !*stop) {
        uint32_t chunk = ms > 40 ? 40 : ms;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

static uint8_t sht_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = SHT_CRC_INIT;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ SHT_CRC_POLY) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

// Both datasheets publish CRC(0xBEEF) = 0x92 as a self-test vector. Checking it
// costs nothing and rules out the one failure mode that would make this test
// accuse every genuine sensor it meets.
static bool sht_crc_self_test(void) {
    const uint8_t vector[2] = {0xBE, 0xEF};
    return sht_crc8(vector, sizeof(vector)) == 0x92;
}

// Reads one measurement in the given dialect. Returns false if the part did
// not answer or the checksums did not verify — from the caller's point of view
// those are the same thing: no number worth showing.
static bool sht_try(
    const LiveTestI2c* i2c,
    uint8_t addr7,
    const volatile bool* stop,
    ShtVariant variant,
    int32_t* rh_centi,
    int32_t* t_centi) {
    if(variant == ShtVariant4x) {
        const uint8_t cmd = SHT4X_CMD_MEASURE_HIGH;
        if(!i2c->write_raw(addr7, &cmd, 1, LIVE_TEST_TIMEOUT_MS)) return false;
        sht_delay(stop, SHT4X_MEASURE_MS);
    } else {
        const uint8_t cmd[2] = {SHT3X_CMD_MEASURE_HIGH_MSB, SHT3X_CMD_MEASURE_HIGH_LSB};
        if(!i2c->write_raw(addr7, cmd, sizeof(cmd), LIVE_TEST_TIMEOUT_MS)) return false;
        sht_delay(stop, SHT3X_MEASURE_MS);
    }
    if(*stop) return false;

    uint8_t buf[SHT_READ_LEN] = {0};
    if(!i2c->read_raw(addr7, buf, sizeof(buf), LIVE_TEST_TIMEOUT_MS)) return false;
    if(sht_crc8(&buf[0], 2) != buf[2]) return false;
    if(sht_crc8(&buf[3], 2) != buf[5]) return false;

    uint32_t raw_t = ((uint32_t)buf[0] << 8) | buf[1];
    uint32_t raw_rh = ((uint32_t)buf[3] << 8) | buf[4];

    *t_centi = (int32_t)(raw_t * SHT_T_SPAN_CENTI / SHT_FULL_SCALE) - SHT_T_OFFSET_CENTI;

    if(variant == ShtVariant4x) {
        int32_t rh =
            (int32_t)(raw_rh * SHT4X_RH_SPAN_CENTI / SHT_FULL_SCALE) - SHT4X_RH_OFFSET_CENTI;
        // The SHT4x equation can legitimately run past both ends of the scale;
        // page 13 asks for it to be cropped rather than shown as -3% humidity.
        *rh_centi = rh < 0 ? 0 : (rh > 10000 ? 10000 : rh);
    } else {
        *rh_centi = (int32_t)(raw_rh * SHT3X_RH_SPAN_CENTI / SHT_FULL_SCALE);
    }
    return true;
}

// Hundredths to one decimal place. The magnitude is split off first so a value
// between 0 and -1 keeps its sign, which plain integer division would drop,
// and clamped so the widest possible result is "-999.9".
static void sht_format(char* out, size_t len, int32_t centi) {
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

static void sht_run(const LiveTestEnv* env) {
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

        if(!sht_crc_self_test()) {
            // Refuse to judge anything rather than risk calling a good sensor
            // a fake because our own checksum is wrong.
            memset(&st, 0, sizeof(st));
            st.phase = LiveTestPhaseLost;
            snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Internal CRC self-test");
            snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "failed - not judging.");
            publish(ctx, &st);
            sht_delay(stop, 1000);
            continue;
        }

        ShtVariant variant = ShtVariantUnknown;
        int32_t baseline = INT32_MAX;
        uint8_t errors = 0;

        while(!*stop && errors < 3) {
            int32_t rh = 0, temp = 0;
            bool ok = false;

            if(variant != ShtVariantUnknown) {
                ok = sht_try(i2c, addr7, stop, variant, &rh, &temp);
                if(!ok) variant = ShtVariantUnknown; // it changed its mind; re-probe
            }
            if(!ok && !*stop) {
                if(sht_try(i2c, addr7, stop, ShtVariant4x, &rh, &temp)) {
                    variant = ShtVariant4x;
                    ok = true;
                } else if(!*stop && sht_try(i2c, addr7, stop, ShtVariant3x, &rh, &temp)) {
                    variant = ShtVariant3x;
                    ok = true;
                }
            }
            if(*stop) break;

            if(!ok) {
                errors++;
                sht_delay(stop, SHT_POLL_MS);
                continue;
            }
            errors = 0;

            if(rh < baseline) baseline = rh;

            memset(&st, 0, sizeof(st));
            st.phase = LiveTestPhaseRunning;
            sht_format(st.heading, sizeof(st.heading), rh);
            snprintf(st.unit, sizeof(st.unit), "%%RH");
            st.value = (float)rh / 100.0f;
            st.bar = (uint8_t)(rh < 0 ? 0 : (rh > 10000 ? 100 : rh / 100));
            st.bar_max = 100;

            // Eight bytes covers the widest the formatter can produce and
            // keeps the composed line provably inside 26 characters.
            char temp_str[8];
            sht_format(temp_str, sizeof(temp_str), temp);
            if(rh - baseline >= SHT_PROOF_RISE_CENTI) {
                st.phase = LiveTestPhasePassed;
                snprintf(
                    st.lines[0],
                    LIVE_TEST_LINE_LEN,
                    "%s felt your breath",
                    variant == ShtVariant4x ? "SHT4x" : "SHT3x");
            } else {
                snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "%s C - breathe on it", temp_str);
            }
            publish(ctx, &st);

            sht_delay(stop, SHT_POLL_MS);
        }

        if(*stop) break;

        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseLost;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "It replied, then stopped.");
        snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "Check 3V3 and the wires.");
        publish(ctx, &st);
        sht_delay(stop, LIVE_TEST_RETRY_MS);
    }
    // Nothing to tear down: single-shot measurements leave the part idle.
}

const LiveTest live_test_sht = {
    .chip = "SHT3x/SHT4x",
    .title = "SHT3x/4x test",
    .offer = "Breathe on it",
    .addrs = {0x44, 0x45},
    .run = sht_run,
    .draw = NULL,
};

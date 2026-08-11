#include "live_mlx90614.h"

#include <furi.h>
#include <string.h>
#include <stdio.h>

// From the Melexis MLX90614 datasheet (DOC#3901090614, datasheet rev 021),
// section 4.1.4 and the memory map in Table 14 on page 16.
//
// The command byte's top three bits pick the memory space: 000 is RAM
// (Table 15, page 19), so a RAM address doubles as its own command byte. RAM
// is read-only, which is the whole reason this test is safe to run on a part
// that might not be an MLX90614 at all: it only ever reads.
#define MLX90614_RAM_TA    0x06
#define MLX90614_RAM_TOBJ1 0x07

// A read returns three bytes: data low, data high, then the packet error code
// (page 17). The datasheet only ever documents the master reading all three —
// permission to stop early exists for the Read Flags command and nothing else
// (note on page 19) — so all three are always read.
#define MLX90614_READ_LEN 3

// CRC-8, polynomial X^8+X^2+X^1+1, over every byte on the wire including both
// address bytes (page 18). Verified against the datasheet's own worked
// example on page 20: CRC8(B4 07 B5 D2 3A) == 0x30.
#define MLX90614_PEC_POLY 0x07

// Bit 15 of a linearised temperature is an error flag, active high (page 19).
// Any value carrying it is one the sensor itself disowns.
#define MLX90614_ERROR_FLAG 0x8000

// The documented valid window for TOBJ1, from page 19: -70.01 C to +382.19 C.
#define MLX90614_TOBJ_RAW_MIN 0x27AD
#define MLX90614_TOBJ_RAW_MAX 0x7FFF

// Page 30: divide by 50 for kelvin, then subtract 273.15. In hundredths of a
// degree that is exactly raw*2 - 27315, with no floating point and no rounding
// error. Worked example from the datasheet: 0x3AF7 -> 2875 -> 28.75 C.
#define MLX90614_CENTI_PER_LSB       2
#define MLX90614_KELVIN_OFFSET_CENTI 27315

// Page 22 warns that back-to-back reads couple SCL noise into the sensor and
// corrupt the reading, and asks for the lines to be quiet for longer than the
// refresh and settling times. Those are per-variant and unreadable over the
// bus; half a second clears every factory default except the 1.33 s optics.
#define MLX90614_POLL_MS 500

// Skin runs several degrees over room temperature indoors. Five is comfortably
// above sensor noise and ambient drift while still being reached by a cold
// hand, which matters when the test is run in a shop doorway in winter.
#define MLX90614_PROOF_DELTA_CENTI 500
#define MLX90614_BAR_MAX_C         10

static void mlx90614_delay(const volatile bool* stop, uint32_t ms) {
    while(ms && !*stop) {
        uint32_t chunk = ms > 50 ? 50 : ms;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

static uint8_t mlx90614_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ MLX90614_PEC_POLY) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

typedef enum {
    MlxReadOk,
    MlxReadNoAnswer, // the part stopped talking
    MlxReadBadPec, // it answered, but not with its own checksum
    MlxReadDisowned, // error flag set, or outside the documented range
} MlxReadResult;

// Reads one RAM word and checks the sensor's own packet error code before
// believing a single bit of it. For a part with no ID register that checksum
// is the closest thing to proof of identity there is: it covers the address
// bytes as well as the data, so a device that merely parks two plausible bytes
// at this address will not reproduce it.
static MlxReadResult
    mlx90614_read_ram(const LiveTestI2c* i2c, uint8_t addr7, uint8_t ram_addr, int32_t* centi_c) {
    uint8_t buf[MLX90614_READ_LEN] = {0};
    if(!i2c->read_mem(addr7, ram_addr, buf, sizeof(buf), LIVE_TEST_TIMEOUT_MS))
        return MlxReadNoAnswer;

    const uint8_t framed[] = {
        (uint8_t)(addr7 << 1), ram_addr, (uint8_t)((addr7 << 1) | 1), buf[0], buf[1]};
    if(mlx90614_crc8(framed, sizeof(framed)) != buf[2]) return MlxReadBadPec;

    uint16_t raw = (uint16_t)(((uint16_t)buf[1] << 8) | buf[0]);
    if(raw & MLX90614_ERROR_FLAG) return MlxReadDisowned;
    if(raw < MLX90614_TOBJ_RAW_MIN || raw > MLX90614_TOBJ_RAW_MAX) return MlxReadDisowned;

    *centi_c = (int32_t)raw * MLX90614_CENTI_PER_LSB - MLX90614_KELVIN_OFFSET_CENTI;
    return MlxReadOk;
}

// Hundredths of a degree to one decimal place. The magnitude is split off
// first so a value between 0 and -1 C keeps its sign, which plain integer
// division would drop, and clamped so the widest possible result is "-999.9".
static void mlx90614_format(char* out, size_t len, int32_t centi) {
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

static void mlx90614_run(const LiveTestEnv* env) {
    const uint8_t addr7 = env->addr7;
    const volatile bool* stop = env->stop;
    const LiveTestI2c* i2c = env->i2c;
    const LiveTestPublish publish = env->publish;
    void* const ctx = env->ctx;

    while(!*stop) {
        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseStarting;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Waking the thermopile");
        publish(ctx, &st);

        uint8_t errors = 0;
        while(!*stop && errors < 3) {
            int32_t object = 0, ambient = 0;
            MlxReadResult obj_result = mlx90614_read_ram(i2c, addr7, MLX90614_RAM_TOBJ1, &object);
            MlxReadResult amb_result =
                (obj_result == MlxReadOk) ?
                    mlx90614_read_ram(i2c, addr7, MLX90614_RAM_TA, &ambient) :
                    obj_result;

            if(obj_result == MlxReadNoAnswer || amb_result == MlxReadNoAnswer) {
                errors++;
                mlx90614_delay(stop, MLX90614_POLL_MS);
                continue;
            }
            errors = 0;

            memset(&st, 0, sizeof(st));
            st.phase = LiveTestPhaseRunning;

            if(obj_result != MlxReadOk || amb_result != MlxReadOk) {
                // It said something, but nothing it will stand behind. Report
                // which of the two it was rather than drawing a temperature.
                MlxReadResult bad = (obj_result != MlxReadOk) ? obj_result : amb_result;
                snprintf(st.heading, sizeof(st.heading), "--");
                snprintf(
                    st.lines[0],
                    LIVE_TEST_LINE_LEN,
                    "%s",
                    bad == MlxReadBadPec ? "Checksum does not match" : "Sensor flagged an error");
                publish(ctx, &st);
                mlx90614_delay(stop, MLX90614_POLL_MS);
                continue;
            }

            int32_t delta = object - ambient;
            mlx90614_format(st.heading, sizeof(st.heading), object);
            snprintf(st.unit, sizeof(st.unit), "C");

            int32_t bar_c = delta / 100;
            st.value = (float)object / 100.0f;
            st.bar = (uint8_t)(bar_c < 0 ?
                                   0 :
                                   (bar_c > MLX90614_BAR_MAX_C ? MLX90614_BAR_MAX_C : bar_c));
            st.bar_max = MLX90614_BAR_MAX_C;

            // Eight bytes holds the widest the formatter can produce, and
            // keeps the composed lines provably inside 26 characters.
            char room[8];
            mlx90614_format(room, sizeof(room), ambient);
            if(delta >= MLX90614_PROOF_DELTA_CENTI) {
                char diff[8];
                mlx90614_format(diff, sizeof(diff), delta);
                st.phase = LiveTestPhasePassed;
                snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "%s C over the room", diff);
            } else {
                snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Aim closer - room %s", room);
            }
            publish(ctx, &st);

            mlx90614_delay(stop, MLX90614_POLL_MS);
        }

        if(*stop) break;

        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseLost;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "It replied, then stopped.");
        snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "Check 3V3 and the wires.");
        publish(ctx, &st);
        mlx90614_delay(stop, LIVE_TEST_RETRY_MS);
    }
    // Nothing to tear down: RAM is read-only and no mode was ever changed.
}

const LiveTest live_test_mlx90614 = {
    .chip = "MLX90614",
    .title = "MLX90614 test",
    .offer = "Point it at your hand",
    .addrs = {0x5A},
    .run = mlx90614_run,
    .draw = NULL,
};

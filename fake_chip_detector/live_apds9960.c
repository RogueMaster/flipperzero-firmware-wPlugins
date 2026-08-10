#include "live_apds9960.h"
#include "i2c_worker.h"

#include <furi.h>
#include <string.h>
#include <stdio.h>

// From the Broadcom APDS-9960 datasheet (AV02-4191EN, 13 November 2015).
// Register addresses in the map on page 19 already carry the command bit, so
// they go on the wire exactly as printed.
#define APDS9960_REG_ENABLE 0x80
#define APDS9960_REG_PPULSE 0x8E
#define APDS9960_REG_CONTROL 0x8F
#define APDS9960_REG_ID 0x92
#define APDS9960_REG_STATUS 0x93
#define APDS9960_REG_PDATA 0x9C

// Page 25 lists exactly one part number, 0xAB. Values circulating in hobby
// libraries for other lots do not appear in the Broadcom document, so they are
// not accepted here — this app would rather say nothing than guess.
#define APDS9960_ID_VALUE 0xAB

// ENABLE bits, page 20: bit 0 PON powers the oscillator, bit 2 PEN starts the
// proximity engine. Those two are the only writes strictly required.
#define APDS9960_ENABLE_PROX 0x05
#define APDS9960_ENABLE_OFF 0x00

// Not required to make it run, but required for the number to mean anything.
// The proximity counts quoted on pages 4 and 5 are measured at eight 8 us
// pulses and 4x gain, which is NOT the reset state (one pulse, 1x gain). Left
// at defaults the part works but reads far lower than any published figure,
// and the test would be judging a genuine sensor against a spec it was never
// characterised under.
#define APDS9960_PPULSE_8_AT_8US 0x47 // PPLEN=8us, 7+1 = 8 pulses (page 23)
#define APDS9960_CONTROL_100MA_4X 0x08 // LDRIVE 100 mA, PGAIN 4x (page 24)

// The reset values of those same two registers, from the map on page 19: one
// 8 us pulse, 100 mA, 1x gain. Both are restored on the way out, because a
// live test has no business leaving the next program's sensor eight times more
// sensitive than the part it thinks it is holding.
#define APDS9960_PPULSE_RESET 0x40
#define APDS9960_CONTROL_RESET 0x00

// STATUS bits, page 25. PVALID says a proximity cycle finished and is cleared
// by the act of reading PDATA; PGSAT says the analog front end saturated and
// the datasheet warns the result "may not be accurate".
#define APDS9960_STATUS_PVALID 0x02
#define APDS9960_STATUS_PGSAT 0x40

// Page 9: 7 ms to leave sleep, plus about 1.2 ms for a conversion.
#define APDS9960_SETTLE_MS 10
#define APDS9960_POLL_MS 60
#define APDS9960_VALID_TIMEOUT_MS 120

// No object reads typically 10 counts and at most 25 (page 4); a target at
// 100 mm reads 96 to 144 (page 5). A rise of 50 counts is therefore far above
// the empty-air ceiling while sitting well inside the weakest specified
// response, and using the rise rather than an absolute value sidesteps the
// part-to-part crosstalk offset the datasheet explicitly allows for.
#define APDS9960_PROOF_RISE 50

static void apds9960_delay(const volatile bool* stop, uint32_t ms) {
    while(ms && !*stop) {
        uint32_t chunk = ms > 25 ? 25 : ms;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

static bool apds9960_present(uint8_t addr7) {
    uint8_t id = 0;
    if(!i2c_worker_read_reg(addr7, APDS9960_REG_ID, &id, I2C_REG_TIMEOUT_MS)) return false;
    return id == APDS9960_ID_VALUE;
}

// Which registers this test actually managed to change. A write can fail
// halfway through the sequence, and the cleanup at the end has to undo exactly
// the ones that landed - no more, and no fewer.
typedef struct {
    bool ppulse;
    bool control;
    bool enable;
} ApdsTouched;

// Page 20 is explicit that every control register must be set before the
// engine is enabled: "changing control register values while operating may
// result in invalid results". So ENABLE is written last, deliberately.
static bool apds9960_start(uint8_t addr7, const volatile bool* stop, ApdsTouched* touched) {
    touched->ppulse = i2c_worker_write_reg(
        addr7, APDS9960_REG_PPULSE, APDS9960_PPULSE_8_AT_8US, I2C_REG_TIMEOUT_MS);
    if(!touched->ppulse) return false;

    touched->control = i2c_worker_write_reg(
        addr7, APDS9960_REG_CONTROL, APDS9960_CONTROL_100MA_4X, I2C_REG_TIMEOUT_MS);
    if(!touched->control) return false;

    touched->enable = i2c_worker_write_reg(
        addr7, APDS9960_REG_ENABLE, APDS9960_ENABLE_PROX, I2C_REG_TIMEOUT_MS);
    if(!touched->enable) return false;

    apds9960_delay(stop, APDS9960_SETTLE_MS);
    return true;
}

typedef enum {
    ApdsReadOk,
    ApdsReadNoAnswer,
    ApdsReadSaturated,
    ApdsReadNotReady,
} ApdsReadResult;

static ApdsReadResult
    apds9960_read(uint8_t addr7, const volatile bool* stop, uint8_t* pdata) {
    uint8_t status = 0;
    uint32_t waited = 0;
    for(;;) {
        if(*stop) return ApdsReadNotReady;
        if(!i2c_worker_read_reg(addr7, APDS9960_REG_STATUS, &status, I2C_REG_TIMEOUT_MS))
            return ApdsReadNoAnswer;
        if(status & APDS9960_STATUS_PVALID) break;
        if(waited >= APDS9960_VALID_TIMEOUT_MS) return ApdsReadNotReady;
        furi_delay_ms(5);
        waited += 5;
    }

    if(status & APDS9960_STATUS_PGSAT) return ApdsReadSaturated;
    // Reading PDATA is what clears PVALID (page 11), so this read both fetches
    // the sample and arms the next one.
    if(!i2c_worker_read_reg(addr7, APDS9960_REG_PDATA, pdata, I2C_REG_TIMEOUT_MS))
        return ApdsReadNoAnswer;
    return ApdsReadOk;
}

static void
    apds9960_run(uint8_t addr7, const volatile bool* stop, LiveTestPublish publish, void* ctx) {
    while(!*stop) {
        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseStarting;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Lighting the IR LED");
        publish(ctx, &st);

        ApdsTouched touched = {0};
        bool running = false;
        if(apds9960_present(addr7)) running = apds9960_start(addr7, stop, &touched);

        uint16_t seen_min = 0xFFFF, seen_max = 0;
        uint8_t errors = 0;

        while(running && !*stop && errors < 3) {
            uint8_t pdata = 0;
            ApdsReadResult result = apds9960_read(addr7, stop, &pdata);
            if(result == ApdsReadNoAnswer) {
                errors++;
                apds9960_delay(stop, APDS9960_POLL_MS);
                continue;
            }
            if(*stop) break;
            errors = 0;

            memset(&st, 0, sizeof(st));
            st.phase = LiveTestPhaseRunning;

            if(result != ApdsReadOk) {
                snprintf(st.heading, sizeof(st.heading), "--");
                snprintf(
                    st.lines[0],
                    LIVE_TEST_LINE_LEN,
                    "%s",
                    result == ApdsReadSaturated ? "Saturated - move back" :
                                                  "No sample came back");
                publish(ctx, &st);
                apds9960_delay(stop, APDS9960_POLL_MS);
                continue;
            }

            if(pdata < seen_min) seen_min = pdata;
            if(pdata > seen_max) seen_max = pdata;
            bool proved = (seen_max - seen_min) >= APDS9960_PROOF_RISE;

            st.value = (float)pdata;
            st.bar = pdata;
            st.bar_max = 255;
            snprintf(st.heading, sizeof(st.heading), "%u", pdata);
            snprintf(st.unit, sizeof(st.unit), "cnt");
            st.phase = proved ? LiveTestPhasePassed : LiveTestPhaseRunning;
            snprintf(
                st.lines[0],
                LIVE_TEST_LINE_LEN,
                "%s",
                proved ? "It follows your hand" : "Wave a hand over it");
            publish(ctx, &st);

            apds9960_delay(stop, APDS9960_POLL_MS);
        }

        // Park only what we started, in the reverse of the order it was
        // started: the engine stops first, so the pulse and gain settings are
        // never changed while it is running, which is the thing page 20 warns
        // against. If the ID never matched, apds9960_start was never called
        // and not one byte is written here — whatever lives at this address,
        // its registers are not ours to clear.
        if(touched.enable) {
            i2c_worker_write_reg(
                addr7, APDS9960_REG_ENABLE, APDS9960_ENABLE_OFF, I2C_REG_TIMEOUT_MS);
        }
        if(touched.control) {
            i2c_worker_write_reg(
                addr7, APDS9960_REG_CONTROL, APDS9960_CONTROL_RESET, I2C_REG_TIMEOUT_MS);
        }
        if(touched.ppulse) {
            i2c_worker_write_reg(
                addr7, APDS9960_REG_PPULSE, APDS9960_PPULSE_RESET, I2C_REG_TIMEOUT_MS);
        }

        if(*stop) break;

        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseLost;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "It replied, then stopped.");
        snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "Check 3V3 and the wires.");
        publish(ctx, &st);
        apds9960_delay(stop, 500);
    }
}

const LiveTest live_test_apds9960 = {
    .chip = "APDS9960",
    .title = "APDS9960 test",
    .offer = "Wave your hand at it",
    .run = apds9960_run,
    .draw = NULL,
};

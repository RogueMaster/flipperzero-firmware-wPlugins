#include "live_vl6180x.h"

#include <furi.h>
#include <string.h>
#include <stdio.h>

// Registers from the ST VL6180X datasheet (DocID026171 Rev 3), section 6.2.
// This part indexes its registers with a 16-bit big-endian address, which is
// why every access below goes through the *_reg16_addr helpers.
#define VL6180X_IDENTIFICATION_MODEL_ID      0x0000
#define VL6180X_SYSTEM_INTERRUPT_CONFIG_GPIO 0x0014
#define VL6180X_SYSTEM_INTERRUPT_CLEAR       0x0015
#define VL6180X_SYSRANGE_START               0x0018
#define VL6180X_RESULT_RANGE_STATUS          0x004D
#define VL6180X_RESULT_INTERRUPT_STATUS_GPIO 0x004F
#define VL6180X_RESULT_RANGE_VAL             0x0062

// The reset value of IDENTIFICATION__MODEL_ID in the same section 6.2 table:
// a VL6180X reads 0xB4 there, and nothing else this app knows about does.
#define VL6180X_MODEL_ID_VALUE 0xB4

// SYSRANGE__START bit 1 selects the mode (0 = single-shot) and bit 0 is the
// trigger, auto-cleared by firmware. 0x01 is therefore "take one reading".
#define VL6180X_START_SINGLE_SHOT 0x01

// RESULT__INTERRUPT_STATUS_GPIO bits 2:0, value 4 = New Sample Ready.
#define VL6180X_INT_RANGE_MASK 0x07
#define VL6180X_INT_NEW_SAMPLE 0x04

// SYSTEM__INTERRUPT_CONFIG_GPIO range_int_mode, bits 2:0. It resets to 0 —
// "Disabled" — and with it disabled the status register above never reports a
// sample, so a measurement looks like a dead sensor. Setting it to 4 is what
// makes single-shot ranging observable at all.
#define VL6180X_INT_CONFIG_RANGE_READY 0x04

// And the reset value it is put back to. This is the only register this test
// changes that does not undo itself, so it is the only one to restore.
#define VL6180X_INT_CONFIG_DISABLED 0x00

// SYSTEM__INTERRUPT_CLEAR bits 0/1/2 clear the range, ALS and error
// interrupts; writing all three is the documented way to reset the flags.
#define VL6180X_INT_CLEAR_ALL 0x07

// SYSRANGE__MAX_CONVERGENCE_TIME defaults to 49 ms, so a reading that has not
// arrived in four times that is a sensor that has stopped answering.
#define VL6180X_CONVERGE_TIMEOUT_MS 200
#define VL6180X_POLL_MS             100

// RESULT__RANGE_VAL is a single byte of millimetres, and the part is specified
// to 100 mm (up to ~200 mm on a good target), so this is the full useful span.
#define VL6180X_RANGE_SCALE_MM 200

// Proof is the number *moving*. A stuck register reads the same value forever;
// a working time-of-flight sensor cannot, once a hand comes near it.
#define VL6180X_PROOF_SPREAD_MM 30

static void vl6180x_delay(const volatile bool* stop, uint32_t ms) {
    while(ms && !*stop) {
        uint32_t chunk = ms > 25 ? 25 : ms;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

static void vl6180x_set_lines(LiveTestState* st, const char* l0, const char* l1, const char* l2) {
    const char* src[LIVE_TEST_LINES] = {l0, l1, l2};
    for(size_t i = 0; i < LIVE_TEST_LINES; i++) {
        snprintf(st->lines[i], LIVE_TEST_LINE_LEN, "%s", src[i] ? src[i] : "");
    }
}

// One single-shot measurement. Returns false when the sensor stopped talking;
// *error_code is the datasheet's RESULT__RANGE_STATUS nibble, non-zero for a
// reading the part itself does not trust (no target, too much ambient light).
static bool vl6180x_measure(
    const LiveTestI2c* i2c,
    uint8_t addr7,
    const volatile bool* stop,
    uint8_t* mm,
    uint8_t* error_code) {
    if(!i2c->write_reg16_addr(
           addr7, VL6180X_SYSRANGE_START, VL6180X_START_SINGLE_SHOT, LIVE_TEST_TIMEOUT_MS))
        return false;

    uint8_t status = 0;
    uint32_t waited = 0;
    for(;;) {
        if(*stop) return false;
        if(!i2c->read_reg16_addr(
               addr7, VL6180X_RESULT_INTERRUPT_STATUS_GPIO, &status, 1, LIVE_TEST_TIMEOUT_MS))
            return false;
        if((status & VL6180X_INT_RANGE_MASK) == VL6180X_INT_NEW_SAMPLE) break;
        if(waited >= VL6180X_CONVERGE_TIMEOUT_MS) return false;
        furi_delay_ms(5);
        waited += 5;
    }

    uint8_t range_status = 0;
    bool ok = i2c->read_reg16_addr(
                  addr7, VL6180X_RESULT_RANGE_STATUS, &range_status, 1, LIVE_TEST_TIMEOUT_MS) &&
              i2c->read_reg16_addr(addr7, VL6180X_RESULT_RANGE_VAL, mm, 1, LIVE_TEST_TIMEOUT_MS);

    // Clear the flag even on a failed read, or the next measurement sees a
    // sample-ready that belongs to this one.
    i2c->write_reg16_addr(
        addr7, VL6180X_SYSTEM_INTERRUPT_CLEAR, VL6180X_INT_CLEAR_ALL, LIVE_TEST_TIMEOUT_MS);

    *error_code = range_status >> 4;
    return ok;
}

static void vl6180x_run(const LiveTestEnv* env) {
    const uint8_t addr7 = env->addr7;
    const volatile bool* stop = env->stop;
    const LiveTestI2c* i2c = env->i2c;
    const LiveTestPublish publish = env->publish;
    void* const ctx = env->ctx;

    while(!*stop) {
        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseStarting;
        vl6180x_set_lines(&st, "Waking the laser", "Point it at something", NULL);
        publish(ctx, &st);

        // The model ID again, not just an ACK: after a hot-unplug whatever is
        // wired up next may well answer at the same address.
        uint8_t model_id = 0;
        LiveTestIdResult id_seen;
        if(!live_test_read_id16(i2c, addr7, VL6180X_IDENTIFICATION_MODEL_ID, &model_id)) {
            id_seen = live_test_id_unreadable(i2c, addr7);
        } else {
            id_seen = model_id == VL6180X_MODEL_ID_VALUE ? LiveTestIdMatch : LiveTestIdMismatch;
        }
        bool alive = (id_seen == LiveTestIdMatch);

        // Tracked separately from `alive` because the two answer different
        // questions: whether the part is worth reading, and whether a register
        // was changed and has to be put back. They stop agreeing the moment
        // the first write lands and the second does not.
        bool configured = false;
        if(alive) {
            configured = i2c->write_reg16_addr(
                addr7,
                VL6180X_SYSTEM_INTERRUPT_CONFIG_GPIO,
                VL6180X_INT_CONFIG_RANGE_READY,
                LIVE_TEST_TIMEOUT_MS);
            alive = configured && i2c->write_reg16_addr(
                                      addr7,
                                      VL6180X_SYSTEM_INTERRUPT_CLEAR,
                                      VL6180X_INT_CLEAR_ALL,
                                      LIVE_TEST_TIMEOUT_MS);
        }

        uint8_t seen_min = 255, seen_max = 0;
        bool proved = false;
        uint8_t errors = 0;

        while(alive && !*stop && errors < 3) {
            uint8_t mm = 0, error_code = 0;
            if(!vl6180x_measure(i2c, addr7, stop, &mm, &error_code)) {
                errors++;
                vl6180x_delay(stop, VL6180X_POLL_MS);
                continue;
            }
            errors = 0;

            char detail[LIVE_TEST_LINE_LEN] = {0};
            if(error_code == 0) {
                if(mm < seen_min) seen_min = mm;
                if(mm > seen_max) seen_max = mm;
                if(seen_max - seen_min >= VL6180X_PROOF_SPREAD_MM) proved = true;

                st.value = (float)mm;
                snprintf(st.heading, sizeof(st.heading), "%u", mm);
            } else {
                // The part itself rejected this reading. Print the code it
                // gave rather than a guess at why: without ST's optional
                // tuning settings loaded, "no target" and "could not
                // converge" are both plausible and we cannot tell them apart.
                st.value = -1.0f;
                snprintf(st.heading, sizeof(st.heading), "--");
                snprintf(detail, sizeof(detail), "err 0x%X", error_code);
            }

            st.phase = proved ? LiveTestPhasePassed : LiveTestPhaseRunning;
            vl6180x_set_lines(
                &st,
                proved          ? "It tracks - real sensor" :
                error_code == 0 ? "Move your hand closer" :
                                  "Hold a hand 5cm away",
                detail,
                NULL);
            publish(ctx, &st);

            vl6180x_delay(stop, VL6180X_POLL_MS);
        }

        // Park only what we started, before the stop check so that leaving the
        // screen runs it too. Single-shot ranging does stop on its own, and
        // INTERRUPT_CLEAR is self-clearing, but the interrupt config is not:
        // left at 4 it keeps asserting range-ready on GPIO1 for whatever uses
        // this sensor next. Nothing is written if the model ID never matched.
        if(configured) {
            i2c->write_reg16_addr(
                addr7,
                VL6180X_SYSTEM_INTERRUPT_CONFIG_GPIO,
                VL6180X_INT_CONFIG_DISABLED,
                LIVE_TEST_TIMEOUT_MS);
        }

        if(*stop) break;

        memset(&st, 0, sizeof(st));
        if(id_seen == LiveTestIdMismatch) {
            // Do not send them to the wiring: the wiring is fine, the module
            // is not a VL6180X. Worded for both ways of finding that out — a
            // model ID that read back wrong, and one that could not be read at
            // all from an address that still answers.
            char where[LIVE_TEST_LINE_LEN];
            snprintf(where, sizeof(where), "0x%02X answers, but not", addr7);
            st.phase = LiveTestPhaseWrongChip;
            vl6180x_set_lines(&st, where, "the way a VL6180X does.", NULL);
        } else {
            st.phase = LiveTestPhaseLost;
            vl6180x_set_lines(&st, "It replied, then stopped.", "Check 3V3 and the wires.", NULL);
        }
        publish(ctx, &st);
        vl6180x_delay(
            stop,
            st.phase == LiveTestPhaseWrongChip ? LIVE_TEST_WRONG_CHIP_RETRY_MS :
                                                 LIVE_TEST_RETRY_MS);
    }
}

static void vl6180x_draw(Canvas* canvas, const LiveTestState* st, uint32_t frame) {
    UNUSED(frame);

    canvas_set_font(canvas, FontBigNumbers);
    // Measured in the big font, before switching: canvas_string_width answers
    // for whatever font is current, so asking afterwards puts "mm" on top of
    // the number.
    uint8_t num_w = canvas_string_width(canvas, st->heading);
    canvas_draw_str(canvas, 4, 26, st->heading);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4 + num_w + 4, 26, "mm");

    // The reading is straight out of the part with none of ST's optional
    // tuning applied, so it is honest about being approximate.
    canvas_draw_str_aligned(canvas, 126, 12, AlignRight, AlignBottom, "raw, uncalibrated");

    // A ruler makes "the number moves" visible from across the room.
    const uint8_t x0 = 4, x1 = 124, y = 40;
    canvas_draw_line(canvas, x0, y, x1, y);
    for(uint8_t i = 0; i <= 4; i++) {
        uint8_t tx = x0 + (uint8_t)((uint16_t)(x1 - x0) * i / 4);
        canvas_draw_line(canvas, tx, y - 2, tx, y + 2);
    }

    if(st->value >= 0.0f) {
        uint16_t mm = (uint16_t)st->value;
        if(mm > VL6180X_RANGE_SCALE_MM) mm = VL6180X_RANGE_SCALE_MM;
        uint8_t mx = x0 + (uint8_t)((uint32_t)(x1 - x0) * mm / VL6180X_RANGE_SCALE_MM);
        canvas_draw_disc(canvas, mx, y, 3);
    }
    canvas_draw_str(canvas, x0, y + 10, "0");
    canvas_draw_str_aligned(canvas, x1, y + 10, AlignRight, AlignBottom, "200mm");
    // Between the scale ends: whatever the part said about a reading it did
    // not like.
    canvas_draw_str_aligned(canvas, 64, y + 10, AlignCenter, AlignBottom, st->lines[1]);

    canvas_draw_box(canvas, 0, 55, 128, 9);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, st->lines[0]);
    canvas_set_color(canvas, ColorBlack);
}

const LiveTest live_test_vl6180x = {
    .chip = "VL6180X",
    .title = "VL6180X live test",
    .offer = "Watch it measure",
    .addrs = {0x29},
    .run = vl6180x_run,
    .draw = vl6180x_draw,
};

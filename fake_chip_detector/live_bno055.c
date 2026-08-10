#include "live_bno055.h"
#include "i2c_worker.h"

#include <furi.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

// Registers from the Bosch BNO055 datasheet (BST-BNO055-DS000-14), page 51
// onwards. CHIP_ID reads 0xA0 on every genuine part.
#define BNO055_REG_CHIP_ID 0x00
#define BNO055_REG_EUL_HEADING_LSB 0x1A
#define BNO055_REG_CALIB_STAT 0x35
#define BNO055_REG_OPR_MODE 0x3D
#define BNO055_CHIP_ID_VALUE 0xA0
#define BNO055_MODE_CONFIG 0x00
#define BNO055_MODE_NDOF 0x0C

// Datasheet table 3-6: CONFIG -> any operating mode takes 7 ms, the other way
// 19 ms. 30 ms is that with room to spare. The 700 ms is not a mode switch —
// it is fusion needing a few output cycles before the heading means anything.
#define BNO055_MODE_SWITCH_MS 30
#define BNO055_FUSION_SETTLE_MS 700
#define BNO055_POLL_MS 100
#define BNO055_MAG_CAL_MAX 3

// Sleeps in slices so leaving the screen is not stuck behind a settle delay.
static void bno055_delay(const volatile bool* stop, uint32_t ms) {
    while(ms && !*stop) {
        uint32_t chunk = ms > 50 ? 50 : ms;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

static void bno055_set_lines(LiveTestState* st, const char* l0, const char* l1, const char* l2) {
    const char* src[LIVE_TEST_LINES] = {l0, l1, l2};
    for(size_t i = 0; i < LIVE_TEST_LINES; i++) {
        snprintf(st->lines[i], LIVE_TEST_LINE_LEN, "%s", src[i] ? src[i] : "");
    }
}

// CHIP_ID, not just an ACK: after a hot-unplug the next thing plugged in may
// well answer at the same address, and everything below writes to registers.
static bool bno055_present(uint8_t addr7) {
    uint8_t chip_id = 0;
    if(!i2c_worker_read_reg(addr7, BNO055_REG_CHIP_ID, &chip_id, I2C_REG_TIMEOUT_MS)) return false;
    return chip_id == BNO055_CHIP_ID_VALUE;
}

// Returns true only once the part is actually running fusion — which is also
// the answer to "is there anything to put back afterwards?".
static bool bno055_enter_ndof(uint8_t addr7, const volatile bool* stop) {
    // Mode changes are only accepted from CONFIG, so go there first even if
    // the part is already idle.
    if(!i2c_worker_write_reg(addr7, BNO055_REG_OPR_MODE, BNO055_MODE_CONFIG, I2C_REG_TIMEOUT_MS))
        return false;
    bno055_delay(stop, BNO055_MODE_SWITCH_MS);
    if(*stop) return false;

    if(!i2c_worker_write_reg(addr7, BNO055_REG_OPR_MODE, BNO055_MODE_NDOF, I2C_REG_TIMEOUT_MS))
        return false;
    bno055_delay(stop, BNO055_FUSION_SETTLE_MS);
    return true;
}

static void
    bno055_poll(uint8_t addr7, const volatile bool* stop, LiveTestPublish publish, void* ctx) {
    LiveTestState st;
    memset(&st, 0, sizeof(st));
    st.progress_max = BNO055_MAG_CAL_MAX;

    uint8_t errors = 0;
    while(!*stop && errors < 3) {
        // Both heading bytes in one transaction, so the reading cannot tear
        // across an update.
        uint8_t heading[2] = {0};
        uint8_t calib = 0;
        bool ok =
            i2c_worker_read_mem(
                addr7, BNO055_REG_EUL_HEADING_LSB, heading, sizeof(heading), I2C_REG_TIMEOUT_MS) &&
            i2c_worker_read_reg(addr7, BNO055_REG_CALIB_STAT, &calib, I2C_REG_TIMEOUT_MS);

        if(!ok) {
            errors++;
            bno055_delay(stop, BNO055_POLL_MS);
            continue;
        }
        errors = 0;

        // Euler output is 1/16 of a degree per LSB (datasheet section 3.6.5.5)
        // and wraps, so a negative raw value is simply the far side of north.
        int32_t raw = (int16_t)(((uint16_t)heading[1] << 8) | heading[0]);
        if(raw < 0) raw += 360 * 16;

        st.progress = calib & 0x03; // CALIB_STAT bits 1:0 = magnetometer
        st.value = (float)raw / 16.0f;
        st.phase = (st.progress >= BNO055_MAG_CAL_MAX) ? LiveTestPhasePassed :
                                                         LiveTestPhaseRunning;

        snprintf(st.heading, sizeof(st.heading), "%ld.%ld", (long)(raw / 16), (long)((raw % 16) * 10 / 16));
        if(st.phase == LiveTestPhasePassed) {
            bno055_set_lines(&st, "Heading, degrees", "Calibrated - now spin it", NULL);
        } else {
            bno055_set_lines(&st, "Heading, degrees", "Rotate in a figure-8", "to calibrate the mag");
        }
        publish(ctx, &st);

        bno055_delay(stop, BNO055_POLL_MS);
    }
}

static void
    bno055_run(uint8_t addr7, const volatile bool* stop, LiveTestPublish publish, void* ctx) {
    while(!*stop) {
        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseStarting;
        st.progress_max = BNO055_MAG_CAL_MAX;
        bno055_set_lines(&st, "Starting NDOF fusion", "Nine axes, warming up", NULL);
        publish(ctx, &st);

        bool in_ndof = false;
        if(bno055_present(addr7)) {
            in_ndof = bno055_enter_ndof(addr7, stop);
            if(in_ndof) bno055_poll(addr7, stop, publish, ctx);
        }

        // Park only what we started. NDOF runs the fusion core at ~12 mA and
        // nobody is watching once this returns — but if the CHIP_ID did not
        // match, whatever is at this address is not a BNO055 and 0x3D is not
        // its mode register. Do not write to a stranger.
        if(in_ndof) {
            i2c_worker_write_reg(
                addr7, BNO055_REG_OPR_MODE, BNO055_MODE_CONFIG, I2C_REG_TIMEOUT_MS);
        }

        if(*stop) break;

        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseLost;
        bno055_set_lines(&st, "It replied, then stopped.", "Check 3V3 and the wires.", NULL);
        publish(ctx, &st);
        bno055_delay(stop, 500);
    }
}

// Lemniscate traced by a moving dot: the figure-8 motion the magnetometer
// needs for calibration, shown instead of described.
static void bno055_draw_figure8(Canvas* canvas, uint8_t cx, uint8_t cy, uint32_t frame) {
    const float rx = 20.0f, ry = 8.0f;
    for(uint8_t i = 0; i < 32; i++) {
        float t = (float)i / 32.0f * 2.0f * (float)M_PI;
        canvas_draw_dot(canvas, cx + (int8_t)(rx * sinf(t)), cy + (int8_t)(ry * sinf(2 * t)));
    }
    float t = (float)(frame % 48) / 48.0f * 2.0f * (float)M_PI;
    canvas_draw_disc(canvas, cx + (int8_t)(rx * sinf(t)), cy + (int8_t)(ry * sinf(2 * t)), 2);
}

static void bno055_draw(Canvas* canvas, const LiveTestState* st, uint32_t frame) {
    char buf[24];

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, 2, 26, st->heading);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 36, "deg");

    // Compass: 0 deg is north and north is up, so the needle points where the
    // sensor thinks north is. Turn the board and it has to follow.
    const uint8_t cx = 100, cy = 24, r = 18;
    canvas_draw_circle(canvas, cx, cy, r);
    canvas_draw_str_aligned(canvas, cx, cy - r + 6, AlignCenter, AlignBottom, "N");
    float a = st->value * ((float)M_PI / 180.0f);
    int8_t dx = (int8_t)(sinf(a) * (r - 4));
    int8_t dy = (int8_t)(-cosf(a) * (r - 4));
    canvas_draw_line(canvas, cx, cy, cx + dx, cy + dy);
    canvas_draw_disc(canvas, cx + dx, cy + dy, 2);

    snprintf(buf, sizeof(buf), "MAG CAL %u/%u", st->progress, st->progress_max);
    canvas_draw_str(canvas, 2, 47, buf);
    for(uint8_t i = 0; i < st->progress_max; i++) {
        uint8_t bx = 58 + i * 9;
        if(i < st->progress) {
            canvas_draw_box(canvas, bx, 40, 7, 7);
        } else {
            canvas_draw_frame(canvas, bx, 40, 7, 7);
        }
    }

    if(st->progress < st->progress_max) {
        bno055_draw_figure8(canvas, 24, 57, frame);
        canvas_draw_str(canvas, 50, 60, "Rotate in figure-8");
    } else {
        canvas_draw_box(canvas, 0, 51, 128, 13);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, 64, 61, AlignCenter, AlignBottom, "CALIBRATED - now spin it");
        canvas_set_color(canvas, ColorBlack);
    }
}

const LiveTest live_test_bno055 = {
    .chip = "BNO055",
    .title = "BNO055 live test",
    .offer = "Prove it finds north",
    .run = bno055_run,
    .draw = bno055_draw,
};

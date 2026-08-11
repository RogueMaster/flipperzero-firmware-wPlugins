#include "live_mpu6050.h"

#include <furi.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

// From the InvenSense MPU-6000/6050 Register Map (RM-MPU-6000A-00 rev 4.0),
// with the MPU-6500 (RM-MPU-6500A-00 rev 2.1) and MPU-9250 (RM-MPU-9250A-00
// rev 1.6) register maps confirming that all three place the accelerometer at
// the same addresses, in the same byte order, with the same default scale.
// That is why one file serves three database entries.
#define MPU_REG_ACCEL_XOUT_H 0x3B
#define MPU_REG_ACCEL_CONFIG 0x1C
#define MPU_REG_PWR_MGMT_1   0x6B
#define MPU_REG_WHO_AM_I     0x75

// WHO_AM_I tells the three apart. Worth knowing for the counterfeit case this
// app exists for: a board sold as an MPU-9250 that answers 0x70 is an
// MPU-6500, which is the single most common relabel in this family. The scan
// has already made that call by the time a live test runs — all this test
// needs is to be sure it is talking to one of the three before it writes.
#define MPU6050_WHO_AM_I 0x68
#define MPU6500_WHO_AM_I 0x70
#define MPU9250_WHO_AM_I 0x71

// The MPU-6050 boots with SLEEP set (its PWR_MGMT_1 resets to 0x40), so it has
// to be woken. The 6500 and 9250 reset to 0x01 and are already awake. Writing
// zero wakes all three; what differs is what has to be put back afterwards,
// which is why the reset value is remembered per part.
#define MPU_PWR_WAKE      0x00
#define MPU6050_PWR_RESET 0x40
#define MPU6500_PWR_RESET 0x01

// The 6500 and 9250 specify 20 ms from sleep; the 6050 documents no figure at
// all, so the larger of the two known numbers is used for every part.
#define MPU_WAKE_SETTLE_MS 30
#define MPU_POLL_MS        100

// ACCEL_CONFIG resets to 0x00, which is the +/-2 g range, and the register map
// gives 16384 LSB per g there. It is written anyway rather than assumed: the
// part keeps its configuration until power is removed, so a board that some
// other firmware left at +/-16 g would divide by a scale eight times too small
// and print a confident, wrong g — with nothing on screen to say so. Writing
// the reset value also means there is nothing extra to put back afterwards.
#define MPU_ACCEL_CONFIG_2G 0x00
#define MPU_LSB_PER_G       16384

// Enough of gravity on one axis to call it the one holding the board up. Well
// clear of the +/-80 mg zero-g tolerance and of the tilt angles a hand wobble
// produces.
#define MPU_DOMINANT_MG 700

// Two different axes have to take the weight. That is the part a stuck
// register cannot do: a canned constant can look like 1 g on Z forever, but it
// cannot hand gravity over to X when the board is tipped on its side.
#define MPU_PROOF_AXES 2

typedef struct {
    uint8_t who_am_i;
    uint8_t pwr_reset;
} MpuIdentity;

static void mpu_delay(const volatile bool* stop, uint32_t ms) {
    while(ms && !*stop) {
        uint32_t chunk = ms > 40 ? 40 : ms;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

static LiveTestIdResult mpu_identify(const LiveTestI2c* i2c, uint8_t addr7, MpuIdentity* id) {
    if(!live_test_read_id8(i2c, addr7, MPU_REG_WHO_AM_I, &id->who_am_i))
        return live_test_id_unreadable(i2c, addr7);
    switch(id->who_am_i) {
    case MPU6050_WHO_AM_I:
        id->pwr_reset = MPU6050_PWR_RESET;
        return LiveTestIdMatch;
    case MPU6500_WHO_AM_I:
    case MPU9250_WHO_AM_I:
        id->pwr_reset = MPU6500_PWR_RESET;
        return LiveTestIdMatch;
    default:
        return LiveTestIdMismatch;
    }
}

// One burst starting at ACCEL_XOUT_H. The register map is explicit that a
// burst read returns all six bytes from the same sampling instant, which
// single reads do not guarantee — and a torn sample would show up here as a
// magnitude that is not 1 g.
static bool mpu_read_accel(const LiveTestI2c* i2c, uint8_t addr7, int32_t out_mg[3]) {
    uint8_t buf[6] = {0};
    if(!i2c->read_mem(addr7, MPU_REG_ACCEL_XOUT_H, buf, sizeof(buf), LIVE_TEST_TIMEOUT_MS))
        return false;

    for(uint8_t axis = 0; axis < 3; axis++) {
        // Big-endian, high byte first. The ADXL345 is the other way round.
        int16_t raw = (int16_t)(((uint16_t)buf[axis * 2] << 8) | buf[axis * 2 + 1]);
        out_mg[axis] = (int32_t)raw * 1000 / MPU_LSB_PER_G;
    }
    return true;
}

static void mpu_run(const LiveTestEnv* env) {
    const uint8_t addr7 = env->addr7;
    const volatile bool* stop = env->stop;
    const LiveTestI2c* i2c = env->i2c;
    const LiveTestPublish publish = env->publish;
    void* const ctx = env->ctx;
    while(!*stop) {
        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseStarting;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Waking the accelerometer");
        publish(ctx, &st);

        MpuIdentity id = {0};
        // Two flags, because they answer different questions. `woken` says a
        // register was changed and has to be put back; `ready` says the scale
        // is known and a number may be shown. If the range write fails the
        // part is awake but unmeasurable, and only the first is true.
        bool woken = false;
        bool ready = false;
        const LiveTestIdResult id_seen = mpu_identify(i2c, addr7, &id);
        if(id_seen == LiveTestIdMatch) {
            woken = i2c->write_reg(addr7, MPU_REG_PWR_MGMT_1, MPU_PWR_WAKE, LIVE_TEST_TIMEOUT_MS);
            if(woken)
                ready = i2c->write_reg(
                    addr7, MPU_REG_ACCEL_CONFIG, MPU_ACCEL_CONFIG_2G, LIVE_TEST_TIMEOUT_MS);
            if(ready) mpu_delay(stop, MPU_WAKE_SETTLE_MS);
        }

        uint8_t axes_seen = 0;
        uint8_t errors = 0;

        while(ready && !*stop && errors < 3) {
            int32_t mg[3] = {0};
            if(!mpu_read_accel(i2c, addr7, mg)) {
                errors++;
                mpu_delay(stop, MPU_POLL_MS);
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
            if(best >= MPU_DOMINANT_MG) axes_seen |= (uint8_t)(1u << dominant);

            uint8_t count = 0;
            for(uint8_t axis = 0; axis < 3; axis++) {
                if(axes_seen & (1u << axis)) count++;
            }

            float total =
                sqrtf((float)mg[0] * mg[0] + (float)mg[1] * mg[1] + (float)mg[2] * mg[2]);

            memset(&st, 0, sizeof(st));
            st.phase = (count >= MPU_PROOF_AXES) ? LiveTestPhasePassed : LiveTestPhaseRunning;
            st.value = total / 1000.0f;
            st.progress = count;
            st.progress_max = MPU_PROOF_AXES;
            snprintf(
                st.heading,
                sizeof(st.heading),
                "%u.%02u",
                (unsigned)((uint32_t)total / 1000u),
                (unsigned)((uint32_t)total % 1000u / 10u));
            snprintf(st.unit, sizeof(st.unit), "g");

            if(count >= MPU_PROOF_AXES) {
                snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Gravity followed the tilt");
            } else if(best >= MPU_DOMINANT_MG) {
                snprintf(
                    st.lines[0], LIVE_TEST_LINE_LEN, "Gravity on %c - tip it", 'X' + dominant);
            } else {
                snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Hold it still and flat");
            }
            publish(ctx, &st);

            mpu_delay(stop, MPU_POLL_MS);
        }

        // Park only what we started, and park it where this particular part
        // woke up: the 6050 resets asleep, the 6500 and 9250 reset awake. If
        // WHO_AM_I never matched, nothing was written and nothing is restored.
        if(woken) {
            i2c->write_reg(addr7, MPU_REG_PWR_MGMT_1, id.pwr_reset, LIVE_TEST_TIMEOUT_MS);
        }

        if(*stop) break;

        memset(&st, 0, sizeof(st));
        if(id_seen == LiveTestIdMismatch) {
            // 0x68 is the busiest address in the database — a DS3231 and ten
            // other IMUs share it — so "something else is here" is by far the
            // likeliest way this test fails, and sending the user to check
            // their wiring would be actively misleading. It does not name
            // WHO_AM_I either: this branch is also where a part that never
            // answered that read at all ends up, and saying a register said
            // something when it was never read would be a lie.
            st.phase = LiveTestPhaseWrongChip;
            snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "0x%02X answers, but not", addr7);
            snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "the way an MPU does.");
        } else {
            st.phase = LiveTestPhaseLost;
            snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "It replied, then stopped.");
            snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "Check 3V3 and the wires.");
        }
        publish(ctx, &st);
        mpu_delay(
            stop,
            st.phase == LiveTestPhaseWrongChip ? LIVE_TEST_WRONG_CHIP_RETRY_MS :
                                                 LIVE_TEST_RETRY_MS);
    }
}

const LiveTest live_test_mpu6050 = {
    .chip = "MPU6050",
    .title = "MPU6050 test",
    .offer = "Tip it and watch gravity",
    .addrs = {0x68, 0x69},
    .run = mpu_run,
    .draw = NULL,
};

const LiveTest live_test_mpu6500 = {
    .chip = "MPU6500",
    .title = "MPU6500 test",
    .offer = "Tip it and watch gravity",
    .addrs = {0x68, 0x69},
    .run = mpu_run,
    .draw = NULL,
};

const LiveTest live_test_mpu9250 = {
    .chip = "MPU9250",
    .title = "MPU9250 test",
    .offer = "Tip it and watch gravity",
    .addrs = {0x68, 0x69},
    .run = mpu_run,
    .draw = NULL,
};

// A live test for the MYPART sensor, built as a loadable plugin.
//
// This file is a complete, working example. Change the register numbers, the
// pass condition and the strings, build it with ufbt, and drop the .fal onto
// the card — nothing in the app has to be rebuilt.
//
// The two halves below are worth telling apart. Everything above the
// FlipperAppPluginDescriptor is an ordinary live test, written exactly as the
// ones inside the app are; everything below it is the four lines that make it
// loadable. That is deliberate: a test that earns its place can be moved into
// the app by deleting the bottom of this file, and one that lives inside the
// app can be pulled out by adding it back.
//
// Before you write one, read LIVE_TESTS.md. The rules there are not style
// preferences — a test that writes to a part it has not identified can brick
// somebody's board, and a test that passes too easily is worse than no test at
// all, because it tells a person their counterfeit is genuine.

#include "../fake_chip_detector/live_test.h"

#include <flipper_application/flipper_application.h>

#include <furi.h>
#include <string.h>
#include <stdio.h>

// EVERY register constant needs a datasheet citation, like this one. Cite the
// document number, the revision and the page. This is the single most
// important convention in the project: a wrong constant here does not produce
// a bug report, it produces a person returning a genuine sensor.
#define MYPART_REG_ID 0x0F // MYPART datasheet DS12345 Rev 3, page 21
#define MYPART_ID_VALUE 0x5A // "the WHO_AM_I register reads 0x5A", page 21
#define MYPART_REG_DATA 0x28 // output register, page 24

// Wait long enough between reads that the part has produced a new sample.
#define MYPART_POLL_MS 100

// How much the reading has to move before the test believes the part is real.
// Choose this from the datasheet, not by trying it until it passes: it has to
// be far enough above the noise floor that a stuck register cannot drift into
// it, and low enough that a genuine part reaches it at a shop counter with
// nothing but a hand.
#define MYPART_PROOF_DELTA 30

// Sleeps in short slices so leaving the screen is instant. Never call
// furi_delay_ms for a long stretch: the user pressing Back would wait it out.
static void mypart_delay(const volatile bool* stop, uint32_t ms) {
    while(ms && !*stop) {
        uint32_t chunk = ms > 50 ? 50 : ms;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

// Confirm what answered BEFORE writing anything to it. An address that
// acknowledges is not proof of which chip is behind it, and the register you
// were about to configure may mean something entirely different on whatever is
// actually there.
static bool mypart_present(const LiveTestI2c* i2c, uint8_t addr7) {
    uint8_t id = 0;
    if(!i2c->read_reg(addr7, MYPART_REG_ID, &id, LIVE_TEST_TIMEOUT_MS)) return false;
    return id == MYPART_ID_VALUE;
}

static void mypart_run(const LiveTestEnv* env) {
    const uint8_t addr7 = env->addr7;
    const volatile bool* stop = env->stop;
    const LiveTestI2c* i2c = env->i2c;

    // The outer loop exists so the test recovers by itself when the sensor is
    // unplugged and plugged back in, which is exactly what happens when
    // somebody is fiddling with jumper wires.
    while(!*stop) {
        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseStarting;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Waking the sensor");
        env->publish(env->ctx, &st);

        bool ready = mypart_present(i2c, addr7);
        // If your part needs configuring, do it here, and remember what you
        // changed so you can put it back at the bottom of this loop.

        uint8_t lowest = 0xFF;
        uint8_t errors = 0;

        while(ready && !*stop && errors < 3) {
            uint8_t reading = 0;
            if(!i2c->read_reg(addr7, MYPART_REG_DATA, &reading, LIVE_TEST_TIMEOUT_MS)) {
                errors++;
                mypart_delay(stop, MYPART_POLL_MS);
                continue;
            }
            errors = 0;

            if(reading < lowest) lowest = reading;
            bool proved = (reading - lowest) >= MYPART_PROOF_DELTA;

            memset(&st, 0, sizeof(st));
            st.phase = proved ? LiveTestPhasePassed : LiveTestPhaseRunning;
            snprintf(st.heading, sizeof(st.heading), "%u", reading);
            snprintf(st.unit, sizeof(st.unit), "cnt");

            // A bar makes "the number is moving" obvious from across the room,
            // which is the entire proof a live test offers.
            st.bar = reading;
            st.bar_max = 255;

            snprintf(
                st.lines[0],
                LIVE_TEST_LINE_LEN,
                "%s",
                proved ? "It followed your hand" : "Wave a hand over it");
            env->publish(env->ctx, &st);

            mypart_delay(stop, MYPART_POLL_MS);
        }

        // Put back only what you actually changed, and only if you got far
        // enough to change it. There is no teardown hook — this is it.

        if(*stop) break;

        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseLost;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "It replied, then stopped.");
        snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "Check 3V3 and the wires.");
        env->publish(env->ctx, &st);
        mypart_delay(stop, 500);
    }
}

static const LiveTest live_test_mypart = {
    // Must match a chip name in the app's database character for character,
    // or the app will never offer this test after a scan. It can still be run
    // by hand from the test browser either way.
    .chip = "MYPART",
    .title = "MYPART test",
    .offer = "Wave a hand over it", // <= 26 characters

    // Every address the part can sit at. The browser probes these when the
    // test is launched by hand, so an address the part cannot actually use is
    // a write aimed at whatever else happens to answer there.
    .addrs = {0x18, 0x19},

    .run = mypart_run,
    .draw = NULL, // NULL gets you the generic screen, which is a good screen
};

// ---------------------------------------------------------------------------
// The plugin plumbing. Four lines and a function; nothing here is specific to
// your sensor except the name.

static const FlipperAppPluginDescriptor plugin_descriptor = {
    .appid = LIVE_TEST_PLUGIN_APPID,
    .ep_api_version = LIVE_TEST_PLUGIN_API_VERSION,
    .entry_point = &live_test_mypart,
};

// The name here must match `entry_point` in application.fam.
const FlipperAppPluginDescriptor* live_test_mypart_plugin_ep(void) {
    return &plugin_descriptor;
}

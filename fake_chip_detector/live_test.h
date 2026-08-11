#pragma once

#include <gui/canvas.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// An ID register is one byte, and one byte is exactly what a relabeller can
// program into a cheaper die. A live test asks the harder question: does the
// part actually *do* what that number promises? A magnetometer that tracks
// north, a barometer that moves when you cover it — those cannot be faked by
// a sticker.
//
// A test is one file. It can be compiled into the app, or built separately as
// a .fal plugin and dropped onto the SD card, and the source is identical
// either way. That is the reason for the shape of everything below: a test
// never calls a function of this app by name. The bus arrives as a table of
// pointers in LiveTestEnv, so the only symbols a test needs resolved are the
// firmware's own (furi_delay_ms, snprintf, memset). Nothing else has to be
// exported, and no part of this contract depends on how the test was built.
//
// Writing a test:
//   1. Add live_<part>.c/.h exporting `extern const LiveTest live_test_<part>;`
//   2. Fill in `chip` with the EXACT name string used in chip_db.c — that is
//      how the app finds your test after a scan — and `addrs` with every
//      address the part can sit at, so it can also be launched by hand.
//   3. Implement run(). Optionally implement draw() if plain text lines are
//      not enough; leave it NULL and you get a readable generic screen free.
//
// See LIVE_TESTS.md for the rules a test is held to and for how to build one
// as a loadable plugin.

// Bumped whenever anything below changes shape: the descriptor, the env, the
// bus table, or LiveTestState. A plugin records the version it was compiled
// against and the loader refuses to run one that disagrees, because the
// alternative is a test reading rubbish off the end of a struct it thinks it
// understands and reporting it as a measurement.
#define LIVE_TEST_PLUGIN_API_VERSION 1

// The application id a plugin must declare in its own descriptor for this app
// to accept it. Keeps someone else's .fal in the same folder from being
// mistaken for a chip test.
#define LIVE_TEST_PLUGIN_APPID "fake_chip_detector_live_test"

#define LIVE_TEST_LINES 3
#define LIVE_TEST_LINE_LEN 26
#define LIVE_TEST_HEADING_LEN 12
#define LIVE_TEST_UNIT_LEN 6

// Enough for every part in the database; the widest today uses two.
#define LIVE_TEST_MAX_ADDRS 4

// The unused-slot marker is zero on purpose. Address 0x00 is the I2C general
// call and can never be a device, so `.addrs = {0x23, 0x5C}` fills the rest
// with terminators by itself. A non-zero marker would look tidier and would
// quietly turn that same natural-looking initialiser into an array claiming
// address zero.
#define LIVE_TEST_ADDR_NONE 0x00

// Timeout for one transfer, in milliseconds. The same value the scanner uses:
// a device that is present answers immediately, so this only bounds a stuck
// bus. Defined here so a test needs no header of this app but this one.
#define LIVE_TEST_TIMEOUT_MS 50

typedef enum {
    LiveTestPhaseStarting, // configuring the part, no readings yet
    LiveTestPhaseRunning, // producing readings
    LiveTestPhaseLost, // it answered, then stopped
    LiveTestPhasePassed, // the test reached its own success condition

    // Something is at this address and talking, but its ID register says it is
    // not the part this test is for. Completely different advice from Lost:
    // there is nothing wrong with the wiring, the wrong module is plugged in.
    //
    // Appended rather than slotted in beside Lost on purpose. These are plain
    // enumerators, so inserting one would renumber LiveTestPhasePassed, and a
    // plugin compiled before the change would go on sending the old number for
    // "it passed" while the app read it as this. Adding at the end cannot do
    // that, which is why this needed no version bump.
    LiveTestPhaseWrongChip,
} LiveTestPhase;

// What an ID check found. Tests used to fold this into a bool and lost the
// distinction that matters most to the person holding the board: nothing on
// the bus at all is a wiring problem, and something on the bus that is not
// this part is not. Note that the split is between those two and not between
// "the read worked" and "the read failed" — see live_test_id_unreadable.
typedef enum {
    LiveTestIdNoAnswer, // nothing acknowledges this address any more
    LiveTestIdMismatch, // something is there, and it is not this part
    LiveTestIdMatch,
} LiveTestIdResult;

// Everything a test tells the UI. Deliberately a plain value type: the worker
// thread fills one on its stack and hands it over, so no test ever touches a
// view, a canvas or the view-model lock.
typedef struct {
    LiveTestPhase phase;

    // The one thing worth reading from across the room — a heading, a
    // pressure, a count. Drawn large. Empty string for "nothing yet".
    char heading[LIVE_TEST_HEADING_LEN];

    // Unit for the heading, set small beside it: "mm", "%RH", "lx", "C".
    // Empty for a bare number.
    char unit[LIVE_TEST_UNIT_LEN];

    // Plain language under it: what is happening, what to do next.
    char lines[LIVE_TEST_LINES][LIVE_TEST_LINE_LEN];

    // Optional step counter, e.g. calibration levels reached. Drawn as filled
    // boxes when progress_max is non-zero, and chimed once on completion.
    // At most eleven of them: that is what fits across the screen, and the app
    // clamps rather than drawing off the side of it.
    uint8_t progress;
    uint8_t progress_max;

    // Optional proportional bar, drawn when bar_max is non-zero and filled to
    // bar/bar_max. This is how a test makes "the reading is moving" obvious
    // from across the room without writing a draw() of its own. Both fields
    // zero — the state of a freshly memset struct — means no bar, so a test
    // that does not want one need not think about it.
    uint8_t bar;
    uint8_t bar_max;

    // The primary reading as a number, for a module's own draw() to render as
    // a dial, a needle or a bar. Ignored by the generic screen.
    float value;
} LiveTestState;

// Called by the test from its worker thread whenever it has something new.
// Cheap; safe to call at whatever rate the test polls at.
typedef void (*LiveTestPublish)(void* ctx, const LiveTestState* state);

// The bus. Every call acquires the external I2C handle and releases it before
// returning on every path, so the handle cannot leak no matter how a test
// exits. All addresses are 7-bit — the <<1 the HAL wants happens on the far
// side of these pointers, in exactly one place in the whole program.
typedef struct {
    bool (*device_ready)(uint8_t addr7, uint32_t timeout_ms);

    bool (*read_reg)(uint8_t addr7, uint8_t reg, uint8_t* value, uint32_t timeout_ms);
    bool (*write_reg)(uint8_t addr7, uint8_t reg, uint8_t value, uint32_t timeout_ms);
    bool (*read_mem)(uint8_t addr7, uint8_t reg, uint8_t* data, size_t len, uint32_t timeout_ms);

    // For parts that index registers with a 16-bit big-endian address: ST
    // time-of-flight sensors, Goodix touch controllers.
    bool (*read_reg16_addr)(
        uint8_t addr7,
        uint16_t reg,
        uint8_t* data,
        size_t len,
        uint32_t timeout_ms);
    bool (*write_reg16_addr)(uint8_t addr7, uint16_t reg, uint8_t value, uint32_t timeout_ms);

    // For the many parts with no register index at all: the humidity sensors
    // take a bare command and answer with a bare block, and a display takes a
    // control byte followed by a stream.
    bool (*write_raw)(uint8_t addr7, const uint8_t* data, size_t len, uint32_t timeout_ms);
    bool (*read_raw)(uint8_t addr7, uint8_t* data, size_t len, uint32_t timeout_ms);
} LiveTestI2c;

// What a failed ID read actually means. Call this instead of returning
// LiveTestIdNoAnswer the moment a read fails.
//
// A read failing is not the part being gone. Reading an 8-bit-indexed register
// is one write of the index byte and then a repeated start, and a part that
// indexes its registers with sixteen bits NACKs that exchange outright while
// sitting perfectly happily on the bus. That is not a hypothetical: a VL6180X
// lives at 0x29, so does a BNO055, and the BNO055 test read 0x00 from a real
// VL6180X and told the user to go and check their wiring.
//
// So ask the address. If it still acknowledges, something is there and does
// not answer this register - which is the wrong part, not a loose jumper. Only
// silence means gone.
//
// Inline in the header rather than a call through the bus table, so a plugin
// built before this existed keeps working and one built after needs nothing
// new from the app.
static inline LiveTestIdResult live_test_id_unreadable(const LiveTestI2c* i2c, uint8_t addr7) {
    return i2c->device_ready(addr7, LIVE_TEST_TIMEOUT_MS) ? LiveTestIdMismatch :
                                                            LiveTestIdNoAnswer;
}

// Read an ID register twice and only believe it if both reads agree. Use these
// rather than i2c->read_reg for an identification; keep read_reg for readings.
//
// One byte of ID is eight bits of evidence and the outer loop asks again twice
// a second for as long as the screen is open, so a one-in-256 accident is not
// unlikely - it is what happens if somebody walks away for a few minutes. It
// did happen: a VL6180X left on the BNO055 test eventually returned 0xA0 from
// an 8-bit read, and the test wrote to it and drew a heading off a part that
// had never measured one.
//
// Worse than the odds is where the byte comes from. An 8-bit read leaves a
// 16-bit-indexed part holding half an index, so what it answers next is not
// its register map at all - it is whatever that half-written index landed on.
// A value that will not repeat is not an identification, and two transactions
// is a cheap price for saying so.
static inline bool
    live_test_read_id8(const LiveTestI2c* i2c, uint8_t addr7, uint8_t reg, uint8_t* out) {
    uint8_t first = 0, second = 0;
    if(!i2c->read_reg(addr7, reg, &first, LIVE_TEST_TIMEOUT_MS)) return false;
    if(!i2c->read_reg(addr7, reg, &second, LIVE_TEST_TIMEOUT_MS)) return false;
    if(first != second) return false;
    *out = first;
    return true;
}

// The same for a part that indexes its registers with sixteen bits.
static inline bool
    live_test_read_id16(const LiveTestI2c* i2c, uint8_t addr7, uint16_t reg, uint8_t* out) {
    uint8_t first = 0, second = 0;
    if(!i2c->read_reg16_addr(addr7, reg, &first, 1, LIVE_TEST_TIMEOUT_MS)) return false;
    if(!i2c->read_reg16_addr(addr7, reg, &second, 1, LIVE_TEST_TIMEOUT_MS)) return false;
    if(first != second) return false;
    *out = first;
    return true;
}

// How long to wait before an outer loop tries to identify the part again.
//
// Half a second while something is merely missing: that is somebody pushing a
// jumper back in, and they should not have to wait to see it take.
#define LIVE_TEST_RETRY_MS 500

// Four times slower once the part has been identified as somebody else. There
// is nothing to recover from - the module has to be physically swapped - and
// re-reading a stranger's registers twice a second is both a worse lottery and
// a lot of pointless traffic aimed at a part whose register map is unknown.
#define LIVE_TEST_WRONG_CHIP_RETRY_MS 2000

// Everything run() is given. Passed as one pointer so the shape can grow
// behind a version bump without every test changing signature again.
typedef struct {
    // The address the part answered at. After a scan this is where it was
    // found; on a manual launch it is whichever of the test's own `addrs`
    // acknowledged. Either way the test never has to search the bus.
    uint8_t addr7;

    // Goes true when the user leaves the screen. Poll it often — see run().
    const volatile bool* stop;

    LiveTestPublish publish;
    void* ctx;

    const LiveTestI2c* i2c;
} LiveTestEnv;

typedef struct {
    // Must match ChipEntry.name in chip_db.c character for character. That is
    // how a test is matched to a part after a scan.
    const char* chip;

    const char* title; // screen title, e.g. "BNO055 test"
    const char* offer; // the pitch on the ALL GOOD screen, <= 26 chars

    // Every 7-bit address this part can sit at, in the order to try them,
    // padded with LIVE_TEST_ADDR_NONE. Required — this is what lets the test
    // be launched by hand with no scan behind it. Listing an address the part
    // cannot use is not a harmless mistake: a manual launch probes these and
    // then starts writing, and the thing that answers may be someone else.
    uint8_t addrs[LIVE_TEST_MAX_ADDRS];

    // Runs the test.
    //
    // Returns when *env->stop becomes true, and NOT BEFORE — a test that
    // finishes early leaves the user staring at a frozen screen. Loop until
    // stopped.
    //
    // And return PROMPTLY once it is. The app waits for this function on the
    // thread that draws every screen, because until it returns the test is
    // still writing into the view model and, from an SD card, still executing
    // inside a mapping about to be unmapped — so the wait cannot be abandoned.
    // While it lasts the whole app is stopped: nothing redraws, no key does
    // anything, and it looks exactly like the firmware has died. Slice long
    // delays and check the flag between the slices; every test in this
    // repository checks it at least five times a second.
    //
    // OWNS ITS OWN CLEANUP, on every exit path. There is no teardown hook and
    // there never will be one: if your part needs to be put back into a low
    // power mode, do it before each `return`, including the error returns.
    // Undo only what you actually did, and only after an ID register confirmed
    // what you are talking to.
    void (*run)(const LiveTestEnv* env);

    // Optional; NULL is fine and gets you a readable generic screen. Called
    // ONLY for the Running and Passed phases — "warming up" and "it dropped
    // off" are the app's screens, not yours, so every test looks the same
    // when nothing is being measured. Implement this when the reading itself
    // deserves a picture rather than a line of text. The canvas arrives
    // cleared and is yours entirely — 128x64, no title bar. `frame` advances
    // ~16 times a second, for animation.
    void (*draw)(Canvas* canvas, const LiveTestState* state, uint32_t frame);
} LiveTest;

// True when this test claims the given 7-bit address.
bool live_test_has_addr(const LiveTest* test, uint8_t addr7);

// The bus table to hand a test. Always the same object; never NULL.
const LiveTestI2c* live_test_i2c(void);

// NULL when this chip has no live test, which is the normal case.
// chip_name may be NULL.
const LiveTest* live_test_for_chip(const char* chip_name);

size_t live_test_count(void);
const LiveTest* live_test_get(size_t index);

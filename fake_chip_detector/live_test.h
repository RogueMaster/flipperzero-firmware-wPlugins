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
// Live tests are optional and per-chip. Each one lives in its own file and is
// listed in the table in live_test.c, so adding a test for a new part means
// writing one module and adding one line; no screen, menu or view has to
// change. When no test exists for the chip that was just identified, the app
// simply does not offer one.
//
// Writing a test:
//   1. Add live_<part>.c/.h exporting `extern const LiveTest live_test_<part>;`
//   2. Fill in `chip` with the EXACT name string used in chip_db.c — that is
//      how the app finds your test after a scan.
//   3. Implement run(). Optionally implement draw() if plain text lines are
//      not enough; leave it NULL and you get a readable generic screen free.

#define LIVE_TEST_LINES 3
#define LIVE_TEST_LINE_LEN 26
#define LIVE_TEST_HEADING_LEN 12

typedef enum {
    LiveTestPhaseStarting, // configuring the part, no readings yet
    LiveTestPhaseRunning, // producing readings
    LiveTestPhaseLost, // it answered, then stopped
    LiveTestPhasePassed, // the test reached its own success condition
} LiveTestPhase;

// Everything a test tells the UI. Deliberately a plain value type: the worker
// thread fills one on its stack and hands it over, so no test ever touches a
// view, a canvas or the view-model lock.
typedef struct {
    LiveTestPhase phase;

    // The one thing worth reading from across the room — a heading, a
    // pressure, a count. Drawn large. Empty string for "nothing yet".
    char heading[LIVE_TEST_HEADING_LEN];

    // Plain language under it: what is happening, what to do next.
    char lines[LIVE_TEST_LINES][LIVE_TEST_LINE_LEN];

    // Optional step counter, e.g. calibration levels reached. Drawn as filled
    // boxes when progress_max is non-zero, and chimed once on completion.
    uint8_t progress;
    uint8_t progress_max;

    // The primary reading as a number, for a module's own draw() to render as
    // a dial, a needle or a bar. Ignored by the generic screen.
    float value;
} LiveTestState;

// Called by the test from its worker thread whenever it has something new.
// Cheap; safe to call at whatever rate the test polls at.
typedef void (*LiveTestPublish)(void* ctx, const LiveTestState* state);

typedef struct {
    // Must match ChipEntry.name in chip_db.c character for character.
    const char* chip;

    const char* title; // screen title, e.g. "BNO055 live test"
    const char* offer; // the pitch on the ALL GOOD screen, <= 26 chars

    // Runs the test. Called on a dedicated thread with the address the scan
    // found the part at, so there is no need to search for it again.
    //
    // Returns when *stop becomes true, and NOT BEFORE — a test that finishes
    // early leaves the user staring at a frozen screen. Loop until stopped.
    //
    // OWNS ITS OWN CLEANUP, on every exit path. There is no teardown hook and
    // there never will be one: if your part needs to be put back into a low
    // power mode, do it before each `return`, including the error returns.
    void (*run)(uint8_t addr7, const volatile bool* stop, LiveTestPublish publish, void* ctx);

    // Optional; NULL is fine and gets you a readable generic screen. Called
    // ONLY for the Running and Passed phases — "warming up" and "it dropped
    // off" are the app's screens, not yours, so every test looks the same
    // when nothing is being measured. Implement this when the reading itself
    // deserves a picture rather than a line of text. The canvas arrives
    // cleared and is yours entirely — 128x64, no title bar. `frame` advances
    // ~16 times a second, for animation.
    void (*draw)(Canvas* canvas, const LiveTestState* state, uint32_t frame);
} LiveTest;

// NULL when this chip has no live test, which is the normal case.
// chip_name may be NULL.
const LiveTest* live_test_for_chip(const char* chip_name);

size_t live_test_count(void);
const LiveTest* live_test_get(size_t index);

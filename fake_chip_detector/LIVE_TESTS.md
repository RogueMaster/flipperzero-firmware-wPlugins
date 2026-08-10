# Live tests

An ID register is one byte. A relabeller who can program that byte can make a cheap die claim
to be an expensive one, and every ID-based check in the world will agree with it.

A **live test** asks the harder question: does the part actually *do* what the number promises?
A magnetometer that tracks north, a rangefinder whose reading follows your hand — those cannot
be faked by a sticker or a fuse.

## How the app uses them

Live tests are not a menu item. The app offers one at exactly the moment it is worth running:
after a scan has identified a single part, and after you have confirmed it is what you ordered.
On that **ALL GOOD** screen, if a module exists for the chip that was found, `OK` runs it.

Chips with no live test are not treated as suspect and the screen does not change for them —
most parts have no test, and that is normal.

## Writing one

Each test is one file pair, `live_<part>.c` / `live_<part>.h`, plus a single line in the
registry in `live_test.c`. Nothing in the UI, the menus or the views has to change. The full
contract is in [`live_test.h`](live_test.h); this is the shape of it:

```c
static void mypart_run(uint8_t addr7, const volatile bool* stop,
                       LiveTestPublish publish, void* ctx) {
    while(!*stop) {
        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseRunning;
        snprintf(st.heading, sizeof(st.heading), "%u", reading);
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Breathe on it");
        publish(ctx, &st);
        furi_delay_ms(100);
    }
}

const LiveTest live_test_mypart = {
    .chip = "MYPART",              // exactly as spelled in chip_db.c
    .title = "MYPART live test",
    .offer = "Watch it react",     // the pitch on the ALL GOOD screen, <= 26 chars
    .run = mypart_run,
    .draw = NULL,                  // optional; NULL gets you a readable text screen
};
```

Then add it to the table in `live_test.c`:

```c
static const LiveTest* const live_tests[] = {
    &live_test_bno055,
    &live_test_vl6180x,
    &live_test_mypart,
};
```

and re-run `python tools/gen_supported_chips.py` from the repository root so the chip table
lists it.

### Rules the app holds tests to

- **`run` returns only when `*stop` is true.** A test that finishes early leaves the user
  staring at a frozen screen. Loop.
- **`run` owns its own cleanup, on every exit path.** There is no teardown hook. If your part
  has to be put back into a low-power mode, do it before every `return`, including the error
  ones. The BNO055 test parks the sensor in CONFIG whatever happened, because NDOF fusion burns
  ~12 mA and nobody is watching once the screen is gone.
- **`run` is called on its own thread** with the address the scan already found the part at.
  It never has to search the bus.
- **Never touch a view, a canvas or the view model.** Publish a `LiveTestState` and the app
  draws it.
- **Every register constant comes from the manufacturer's datasheet**, cited in a comment.
  This is the same rule the chip database is held to, for the same reason: a wrong constant
  makes the app accuse a genuine part of being counterfeit.
- **Never print a number the part did not stand behind.** If the sensor reports an error or an
  out-of-range status, say so — do not render the raw byte as if it were a measurement.

### Drawing

Leave `.draw` as `NULL` and you get a generic screen: title, the big `heading`, up to three
lines of text, and a progress row if you set `progress_max`. That is enough for most tests.

Set it and you own the whole 128×64 canvas for the **Running** and **Passed** phases only —
"warming up" and "it dropped off" stay the app's screens, so every test looks the same when
there is nothing to measure. `live_bno055.c` draws a compass and an animated figure-8;
`live_vl6180x.c` draws a distance ruler.

### Making it pass

`LiveTestPhasePassed` is the test's own success condition, and the app chimes once when it is
reached. Pick something the part cannot fake by holding still:

- **BNO055** — magnetometer calibration reaches level 3, which only happens if the sensor is
  genuinely tracking a magnetic field.
- **VL6180X** — the distance reading moves by 30 mm or more. A stuck register reads the same
  value forever; a working time-of-flight sensor cannot, once a hand comes near it.

## A test has to be runnable where it matters

The point of this app is to catch a counterfeit *before* you pay for it — standing at the
counter, at a pickup point, with the courier waiting. So a good live test is one you can run
right there:

- fast (seconds, not a warm-up cycle),
- reliable enough that a pass means something,
- and needing no props beyond your own hand and breath.

If a test needs a reference instrument, a heat source, a magnet, a vacuum or a long
calibration, it fails that bar. Skip the part rather than shipping a test nobody can actually
use in the moment that counts.

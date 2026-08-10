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
  ones. The BNO055 test parks the sensor in CONFIG when it leaves, because NDOF fusion burns
  ~12 mA and nobody is watching once the screen is gone.
- **Park only what you started.** Cleanup is not unconditional. An address that ACKs is not
  proof of which part answered — check the ID register first, and if it does not match, write
  nothing. The mode register you were about to restore belongs to a different chip's map, and
  a stray write to a stranger is a real way to brick someone's board.
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

**Prefer `.draw = NULL`.** The generic screen is not a consolation prize — it gives you a
title, the big `heading` with its `unit` set beside it, up to three lines of text, and one of
two indicators underneath:

- set `bar` and `bar_max` for a proportional bar — the right choice for anything that rises and
  falls, like humidity under your breath or lux under your hand. It makes "the reading is
  moving" obvious from across the room, which is the whole proof.
- set `progress` and `progress_max` for a row of boxes — the right choice for steps completed,
  like calibration levels.

Leave all four at zero and neither is drawn, so a `memset` struct needs no thought.

Setting `.draw` hands you the whole 128×64 canvas, but only for the **Running** and **Passed**
phases — "warming up" and "it dropped off" stay the app's screens, so every test looks the same
when there is nothing to measure. `live_bno055.c` draws a compass and an animated figure-8;
`live_vl6180x.c` draws a distance ruler.

Weigh that against portability before you reach for it. A custom `draw` is C compiled into the
app, so a test that has one can only ever ship *inside* the app. Tests that stay on the generic
screen describe themselves entirely in data, which is what lets them move somewhere else later.
If the picture genuinely carries the proof, draw it. If it is decoration, take the bar.

### Making it pass

`LiveTestPhasePassed` is the test's own success condition, and the app chimes once and draws a
tick when it is reached. Pick something the part cannot fake by holding still:

- **BNO055** — magnetometer calibration reaches level 3, which only happens if the sensor is
  genuinely tracking a magnetic field.
- **VL6180X** — the distance reading moves by 30 mm or more. A stuck register reads the same
  value forever; a working time-of-flight sensor cannot, once a hand comes near it.
- **BH1750** — the count rises above 30 in the light **and** falls to 3 or below under a hand.
  Both halves are needed: the dark floor of 0–3 counts is a printed datasheet figure, but a
  part stuck at zero would satisfy it forever without the light half.
- **DS3231** — the seconds register advances by exactly one, three times running. A frozen
  register never moves and a part improvising bytes jumps around.
- **MPU6050 / ADXL345** — gravity is seen on two *different* axes. A canned constant can look
  like 1 g on Z forever; it cannot hand the weight over to X when the board is tipped.
- **AHT / SHT** — humidity rises 15 points above the lowest reading seen.
- **MLX90614** — the object temperature runs 5 °C above the ambient the same part reports.

Two of these are worth copying for the shape rather than the numbers. The BH1750 test insists
on **both directions**, which is what stops a dead part passing by accident. The accelerometer
tests pass on a **change of which axis** holds gravity rather than on any absolute value, so
they need no knowledge of how the board is mounted and no trust in the zero-g offset.

### When a test cannot honestly pass

Sometimes there is no measurement to check, and the right answer is to say so. The OLED test is
the case in point: a display has no readback, so every command is acknowledged by a controller
whose panel may be stone dead. It blinks the screen and tells the user that only they can
judge, and it **never sets `LiveTestPhasePassed`** — a pass there would mean "the chip
acknowledged some bytes", dressed up as "your display works".

If your part is like that, do the same. A test that reports honestly beats a test that passes.

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

Every test in the app today passes it with nothing but a hand and a breath: breathe on it,
cover it, wave at it, tip it over, or just watch it tick.

### Parts deliberately left without a test

Skipping is a real answer, and these were all considered and dropped rather than overlooked:

| Part | Why not |
|---|---|
| INA219 / INA226 / INA260 | Reads zero until current flows. Needs a load and a supply. |
| CCS811 / ENS160 / SGP30 / SGP40 | The datasheets require a burn-in of minutes to days before a reading means anything. |
| SCD30 / SCD4x | CO2 needs a warm-up, and breath saturates it rather than proving anything. |
| AS5600 | Needs a diametrically magnetised magnet on an axle. |
| MCP23017 / PCF8574 | A GPIO expander proves itself by driving a pin. Needs a wire and something to watch. |
| MCP4725 / PCA9685 | Output is a voltage or a servo pulse. Needs a meter or a servo. |
| TCA9548A | A multiplexer proves itself only through devices behind it. |
| AT24Cxx | The honest test writes a byte, and that could destroy data the user cares about. |
| MLX90640 | A 768-pixel thermal image, on a 128x64 screen, over I2C. Possible, but not in seconds. |
| MAX17048 | Reports the battery it is soldered to; nothing to make it move. |
| VL53L0X | Ranging without ST's initialisation blob is not documented, and the blob is not in the datasheet. |

If you disagree about one of these, that is exactly the sort of contribution the module layout
exists for.

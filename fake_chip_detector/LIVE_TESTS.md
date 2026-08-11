# Live tests

An ID register is one byte. A relabeller who can program that byte can make a cheap die claim
to be an expensive one, and every ID-based check in the world will agree with it.

A **live test** asks the harder question: does the part actually *do* what the number promises?
A magnetometer that tracks north, a rangefinder whose reading follows your hand — those cannot
be faked by a sticker or a fuse.

## How the app uses them

The app offers one at exactly the moment it is worth running: after a scan has identified a
single part, and after you have confirmed it is what you ordered. On that **ALL GOOD** screen,
if a module exists for the chip that was found, `OK` runs it.

Chips with no live test are not treated as suspect and the screen does not change for them —
most parts have no test, and that is normal.

**Live tests** in the main menu lists every test the app can run — the ones built in, and any
found on the SD card — and runs any of them on demand, with no scan first. Launching one that
way probes the addresses the test declares and starts only if something answers there. If
nothing does, it says so and does nothing: running a test against an address with no device on
it would mean writing configuration registers to whatever else happens to be listening.

Only built-in tests are offered on the ALL GOOD screen. A test from the card is run
deliberately, from the browser, and is marked `SD` both in the list and on the test screen —
a built-in test was written against a datasheet and reviewed in this repository, and one from
the card is somebody else's code. Anyone reading a pass off that screen is entitled to know
which of the two they are looking at.

## Writing one

Each test is one file pair, `live_<part>.c` / `live_<part>.h`, plus a single line in the
registry in `live_test.c`. Nothing in the UI, the menus or the views has to change. The full
contract is in [`live_test.h`](live_test.h); this is the shape of it:

```c
static void mypart_run(const LiveTestEnv* env) {
    while(!*env->stop) {
        uint8_t reading = 0;
        env->i2c->read_reg(env->addr7, MYPART_REG_DATA, &reading, LIVE_TEST_TIMEOUT_MS);

        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseRunning;
        snprintf(st.heading, sizeof(st.heading), "%u", reading);
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Breathe on it");
        env->publish(env->ctx, &st);
        furi_delay_ms(100);
    }
}

const LiveTest live_test_mypart = {
    .chip = "MYPART",              // exactly as spelled in chip_db.c
    .title = "MYPART test",
    .offer = "Watch it react",     // the pitch on the ALL GOOD screen, <= 26 chars
    .addrs = {0x42, 0x43},         // every address the part can sit at
    .run = mypart_run,
    .draw = NULL,                  // optional; NULL gets you a readable text screen
};
```

Most of the shipped tests unpack `env` into locals on the first lines of `run` — `const uint8_t
addr7 = env->addr7;` and so on — which keeps the body below readable. Either style is fine.

**Why the bus arrives as `env->i2c` and not as a function you call by name.** It is the one
thing that lets the same `.c` file be either compiled into the app or built separately as a
`.fal` and dropped onto the SD card. A test never links against a symbol of this app, so nothing
has to be exported and there is no second version of the file to keep in step. Do not add
`#include "i2c_worker.h"` to a test — the moment you do, that test can only ever live inside the
app. `LIVE_TEST_TIMEOUT_MS` is in `live_test.h` for the same reason.

**`.addrs` is required, and listing an address the part cannot use is not a harmless mistake.**
It is what allows a test to be launched by hand with no scan behind it: the app probes these
addresses, and whatever answers gets written to. Copy them from the part's row in `chip_db.c`.

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
- **And it returns promptly once it is.** The app waits for `run` on the thread that draws every
  screen, because until it returns the test is still writing into the view model and, from a
  card, still executing inside a mapping about to be unmapped — so the wait cannot be abandoned.
  While it lasts nothing redraws and no key does anything: it looks exactly like the firmware
  has died, and past two seconds the app chirps to say the stall is real. Slice long delays and
  check the flag between the slices; every test here checks it at least five times a second.
- **`run` owns its own cleanup, on every exit path.** There is no teardown hook. If your part
  has to be put back into a low-power mode, do it before every `return`, including the error
  ones. The BNO055 test parks the sensor in CONFIG when it leaves, because NDOF fusion burns
  ~12 mA and nobody is watching once the screen is gone.
- **Park only what you started.** Cleanup is not unconditional. An address that ACKs is not
  proof of which part answered — check the ID register first, and if it does not match, write
  nothing. The mode register you were about to restore belongs to a different chip's map, and
  a stray write to a stranger is a real way to brick someone's board.
- **If you cannot identify the part and still have to write, say so on screen first.** Some
  parts leave no choice: a display has no readback at all, and an AHT's checksum cannot be read
  until after a measurement has been triggered. Both of those addresses are shared — the
  database puts a PCF8574A across 0x38-0x3F, covering the AHT and both OLED addresses, where
  command bytes land as output-port writes driving whatever is on its pins. Neither test can be
  made safe, so both spend three seconds saying "cannot identify this part, unplug now if it is
  not a display" before the first byte goes out, once per run rather than after every retry.
  Silence would be the app quietly taking a risk on the user's behalf.
- **`run` is called on its own thread** with the address the scan already found the part at.
  It never has to search the bus.
- **Never touch a view, a canvas or the view model.** Publish a `LiveTestState` and the app
  draws it.
- **Every register constant comes from the manufacturer's datasheet**, cited in a comment.
  This is the same rule the chip database is held to, for the same reason: a wrong constant
  makes the app accuse a genuine part of being counterfeit.
- **Never print a number the part did not stand behind.** If the sensor reports an error or an
  out-of-range status, say so — do not render the raw byte as if it were a measurement.
- **Write the configuration your arithmetic depends on. Do not assume the reset value.** If you
  divide by an LSB-per-unit constant, that constant is true for exactly one range setting, and
  the part in front of you keeps its registers until power is pulled — a board that some other
  firmware configured for a wider range arrives still configured that way. The ADXL345 is the
  sharp case: no soft reset at all, and a part left at ±16 g reads eight times low while the
  screen shows a perfectly plausible number. Write the range register even when the value you
  write *is* the reset value; that costs one transaction and means there is nothing to restore.
  Track it separately from your teardown flag, though — a write that fails halfway leaves the
  part changed but unmeasurable, and cleanup still has to undo exactly what landed.

### Drawing

**Prefer `.draw = NULL`.** The generic screen is not a consolation prize — it gives you a
title, the big `heading` with its `unit` set beside it, up to three lines of text, and one of
two indicators underneath:

- set `bar` and `bar_max` for a proportional bar — the right choice for anything that rises and
  falls, like humidity under your breath or lux under your hand. It makes "the reading is
  moving" obvious from across the room, which is the whole proof.
- set `progress` and `progress_max` for a row of boxes — the right choice for steps completed,
  like calibration levels. Eleven boxes is what the strip holds; ask for more and the app clamps
  it rather than drawing off the side of the screen.

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

### Telling "it fell off" apart from "that is not the part"

When a test stops getting readings there are two completely different reasons, and they call for
opposite advice. If nothing acknowledges the address any more, the part is gone: that is
`LiveTestPhaseLost`, and the screen says **Sensor dropped off!** and to check the wires. If
something is still there and it is not this part, the wiring is fine and the wrong module is
plugged in: that is `LiveTestPhaseWrongChip`, and the screen says **Wrong chip!** instead.

Folding both into one bool is the easy mistake, and it sends somebody to reseat a jumper that
was never loose. Return `LiveTestIdResult` from your identify helper rather than `bool`:

```c
static LiveTestIdResult mypart_identify(const LiveTestI2c* i2c, uint8_t addr7) {
    uint8_t id = 0;
    if(!live_test_read_id8(i2c, addr7, MYPART_REG_ID, &id))
        return live_test_id_unreadable(i2c, addr7);
    return id == MYPART_ID_VALUE ? LiveTestIdMatch : LiveTestIdMismatch;
}
```

**Identify with `live_test_read_id8`, not `i2c->read_reg`.** It reads the register twice and
believes it only if both reads agree. One byte is eight bits of evidence and this loop asks again
for as long as the screen is open, so a one-in-256 accident is not a tail risk — a VL6180X left on
the BNO055 test returned 0xA0 after about ninety seconds, and the test went on to write its mode
register and draw a heading off a part with no magnetometer in it. `live_test_read_id16` is the
same for a part that indexes its registers with sixteen bits.

**A failed read is not `LiveTestIdNoAnswer`** — that is what `live_test_id_unreadable` is for, and
returning the enumerator directly is a bug the app already shipped once. Reading an 8-bit-indexed
register is one write of the index byte followed by a repeated start, and a part that indexes its
registers with sixteen bits NACKs that whole exchange while sitting perfectly happily on the bus.
A real VL6180X at 0x29 does exactly that to the BNO055 test, which used to answer by telling the
user to go and check their wiring. The helper asks the address itself: still acknowledging means
something is there and does not speak this register map, which is the wrong part, not a wire.

Word the **Wrong chip!** lines so they hold for both — an ID that read back wrong and an ID that
could not be read at all. Naming a register the code may never have reached ("WHO_AM_I says…")
is how that goes wrong; the shipped tests say `0x29 answers, but not the way a BNO055 does.`

It matters most at the crowded addresses. 0x68 carries a DS3231 and ten IMUs, and 0x28/0x29 hold
a BNO055 and a VL6180X — at those, "something else is here" is the likeliest way a test fails.

Both phases keep retrying, so swapping the module is picked up without leaving the screen —
`LIVE_TEST_RETRY_MS` while something is merely missing, and `LIVE_TEST_WRONG_CHIP_RETRY_MS` once
the part has been identified as somebody else, because that does not recover without a hand. A
part with no ID register cannot make this distinction and should not pretend to.

### When a test cannot honestly pass

Sometimes there is no measurement to check, and the right answer is to say so. The OLED test is
the case in point: a display has no readback, so every command is acknowledged by a controller
whose panel may be stone dead. It blinks the screen and tells the user that only they can
judge, and it **never sets `LiveTestPhasePassed`** — a pass there would mean "the chip
acknowledged some bytes", dressed up as "your display works".

If your part is like that, do the same. A test that reports honestly beats a test that passes.

## Shipping one as a plugin

A test does not have to be merged here to be useful. Built as a `.fal` and copied to the card,
it appears in the browser and runs like any other — no rebuild of the app, no pull request, no
waiting for anyone.

Start from [`test_plugin_template/`](../test_plugin_template) in the repository root. It is a
complete working test, not a snippet: copy the folder, change the registers, the pass condition
and the strings, then

```
cd your_folder
ufbt                       # produces dist/live_test_yourpart.fal
```

and copy the `.fal` to the Flipper at

```
SD Card/apps_data/fake_chip_detector/tests/
```

The app creates that folder the first time it opens the browser, so run it once before going
looking for the directory.

The only difference between a plugin and a built-in test is fourteen lines at the bottom of the
file — a `FlipperAppPluginDescriptor` and the entry point named in `application.fam`. Delete
them and the same file compiles into the app; add them and it compiles out. Nothing in the test
itself changes, which is the entire reason the bus arrives as `env->i2c`.

Two things about the manifest that will otherwise cost you an evening:

- **`requires[0]` is read as the parent application and must name a manifest ufbt has loaded**,
  and ufbt loads exactly one file — yours. The template therefore declares a stub
  `fake_chip_detector` of type `METAPACKAGE` in the same `application.fam`. `METAPACKAGE` is
  the right type because ufbt does not build it, so the stub costs nothing and produces
  nothing. Without it the build stops at *"Missing application manifest"*.
- **A plugin must not set `stack_size`.** It runs on a thread the app already owns.

The app refuses to run a file rather than guess, and says which of these it was: not built as a
plugin, built for a different app, **built against another version of the contract**, will not
load at all, or declares no address. That third one is the one to expect after a release: the
descriptor carries `LIVE_TEST_PLUGIN_API_VERSION`, and it is bumped whenever `LiveTestState`,
`LiveTestEnv`, the bus table or the descriptor changes shape. A plugin compiled against the old
shape would otherwise read past the end of a struct and publish whatever it found there as a
measurement, which is precisely the failure this app exists to prevent. Rebuild against the
current header and it will load.

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

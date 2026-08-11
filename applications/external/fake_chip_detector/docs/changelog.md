# Changelog

## 0.7 — beta

- First public release, and deliberately not called 1.0. Everything here is built and reviewed,
  but only one part has been driven end to end on real silicon: a VL6180X. Twelve of the
  thirteen live tests have never met the chip they are written for, and the official-firmware
  and Momentum builds compile but have not been run by anybody. The version will go up when
  that changes — feedback is the thing standing between this and a 1.0.
- Identifies I2C chips by their factory ID registers and reports whether the silicon really is
  the part it claims to be. 80 chips in the database, each with a plain-language description of
  what it does.
- Asks whether the part found is the one the user ordered, since the app cannot see the label,
  and turns a "no" into a report written for a seller or a courier.
- Reports are readable on screen and saved to /ext/apps_data/fake_chip_detector/ as evidence:
  plain statement first, an explanation of why a factory ID cannot be forged, technical detail
  last.
- Supports 8-bit and 16-bit register indices and 16-bit values, covering ST time-of-flight
  parts and TI power monitors alongside the usual WHO_AM_I chips.
- Resolves address collisions by probing every candidate that shares an address.
- Never overclaims: chips without an ID register are reported as present, not genuine; a device
  matching nothing is unidentified rather than fake; a failed read is shown as a failure.
- Wiring guide with live per-line detection, a stray-pull-up sweep for the wrong pins, and
  SDA/SCL short detection.
- Live tests: after a part checks out, the app offers to prove it actually works rather than
  merely identifying itself. Thirteen are listed, and every test can be run standing at a
  pickup counter before paying, with nothing but a hand and a breath: breathe on an AHT10/20 or
  SHT3x/4x, cover a BH1750, tip an MPU6050/6500/9250 or ADXL345, wave at an APDS9960, point an
  MLX90614 at your palm, watch a DS3231 tick, make an SSD1306 blink, rotate a BNO055 through a
  figure-8, or hold a hand in front of a VL6180X. Each test is a self-contained module, so
  adding one for another part touches nothing else.
- Live tests matter most for the chips with no ID register, where the app can otherwise only
  report presence — six of them cover exactly those parts, and such a part is now reported as
  IT ANSWERS rather than being told it is the real deal.
- The DS3231 test writes nothing whatsoever, so it cannot disturb a clock already keeping time.
- The OLED test never claims a pass: a display has no readback, so the verdict is the user's.
- A Live tests screen listing every test the app can run, built in or found on the SD card, and
  running any of them on demand without scanning first. Launching one probes the addresses that
  test declares and refuses to start if nothing answers, rather than writing configuration
  registers to whatever else is on the bus.
- Tests can be written by anyone and dropped onto the card as .fal plugins in
  apps_data/fake_chip_detector/tests/ — no rebuild of the app. The same source file compiles
  either into the app or out of it, because a test is handed the bus as a table of pointers
  instead of calling the app by name. A complete template to copy ships in the repository.
- Tests loaded from the card are marked SD in the list and on the test screen: a built-in test
  was written against a datasheet and reviewed in the repository, and one from the card is
  somebody else's code. A plugin built against an older version of the contract is refused with
  a reason rather than run.
- The two tests that have to write to a part they cannot identify now say so before they do.
  A display has no readback, and an AHT's checksum cannot be read until a measurement has been
  triggered — and the database puts a PCF8574A across 0x38-0x3F, covering both OLED addresses
  and the AHT, where those bytes would drive a GPIO expander's pins. Three seconds of "cannot
  identify this part, unplug now if it is not a display" beats quietly taking that risk on
  somebody's behalf.
- The AHT test verifies the AHT20's checksum rather than trusting the status byte. Address 0x38
  is shared with a VEML6070 and sits inside the PCF8574A's range, and an all-ones answer has the
  calibration bit set — so the status byte on its own says almost nothing. A checksum that
  verifies identifies an AHT20; when none does the screen says so, because that is consistent
  with an AHT10 and equally consistent with something else at the same address.
- A live test tells "the sensor fell off" apart from "that is not the part". If nothing
  acknowledges the address any more the screen says the sensor dropped off and to check the
  wires; if something is still there and it is not this part it says WRONG CHIP instead, because
  the wiring is fine and the advice would otherwise send somebody to reseat a jumper that was
  never loose. It matters at the crowded addresses: 0x68 carries a DS3231 and ten IMUs, 0x28/0x29
  a BNO055 and a VL6180X.
- An ID register is read twice and believed only if both reads agree, and the test backs off once
  it has decided the part is somebody else. One byte is eight bits of evidence and a test screen
  asks again for as long as it is open, so a one-in-256 accident is not a tail risk: a VL6180X
  left on the BNO055 test came back 0xA0 after a minute and a half, and the test wrote its mode
  register and drew a heading off a part with no magnetometer in it.
- The success chime cannot fire faster than once every three seconds. It re-arms when a reading
  falls back out of its success state, which is right, but a reading flapping on its threshold
  turned that into a machine gun — and a test can arrive on somebody's SD card, so the buzzer is
  the app's to bound rather than each test's.
- Which of the two it is comes from asking the address, not from whether a register read worked.
  Reading an 8-bit-indexed register is one write of the index byte and then a repeated start, and
  a part that indexes its registers with sixteen bits refuses that outright while sitting
  perfectly happily on the bus — so a genuine VL6180X at 0x29 made the BNO055 test tell a user to
  check wiring that was already correct. If the address still answers, something is there and
  does not speak this register map, and that is the wrong part rather than a loose wire.
- The BH1750 test warns before its blind write too. It has no ID register at all and its
  addresses are 0x23 and 0x5C, where the database puts a PCF8574 and an MCP23017 — the single
  trigger byte would land there as an output-port write, repeated every 180 ms.
- The question "is this what you bought?" is asked again for every scan. Its answer used to
  survive into the next one, so a genuine sensor scanned after a rejected one was called NOT
  YOURS without ever being asked about, and the report was saved as disputed.
- Nothing on a card is taken at its word. A plugin's own name, title and offer must end inside
  the room there is for them or it is refused with a reason: copying an unterminated string
  walked off the end of the mapped file while merely listing the folder, and the resulting crash
  left no way to delete the file from inside the app. The state a running test publishes is
  terminated by the app before anything draws it, and a step count wider than the screen is
  clamped rather than wrapped. A test that ignores its stop flag can still stall the app on the
  way out — the worker is writing into the view model and, from a card, running inside a mapping
  about to be unmapped, so the wait cannot be abandoned — and the screen cannot say so, because
  the thread that draws it is the one waiting. Past two seconds it chirps instead, which is the
  one signal that still gets out.
- A 1-Wire scratchpad of nothing but zeros is rejected rather than reported as 0.0 °C. CRC8 over
  eight zero bytes is zero, so a transfer that collapsed halfway passed the checksum and was
  drawn as a working sensor.
- The success chime is latched only once it has actually played. Latching a chime the rate limit
  had just swallowed meant that success was never announced at all.
- Bars are drawn in a strip of their own at the foot of the screen. Laid out after the text they
  could not fit under a heading and two lines, so the AHT's humidity bar was computed on every
  poll and silently never shown.
- The saved-reports list keeps the newest thirty-two and says how many older ones remain. Reading
  the directory in its own order dropped exactly the report that had just been saved.
- Browsable list of every known chip.
- Melody, LED and vibration feedback, each switchable in Settings.

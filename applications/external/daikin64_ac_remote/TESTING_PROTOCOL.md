# Compatibility Test Protocol

Use this checklist before marking a Daikin indoor unit as compatible. A unit should only be listed as verified when the tested functions visibly change the AC behavior, not just when the indoor unit beeps.

## Test Record

Create one test record per indoor unit.

```text
Date:
Tester:
Country/region:

Indoor unit model:
Outdoor unit model, if known:
Original remote model:
Original remote photo available: yes/no

Flipper Zero firmware version:
Daikin64 AC Remote app version/commit:
Install path:

Test result:
  Verified / Partial / Failed

Notes:
```

## Setup

1. Install the latest `daikin64_ac_remote.fap` on the Flipper.
2. Set the Flipper clock to the correct local time.
3. Stand 1 to 3 meters from the indoor unit with the Flipper IR side aimed at the AC receiver.
4. Start with the AC off.
5. Keep the original Daikin remote nearby so you can recover the AC state if a command leaves it in an unexpected mode.
6. Wait at least 5 seconds between commands that change mode, temperature, or timers.

## Required Tests

Mark each row as `Pass`, `Fail`, or `Not supported by unit`.

```text
Power
[ ] Power ON from off
[ ] Power OFF from on

Mode
[ ] Cool mode
[ ] Heat mode
[ ] Dry mode
[ ] Fan mode
[ ] Auto mode

Temperature
[ ] 16 C
[ ] 20 C
[ ] 24 C
[ ] 27 C
[ ] 30 C

Fan speed
[ ] Auto
[ ] Low
[ ] Medium
[ ] High
[ ] Turbo / Powerful
[ ] Quiet, if supported

Swing
[ ] Swing ON
[ ] Swing OFF

Functions
[ ] Sleep, if supported
[ ] LED/display, if supported
[ ] Clock command does not break current AC state

Timers
[ ] Current time is shown correctly in the app
[ ] ON timer can be enabled at a :00 time
[ ] ON timer can be enabled at a :30 time
[ ] ON timer can be cleared
[ ] OFF timer can be enabled at a :00 time
[ ] OFF timer can be enabled at a :30 time
[ ] OFF timer can be cleared

Favorites
[ ] Favorite 1 sends the saved state
[ ] Favorite 2 sends the saved state
[ ] Favorite 3 sends the saved state
[ ] Favorite 4 sends the saved state
```

## How To Verify Behavior

Use visible or physical confirmation where possible:

- Power: indoor unit turns on or off, not just a beep.
- Mode: icon or LED behavior changes on units with a display; otherwise check whether the air output changes after a few minutes.
- Temperature: verify with the original remote or unit display if available. If the AC has no display, mark temperature as `Pass by response only` unless you can confirm setpoint behavior.
- Fan: airflow changes within a few seconds.
- Swing: vane movement starts or stops.
- Turbo/Quiet: airflow changes noticeably.
- Timer: the original remote or indoor unit indicator confirms timer state, if available.

If the AC only beeps but behavior does not change, mark the test as `Fail` or `Unconfirmed`, not `Pass`.

## Compatibility Levels

Use these exact labels in the README or release notes.

```text
Verified:
  Power, modes, temperature, fan, and swing all pass on real hardware.

Partial:
  Power and basic cooling/heating pass, but one or more features are missing, unconfirmed, or unit-specific.

Failed:
  The indoor unit does not respond correctly to generated Daikin64 packets.

Unconfirmed:
  The unit beeps, but behavior could not be verified.
```

## Compatibility Entry Template

```text
| Indoor unit | Original remote | Region | Result | Tested app version | Notes |
|-------------|-----------------|--------|--------|--------------------|-------|
| FTXSxx...   | APGS02          | EU     | Verified | commit/hash or v0.x | Power, Cool, Heat, Fan, Swing OK |
```

## Failure Report Template

When a unit fails, save the details below so the protocol can be adjusted safely.

```text
Indoor unit model:
Original remote model:
Which command failed:
What the AC did:
What the app showed:
Distance/angle from AC:
Did the AC beep: yes/no
Does the original remote command work from the same position: yes/no

Capture from original remote, if available:
Capture from Flipper-generated signal, if available:
Extra notes:
```

## Capture Guidance

For protocol fixes, capture both the original remote and the Flipper-generated command where possible.

Recommended captures:

```text
Power ON
Power OFF
Cool 27 C Fan Auto Swing Off
Heat 27 C Fan Auto Swing Off
Dry 27 C
Fan mode
Swing toggle
Turbo / Powerful
Quiet
ON timer at 14:00
ON timer at 14:30
OFF timer at 18:00
OFF timer at 18:30
Timer clear/cancel
```

Name files with the model and command, for example:

```text
FTXS35_APGS02_cool_27_auto.ir
FTXS35_APGS02_on_timer_1430.ir
```

Do not publish captures that include personal information in file names or comments.

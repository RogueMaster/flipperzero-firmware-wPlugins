# ARDF Foxhunting

`ARDF Foxhunting` turns Morse Flipper into a clock-scheduled fox transmitter. It can use the built-in CC1101 radio, provide keying signals on GPIO, and produce optional local audio.

Choose the working frequency under `Flipper Radio → Frequency` before opening the fox settings. The selected frequency is inherited.

## Settings

- `Mode`: `Standard`, `Sprint`, or `Custom`.
- `Modulation`: carrier-keyed `CW` or tone-modulated `CWFM`.
- `Message`: `1 - MOE` through `5 - MO5`, plus `S` and `MO`.
- `Custom`: an identifier of up to five letters, digits, or spaces, used when `Mode` is `Custom`.
- `Custom interval`: spacing between Custom transmissions.
- `Light assistance`: wakes the backlight while transmitting.
- `Audio output`: enables local Morse audio through the configured audio path.
- `WPM`: 8-30 words per minute.

Select `Start ARDF Fox` and check the clock. Press `OK` if it is correct. To change it, select a field, press `OK`, adjust it with `Up` or `Down`, then press `OK` to set it and `OK` again to start.

The run screen shows the clock, a countdown, and a progress bar. Press `Back` to stop.

## Transmit Windows

`Standard` uses a five-minute cycle divided into five one-minute windows. `Sprint` uses a one-minute cycle divided into five twelve-second windows.

Foxes 1-5 occupy those windows in order. During its window, the selected identifier repeats with normal word spacing while another complete repetition will still fit.

If the fox starts during its assigned window, it joins immediately if one complete identifier still fits. Otherwise, it waits for the next assigned window. Later transmissions remain aligned to the Flipper clock.

`Custom` starts its first transmission immediately. Later transmissions follow an interval grid anchored to the next whole minute. If a transmission overlaps a scheduled point, that occurrence is skipped without shifting the grid.

`S` and `MO` are continuous, unnumbered beacons and begin when the run starts.

## The 250 ms Lead-In

For a known scheduled transmission, the app prepares the radio and raises `P16` 250 ms before the window opens. The Morse begins at the window boundary; `P15`, local audio, and the mark indication remain idle during the lead-in.

An immediate Custom CW transmission begins at once. In CWFM mode, the app establishes a quiet carrier for 250 ms before sending the first 700 Hz mark.

`P15` follows the Morse marks. When local audio is enabled and the global audio path is `P2 (HD)`, the same keyed tone appears on `P2 / PA7`.

## Slow And Fast Sprint Foxes

Slow and fast Sprint foxes use the same identifiers and clock phases. They are distinguished by frequency and speed, so no separate Fast setting is needed.

A typical pairing is:

- Slow fox: slow-set frequency at `10 WPM`.
- Fast fox: fast-set frequency at `15 WPM`.

One Flipper acts as one transmitter; it does not switch between the two frequency sets.

## CW, CWFM And Light Assistance

`CW` keys the built-in carrier on and off. It is suitable for another Flipper, an SDR, or a receiver capable of receiving carrier-keyed CW.

`CWFM` sends a fixed 700 Hz tone over FM, with a quiet carrier between marks. Compatible FM handhelds can therefore hear the Morse directly.

For an external 2 m FM transmitter, enable `Audio output` and select `P2 (HD)` globally. `P2 / PA7` then supplies the keyed audio tone. `P15` remains available as the mark-level key output for equipment which accepts one.

`Light assistance` wakes the backlight when transmission begins and releases it afterwards. It does not alter RF or timing.

The transmitter is deliberately configurable, so a selected combination of mode, frequency, speed, and transmit window may fall outside an official IARU ARDF event format.

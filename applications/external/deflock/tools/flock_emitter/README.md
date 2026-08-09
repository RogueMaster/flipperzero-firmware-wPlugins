# Flock Emitter — bench target for FlipDeFlock

> **This board transmits.**
>
> Flock / ALPR detection in FlipDeFlock is passive. This is a separate tool that
> exists only so the detector has something to detect. It is not part of the
> `.fap`, it never ships to a Flipper, and nothing in the app depends on it.
>
> **Run it on a bench, on hardware you own, and switch it off when you are
> done.** It impersonates surveillance hardware. Leaving it running somewhere
> public would pollute other researchers' captures and put fake Flock
> signatures into datasets that people rely on. Don't.

## Why this exists

Every change to FlipDeFlock since v0.20 has been compile-verified in CI and
never validated against a radio. The host suite proves the scoring maths and
the parser; the CI build proves it links. Neither proves that a frame on the air
becomes the right row on the Flipper's screen.

Closing that needs a transmitter, and driving to a real ALPR pole is a poor test
loop: you cannot ask a real camera to emit a *Possible*, then a *Likely*, then a
*CONFIRMED*, then a known false positive, on a three-second rotation. This board
can, so it does.

## What it emits

Each identity is picked to land on exactly one branch of the ladder in
`helpers/flock_db.c` and `helpers/esp_parser.c` — **including the branches that
must not fire**. If the Flipper shows anything other than the Expect column,
that is a real bug worth chasing.

### Wi-Fi (rotates every 3 s, channel 6)

| # | Identity | Expect on the Flipper |
|---|----------|----------------------|
| 0 | Flock OUI `b4:1e:52`, beacon, ordinary SSID | `p` Possible — OUI only |
| 1 | Flock OUI, wildcard probe request | `L` Likely — OUI + probe behaviour |
| 2 | SSID `Flock-A1B2C3` | `!` CONFIRMED — anchored provisioning name |
| 3 | SSID `test_flck` | `!` CONFIRMED — the CVE-2025-59409 dev SSID |
| 4 | SSID `Flock-Guest` | `L` Likely — **must never be CONFIRMED** |
| 5 | SoundThinking OUI `d4:11:d6` | `ST` tag, acoustic class |
| 6 | Flock OUI, beacon, **zero-length** SSID IE | `[hid]` tag, confidence rung unchanged |
| 7 | Flock OUI, beacon, **all-NUL** SSID IE | `[hid]` tag, confidence rung unchanged |

**Identity 4 is the one to watch, and it is not hypothetical.** Through v0.46 the
companion substring-matched `flock-`, the Flipper took its score verbatim, and
`Flock-Guest` really did display as CONFIRMED. v0.47 anchors both sides on
`^flock-[0-9a-f]{6}$` *and* has the Flipper re-derive any claimed CONFIRMED from
the SSID it was sent. If the bench ever shows `Flock-Guest` as CONFIRMED again,
one of those two guards regressed.

That bug shipped with green host tests, because `test_flock_db.c` was exercising a
function no companion-path code called. **This rig would have caught it on the
first run.** It is the argument for actually running it.

**Identities 6 and 7** must raise the `[hid]` tag *without* moving the rung.
Hidden-SSID beaconing is reported as an observation, never scored — see the note
in `helpers/esp_parser.c` for why. They use the two different legal encodings
because the companion detects them on **different code paths** (a zero length
versus a byte scan). The host tests cover the `hid=1` token, not the detection,
so this sketch is the only thing that ever runs either branch.

### BLE (rotates every 1.5 s)

| # | Identity | Expect |
|---|----------|--------|
| 0 | mfg `0x09C8` + name `Penguin-1234567890` | FLOCK, serial `TN72023022000771` decoded |
| 1 | mfg `0x09C8` + name `FS Ext Battery` | FLOCK, **no** serial — that string is a model label |
| 2 | Raven GATT service `0x3100` | `Flock Raven (audio)` |

## Build and flash

Any classic ESP32 (WROOM/WROVER). 2.4 GHz only, which is all the FlipDeFlock
companion scans, so there is nothing here it cannot reach.

```sh
arduino-cli core install esp32:esp32@2.0.17
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app tools/flock_emitter
arduino-cli upload  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app -p <port> tools/flock_emitter
```

It prints every rotation to serial at 115200 with the expected result, so you
can watch the two sides side by side:

```
[WIFI] #4 80:30:49:00:00:05 ssid=Flock-Guest    -> expect: L Likely -- MUST NOT be CONFIRMED (B6)
[BLE ] #0 name=Penguin-1234567890   -> expect: FLOCK, serial TN72023022000771
```

## Running the bench test

1. Flash this sketch to a spare ESP32 and power it up.
2. Power the Flipper with its own companion board and open **Flock / ALPR
   Detect**. Keep the two boards a metre or so apart — right on top of each
   other saturates the receiver.
3. Let it cycle for a couple of minutes. All eight Wi-Fi identities and all
   three BLE ones should appear as separate rows.
4. Check each row against the Expect column, then open the detail screen for
   the SoundThinking and hidden-SSID rows and confirm the labels.
5. Turn the emitter off.

Until that run has actually happened, describe the work as compile-verified,
not field-proven.

## Notes on the injection

Beacons are hand-built and sent with `esp_wifi_80211_tx()` rather than via
`softAP`, for two reasons: the source MAC has to be an arbitrary Flock OUI, and
`softAP`'s hidden mode cannot produce the exact SSID-IE shapes we need to test.

Probe requests come from `esp_wifi_scan_start()` after setting the STA interface
MAC, because the IDF refuses to inject a probe request with a foreign address.

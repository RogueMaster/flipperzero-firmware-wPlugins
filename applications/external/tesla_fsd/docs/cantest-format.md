# CAN Test Profiles (`.cantest`)

A `.cantest` file is a plain text list of CAN frames you want the Flipper to
send. You edit it on a computer, drop it on the SD card, and run it from the
app. It closes the loop:

```
CAN Capture  ->  edit a .cantest  ->  Send Test (parked)  ->  result log  ->  GitHub issue
```

A line copied straight out of a capture log can be tweaked and replayed, so the
capture format and the send format are deliberately the same.

## File format

One frame per line. `#` starts a comment.

```
# Name: poke the right stalk
229#00112233445566AA  repeat=20  delay=100
3FD#1000000000004000
(1.234000) can0 370#0000000000000000      # a raw capture line also works
```

| Token | Meaning | Default |
|-------|---------|---------|
| `ID#DATA` | hex CAN id, `#`, then 0–8 hex data bytes | required |
| `repeat=N` | send the frame N times | 1 |
| `delay=N` | milliseconds between sends (`delay=100ms` also accepted) | 50 |
| `# Name: ...` | profile name shown on the Flipper | filename |

A leading `(timestamp) bus ` candump prefix is ignored, so you can paste a line
straight from `apps_data/tesla_mod/captures/*.log`.

## Using it on the Flipper

1. Edit a `.cantest` on your computer (see [`examples/example.cantest`](../examples/example.cantest)).
2. Copy it to the SD card: `apps_data/tesla_mod/tests/`.
3. App → **Send Test [BETA]** → pick the file.
4. It opens in **DRY RUN** — it shows the frames and the live safety state but
   transmits nothing.
5. Press **ARM SEND** to transmit. This only works when:
   - Mode is **Active** or **Service** (not Listen-Only), and
   - a fresh `DI_speed` (`0x257`) frame shows the car **stationary**.
   The interlock is re-checked before every frame, so if the car starts moving
   the run aborts. If sending is blocked, the screen shows why.
6. The run is written to `apps_data/tesla_mod/tests/results/send_<n>.log`.
   Attach it to a GitHub issue to report what happened.

## Safety

- **The parked/stationary interlock prevents injection on a moving car. It does
  not make an arbitrary frame safe.** You are responsible for the frames you
  send. Test parked, with nobody near the car, and only run profiles you
  understand.
- Sending is **fail-closed**: if the app has not seen a speed frame (e.g. the
  bus you tapped doesn't carry `0x257`), it will not transmit at all.
- Dry-run is the default; transmitting always requires a deliberate ARM.

## Cracking a checksum from a capture

Frames with a rolling counter + checksum (e.g. `0x485`, `0x229`) need the right
checksum to be accepted. Capture them and recover the parameters:

```
python3 tools/tesla_crc_cracker.py --id 0x485 apps_data/tesla_mod/captures/cap_*.log
```

Capture from the **direct vehicle bus** (e.g. Tesla X179 pin 9/10 or OBD-II),
not a gateway-forwarded subset bus — forwarded frames may be re-emitted without
the original counter/CRC.

# Flipper Share IR — hardware & host test tools

Helper scripts used to bring up and validate Flipper Share IR on two real Flipper Zeros.
They are kept here for future debugging and tuning; they are not part of the app.

## Host unit test (no hardware)

`modem_test.c` round-trips random/pathological payloads through the pure modem
codec (`../ir_modem.c`) with simulated TSOP bias/jitter. Run:

```sh
cc -std=c11 -Wall -Wextra -I.. modem_test.c ../ir_modem.c -o /tmp/modem_test && /tmp/modem_test
```

Expected: 0 failures on the clean and realistic-distortion (bias 60 µs, jitter
80 µs) passes. The "harsh" pass (±220 µs > STEP/2) is expected to fail — it just
maps the usable error budget.

## On-device tools (two Flippers over USB)

`flip.py` is a small Flipper CLI driver over the USB CDC serial port. It uses a
background reader thread so a stray `log` stream can't wedge it, and it sends
Ctrl-C on connect to leave any leftover log mode.

Device ports are NOT committed: copy `ports_local.example.py` to `ports_local.py`
(gitignored) and set `PORT_RX` / `PORT_TX` to your two `/dev/cu.usbmodemflip_*`
ports (or export `IR_RX_PORT` / `IR_TX_PORT`).

- `capture_logs.py [seconds]` — read-only live `log` capture from **both** devices,
  filtered to Flipper Share IR / transport lines. Start it, then drive the transfer by
  hand and watch the output.
- `run_send.py` — drive the sender into the send scene and observe the announce log.
- `run_transfer.py [seconds]` — full end-to-end attempt (Flipper_First=RX, Flipper_Second=TX) that
  also verifies the received file's MD5 via `storage md5`.

Upload the app to a specific device with ufbt (the plain env var is ignored by
`launch`, pass it as an fbt arg too):

```sh
FLIP_PORT=/dev/cu.usbmodemflip_Flipper_First1 ufbt launch FLIP_PORT=/dev/cu.usbmodemflip_Flipper_First1
```

Push a test file with the SDK uploader (more reliable than the CLI write_chunk):

```sh
python3 ~/.ufbt/current/scripts/storage.py -p /dev/cu.usbmodemflip_Flipper_Second1 send tx.bin /ext/ir_share_tx.bin
```

### Caveat: GUI navigation

Injecting input over the CLI (`input send <key> <type>`) proved unreliable for
driving the app's menus/file browser in this setup — the menu did not respond.
The reliable procedure is: **navigate by hand on the devices** (Receive on one,
Send → pick a file → OK on the other) while `capture_logs.py` observes both logs.
The app verifies the MD5 itself and logs `MD5 match, file received successfully`
or a hash failure, which is the definitive end-to-end check.

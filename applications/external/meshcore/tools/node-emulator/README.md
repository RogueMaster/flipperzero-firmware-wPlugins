# Node emulator

A fake MeshCore companion node, so the Flipper app and `meshlog.py` can be
exercised without a radio on the bench.

It is a **test fixture**, not a firmware simulator: it answers companion
protocol requests and emits plausible unsolicited events. It does not model the
mesh, and it is not a conformance tester for real firmware.

## Why it exists

The nodes were still in transit. Everything above the wire — framing, request
and reply layouts, the Logger's CSV pipeline, the Messenger's mailbox — can be
proved wrong in software, and finding those problems against a fixture is very
much cheaper than finding them in a field with a laptop in the rain.

## Run it

```sh
python tools/node-emulator/meshcore_emulator.py --tcp 127.0.0.1:5000
```

Then point anything that speaks the companion protocol at it:

```sh
# The user's field logger, unmodified
python meshlog.py --tcp 127.0.0.1:5000 --outdir ./meshtest

# Or the reference client directly
python -c "
import asyncio
from meshcore import MeshCore
async def main():
    mc = await MeshCore.create_tcp('127.0.0.1', 5000)
    print((await mc.commands.send_appstart()).payload)
    await mc.disconnect()
asyncio.run(main())
"
```

Options:

| Flag | Meaning |
| --- | --- |
| `--tcp HOST:PORT` | listen for a client over TCP (no hardware at all) |
| `--serial PORT` | pretend to be a node on a real wire *(stage 3)* |
| `--model {t114,v4}` | which board to claim to be — exercises FAP auto-detect |
| `--name NAME` | node name reported in self-info |
| `--contacts A,B,C` | the fake mesh peers to hand out |
| `-v` | log every frame in and out |

## Is the protocol actually right?

Yes, and it is checked rather than asserted. Run:

```sh
python tools/node-emulator/tests/gate_stage1.py
```

This starts the emulator and drives it with the **reference `meshcore` Python
library** — the same one `meshlog.py` imports. If an independent implementation
decodes our frames into sane values, the layouts are right.

```
15 checks, 0 failures
```

Every byte layout came from a primary source, never from guesswork:

- framing and command codes — `docs/companion_protocol.md` and
  `examples/companion_radio/MyMesh.cpp` in `meshcore-dev/MeshCore`
- reply layouts — cross-checked against the reference Python client's parser

### Framing, the part that is easy to get backwards

```
frame = [type:u8][len:u16 little-endian][payload:len]
type 0x3C '<'   app  -> radio    (what a client sends us)
type 0x3E '>'   radio -> app     (what we send back)
```

Confirmed three independent ways: the firmware's `ArduinoSerialInterface`
(`writeFrame` emits `'>'`, `checkRecvFrame` waits for `'<'`), the reference
Python client (sends `b"\x3c"`, scans for `0x3e`), and the `meshcore_c`
constants. Note this is the reverse of the intuitive reading.

## Two things that look like bugs but are not

**Frequency and bandwidth use different scales.** On the wire frequency is in
kHz (`869525` = 869.525 MHz) but bandwidth is in Hz (`250000` = 250 kHz). The
reference client encodes `set_radio` as `freq * 1000` from MHz and `bw * 1000`
from kHz, which is where the mismatch comes from. The emulator reproduces it
because a fixture that quietly normalises would hide the bug it exists to find.

**`meshlog.py`'s `batt_pct` column holds millivolts.** The firmware's
`CMD_GET_BATT_AND_STORAGE` returns `board.getBattMilliVolts()`, and the
reference client surfaces that as `level`, which `meshlog.py` writes into a
column named `batt_pct`. So expect roughly `4050`, not `85`. Nothing here is
wrong; the column name is just optimistic.

Related: `meshlog.py` looks for `rx_total` / `tx_total` in the packet stats,
while the reference client names those fields `recv` and `sent`, so those two
telemetry columns come out empty. That is a `meshlog.py` detail, left alone
deliberately — the fixture reproduces what real firmware sends.

## Status

- **Stage 1 — done, gated.** Framing plus self-info, device query, battery,
  radio and packet stats, and the contact list.
- **Stage 2** — unsolicited events (adverts, RX log with drifting SNR, incoming
  messages) and ping with configurable loss.
- **Stage 3** — serial transport, `--scenario`, and the devboard bridge.

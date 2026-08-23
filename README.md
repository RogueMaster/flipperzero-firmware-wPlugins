# Car TPMS sensors on a Flipper Zero

Reads the tyre pressure sensors in car wheels — 39 protocols, ported from
[rtl_433](https://github.com/merbanan/rtl_433) — on the Flipper itself,
and optionally streams what it decodes to a computer over USB.

Two parts:

* **`flipper/tpms_bridge`** — the app for the Flipper Zero. Configures the
  CC1101, listens on air, decodes every protocol it knows at once and
  shows the readings **on its own screen**: a sensor list with signal
  levels and a detail screen for each one. The same frames are printed as
  line-delimited JSON on the CLI, which means over USB.
* **`host/`** — the desktop application in Python and PySide6: sensor
  table, pressure and temperature charts, CSV export, offline parsing of
  captures.

## Which sensors work

**Verified against real hardware: the Renault group sensor only.** A
407003VU0B was read on the bench and on a moving car with all four wheels
reporting at once. That part belongs to the Renault-Nissan family, and
rtl_433 reports the same frame format on Renault Clio, Captur and Zoe (the
Zoe ships with the 407004CB0B) and possibly on Dacia Sandero.

**Every other protocol is verified against rtl_433, not against a
sensor** — see [Tests](#tests) for how. Between them they cover the OEM
sensors of Renault, PSA, Ford, Toyota, Hyundai, Kia, Honda, BMW, Mini,
Mercedes, Porsche, Audi, Fiat, Mazda, Chrysler, Jeep, Opel, Saab,
Vauxhall, Chevrolet, GM, Subaru, Infiniti, Nissan and Aston Martin, plus a
dozen aftermarket and truck units. The full list is in
[flipper/tpms_bridge/README.md](flipper/tpms_bridge/README.md).

Two overlaps are inherent to the protocols and rtl_433 has the same ones:
Jeep sensors are indistinguishable from Citroen and read half the
pressure, and the Schrader 3039 for Infiniti, Nissan and Renault is
indistinguishable from the Subaru SMD3MA4 and reads a fifth low.

## How it decodes

The radio hands over a stream of "level plus duration" intervals. Each
protocol declares a chip duration, so intervals are split into chips at
every rate in use at once; within each rate the chips are sliced into bits
two ways, because rtl_433 has two kinds of TPMS modulation — PCM passes
chips through one to one, Manchester decodes pairs of them, and a
Manchester stream can start on either chip of a pair, so both phases run
side by side.

Every combination is a stream that keeps its bits in a ring buffer and a
shift register of the last few. A protocol matches when the register ends
with its sync word; the position is written down, and once enough bits
have gone by the payload is read back out of the ring and handed to that
protocol's own decoder. Positions rather than buffers matter here: sync
words turn up inside payloads by chance and several protocols share the
same preamble, so no match may cost another one its frame.

The demodulator polarity is not known in advance, so a sync word is
matched both as written and inverted, and a payload is decoded both ways
round. On a real 407003VU0B the sync word arrives in normal polarity while
the Manchester pairs come inverted, which is exactly that case.

A frame is accepted only if its protocol's checksum matches. Half of these
protocols have nothing stronger than an eight bit checksum, so a last
filter throws out readings no tyre could produce — pressure outside 0 to
1200 kPa, temperature outside -60 to 130 C.

### The Renault frame

The one protocol checked against hardware, cross-checked against rtl_433's
`tpms_renault.c` and ProtoView's `renault.c`:

| Parameter | Value |
|---|---|
| Modulation | 2-FSK, 433.92 MHz, chip ~50 us (~20 kBaud) |
| Coding | Manchester |
| Preamble + sync | `55 55 55 56` |
| Frame | 9 bytes |
| Integrity | CRC-8, poly `0x07`, init `0x00`, over the first 8 bytes |

```
flags        = b[0] >> 2
pressure_kPa = (((b[0] & 3) << 8) | b[1]) * 0.75
temperature  = b[2] - 30          (°C)
id           = b[5]<<16 | b[4]<<8 | b[3]      (little-endian, 24 bits)
crc          = b[8]
```

The CC1101 preset for FSK (2-FSK, 20 kBaud, 28.56 kHz deviation, 325 kHz
bandwidth) comes from ProtoView, where it is proven against real sensors.
OOK uses the firmware's own 270 kHz preset.

## Installing
### The desktop application

```sh
cd host
python3 -m pip install -r requirements.txt
```

## Using it on the Flipper alone

A computer is not always needed: **Apps → Sub-GHz → TPMS Bridge**, and
reception starts immediately, with no key presses. This is the mode to use
in a car.

The list screen shows one row per sensor, up to eight held in memory, four
visible at a time and the rest reachable by scrolling:

```
433F                  4 RX
02c99d   2.19b   26C     ▁▃▅▇
7ad779   2.25b   24C     ▁▃▅
1b04f2   2.15b   31C     ▁▃
0a11c3   2.29b   19C        ·
OK:info       L:auto      R:wake
```

The header opens with what the radio is set to — `433F` is 433.92 MHz and
FSK, `315O` is 315 MHz and OOK — and then gives the number of sensors and
whether reception is local (`RX`) or handed over to a USB session (`USB`).
**Up** and **Down** step through the five settings: the four combinations
and a scan that takes all of them in turn, four seconds each. While
scanning the header reads `AUTO-433f`, lowercase and with the prefix,
because the setting moves on by itself.

The bars on the right are the signal level: the taller, the closer the
sensor. That is how the wheels are told apart — carry the Flipper from one
wheel to the next and watch which row goes up. Outlined bars mean the
sensor has been quiet for over a minute and its readings are stale.

Keys:

| Key | What it does |
|---|---|
| **Up / Down** | on the list: step through the five radio settings (433F, 433O, 315F, 315O, scan); on the detail screen: previous and next sensor |
| **Up / Down, long press** | pick a sensor out of the list |
| **OK** | details of the selected sensor, press again to go back |
| **OK, long press** | clear the list (before moving to another car, say) |
| **Left** | periodic waking on and off, a pulse every 5 s; when on, the header shows `wake` |
| **Right** | one wake pulse, 0.7 s |
| **Back** | from the details to the list, from the list out of the app |

The detail screen shows pressure in bar, PSI and kPa, temperature, the
current signal level and the peak over the last 10 seconds, how many
frames arrived, how long ago the last one did, and which protocol the
frame turned out to be.

A sensor at rest stays silent: it transmits when the wheel turns or when
it is hit by a low-frequency field, the same way a factory activation tool
works. On a bench it therefore needs waking (**Right** or **Left**), and
the sensor has to be held against the **back** of the Flipper, where the
125 kHz coil is. In a moving car waking is unnecessary: the wheels turn
and the sensors transmit by themselves.

## Using it with a computer

1. Start **Apps → Sub-GHz → TPMS Bridge** on the Flipper. The app has to
   stay open: while it is closed there is no `tpms_rx` command in the CLI.
   Local reception hands the radio over to the USB session on its own and
   takes it back once that session ends.
2. Start the desktop application:

   ```sh
   cd host
   python3 -m tpms.app
   ```

3. Pick the port (it is found automatically), tick **Wake** and press
   **Connect**.
4. Put the sensor against the **back** of the Flipper.

With **Wake** ticked the Flipper emits the field in 0.7 s pulses every 5 s
without interrupting reception — the sensor answers right away.

Readings appear in the table; the selected sensor is drawn on the chart.
**Export CSV…** writes out every frame received.

Tested with a 407003VU0B sensor: 51 frames in 25 seconds, ID `02c99d`,
26 °C, pressure near zero — the sensor was lying on a table rather than
mounted in a tyre.

### Console mode

No window, the same code:

```sh
python3 -m tpms.console --wake                # listen, wake the sensor
python3 -m tpms.console --wake --csv out.csv  # and export to CSV
python3 -m tpms.console --mode raw --wake     # raw timings
python3 -m tpms.console --decode capture.sub  # parse a file offline
```

### Straight from the Flipper CLI

```
tpms_rx                                 # the current band and modulation
tpms_rx 433920000 json wake             # with sensor waking
tpms_rx 315000000 json ook              # the American band, OOK sensors
tpms_rx 433920000 json scan             # step between FSK and OOK
tpms_rx 433920000 raw wake              # raw timings, for diagnosis
```

Every frame is one line:

```json
{"t":123456,"proto":"renault","id":"7ad779","raw":"d9453479d77affffbf","pressure_kpa_x100":24375,"temp_c":22,"flags":54,"rssi_dbm_x10":-625}
```

The numbers are integers: the firmware `printf` is not required to handle
`%f`. Fields a protocol does not carry are left out rather than sent as a
zero, and `raw` always holds the decoded payload so anything can be
re-derived on the computer.

Stop it with `Ctrl+C`.

## When no frames arrive

1. Check the band and the modulation in the header. A sensor that speaks
   OOK is invisible while the radio is set up for FSK; hold **Left** to
   scan both.
2. Check that waking is on and that the sensor lies against the back of
   the Flipper. Without that a parked sensor stays silent.
3. Run `tpms_rx 433920000 raw wake`, or pick **raw** mode in the UI, and
   raw timings will start flowing. The sensor's answer looks like a burst
   of some 130 pulses with values around 50 us in a row.
4. If there are no such bursts, the sensor is not waking up, or the
   frequency is different.

## Limitations

* While the app is open on the Flipper it occupies the screen. It can be
  closed from the outside as well: `loader close` or `ufbt launch` — the
  app handles the exit signal.
* The radio serves one owner at a time. Local reception is always on, but
  yields to a USB session and takes the radio back when that session ends.
  If yielding does not happen within two seconds, the command answers
  `{"error":"radio busy"}`.
* One band and one modulation at a time. Scanning finds more kinds of
  sensor but hears each of them half as often.
* Eight sensors are held in memory, keyed by protocol and id. A ninth
  evicts whichever has been silent the longest.
* If the host stops reading the stream without closing the command, the
  session ends by itself after 5 seconds and frees the radio.
* Raw mode is capped at 200,000 intervals per session: FSK noise arrives as
  a solid stream and would otherwise flood the channel.

## Tests

```sh
cd host && python3 -m pytest -q          # 51 tests, no hardware needed
cd flipper/decoder_test && ./run.sh      # every protocol, on the host
cd flipper/view_test && ./run.sh         # the firmware screens on the host
```

`decoder_test` is where the ports are held to account. For each of the 39
protocols there is one frame built bit by bit — preamble, coding,
checksum — in `gen_vectors.py`. That script hands the frame to a real
`rtl_433` binary and writes down **the values rtl_433 reports** as the
expectations in `vectors.h`; the test then feeds the very same bits to the
firmware decoder and compares. The expectations are therefore rtl_433's,
not ours: a decoder that reads a field from the wrong place or scales it
wrongly fails. Every protocol is tried in both stream polarities.

The Renault decoder additionally gets the cases real hardware produced: a
sync word in normal polarity with the Manchester pairs inverted, ±15%
timing jitter, and a corrupted frame that must be rejected. Finally a
frame is buried between 40000 noise intervals to check that it still comes
through, and 400000 random intervals are fed in on their own to check that
noise does not turn into readings.

`view_test` draws the app screens with the very same code as the firmware,
only the canvas is an emulator: it prints the picture as ASCII art and
complains if text ran past 128×64 or landed on a neighbouring column.
Character widths are taken with headroom, so it is pickier than the real
screen.

Regenerating the vectors needs rtl_433 built from source:

```sh
cd flipper/decoder_test && RTL433=/path/to/rtl_433 ./gen_vectors.py
```

## Layout

```
LICENSE                  MIT
flipper/tpms_bridge/     the Flipper app (C, ufbt)
  README.md              description for the Apps Catalog page
  changelog.md           version history for the catalog
  catalog/manifest.yml   draft manifest for the catalog pull request
  screenshots/           catalog screenshots (taken with qFlipper)
  tpms_bridge.c          keys, radio ownership, CLI command
  tpms_view.c            screens: sensor list and details
  tpms_store.c           sensor table: latest values, signal peak
  tpms_cli.c             the tpms_rx command: JSON and raw
  tpms_session.c         CC1101, asynchronous reception, interval buffer
  tpms_decoder.c         chips -> streams -> sync words -> frames
  tpms_protocols.c       the protocol table
  tpms_proto_*.c         one file per decoder ported from rtl_433
  tpms_bits.c            bit and checksum helpers, as in rtl_433
  tpms_lf.c              125 kHz field for waking the sensor
  tpms_preset.h          CC1101 registers for FSK
flipper/decoder_test/    every protocol, checked against rtl_433
flipper/view_test/       the same screens run on a computer
host/tpms/
  decoder.py             the Renault decoder, for offline captures
  flipper_link.py        USB CLI: command, NDJSON parsing
  model.py               per-sensor accumulation, CSV export
  sub_file.py            offline parsing of .sub and raw dumps
  ui/                    window and chart
  app.py, console.py     entry points
host/tests/              tests
```

## Publishing to the Flipper Apps Catalog

The catalog does not host source code: a pull request to
[flipper-application-catalog](https://github.com/flipperdevices/flipper-application-catalog)
carries a single file, `applications/Sub-GHz/tpms_bridge/manifest.yml`,
pointing at a public repository and a commit SHA. Their CI does the build
with ufbt.

Already in place: the MIT license, `fap_author` and `fap_version` in
`application.fam`, `flipper/tpms_bridge/README.md` (the text of the app
page), `changelog.md` in the required format and a draft manifest in
`flipper/tpms_bridge/catalog/`.

What is left to do by hand:

1. Take screenshots **with the qFlipper screenshot feature**, without
   changing their resolution or format, and put them into
   `flipper/tpms_bridge/screenshots/` — which shots exactly is written in
   the README there.
2. Fill in `commit_sha` in `catalog/manifest.yml`.
3. Fork the catalog, put the manifest at
   `applications/Sub-GHz/tpms_bridge/manifest.yml` on a branch named like
   `<username>/tpms_bridge_1.1`, and validate it locally:

   ```sh
   python3 -m venv venv && source venv/bin/activate
   pip install -r tools/requirements.txt
   export UFBT_HOME="$PWD/venv/ufbt" && ufbt update
   python3 tools/bundle.py --nolint \
       applications/Sub-GHz/tpms_bridge/manifest.yml bundle.zip
   ```

4. Open the pull request. Every later update needs a new version number in
   `application.fam`.

## License

MIT, see [LICENSE](LICENSE).

## Sources

* [rtl_433](https://github.com/merbanan/rtl_433) — every protocol here is
  a port of one of its decoders, and its output is what the tests check
  against
* [ProtoView](https://github.com/antirez/protoview) — CC1101 preset and
  the Renault test vector
* [Flipper Zero documentation](https://docs.flipper.net/zero)

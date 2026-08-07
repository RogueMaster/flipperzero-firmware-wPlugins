# Renault TPMS on 433 MHz through a Flipper Zero

Reads Renault tyre pressure sensors (433.92 MHz, for example 407003VU0B).
The Flipper Zero acts as the radio and connects to a computer over USB.

Two parts:

* **`flipper/tpms_bridge`** — the app for the Flipper Zero. Configures the
  CC1101 for 2-FSK, listens on air, decodes Renault frames and shows them
  **on its own screen**: a sensor list with signal levels and a detail
  screen for each one. The same frames are printed as line-delimited JSON
  on the CLI, which means over USB.
* **`host/`** — the desktop application in Python and PySide6: sensor
  table, pressure and temperature charts, CSV export, offline parsing of
  captures.

## Which sensors work

Verified on a real **407003VU0B** sensor. This is the Renault group
protocol described by rtl_433, which reports it on Renault Clio, Captur and
Zoe (the Zoe ships with the 407004CB0B sensor) and possibly on Dacia
Sandero. Cars of that family share the same sensors, so other Renault and
Dacia models, and Nissan models built on their platforms, are very likely
to work as well.

Any sensor of this family is picked up as long as its CRC checks out.
Sensors speaking a different protocol will not show up at all: the app
decodes the Renault frame format only, not every TPMS out there.

## Protocol

Cross-checked against
[rtl_433 `tpms_renault.c`](https://github.com/merbanan/rtl_433/blob/master/src/devices/tpms_renault.c)
and [ProtoView `renault.c`](https://github.com/antirez/protoview/blob/main/protocols/tpms/renault.c):

| Parameter | Value |
|---|---|
| Modulation | 2-FSK, 433.92 MHz, chip ~50 us (~20 kBaud) |
| Coding | Manchester, the convention varies — see below |
| Preamble + sync | `55 55 55 56`, in chips `01010101010101010110` |
| Frame | 9 bytes (144 chips after sync) |
| Integrity | CRC-8, poly `0x07`, init `0x00`, over the first 8 bytes |

**About polarity.** The demodulator may hand over the stream in either
polarity and — as the real 407003VU0B sensor showed — the polarity of the
sync word does not have to match the Manchester convention in the data: the
sync came in normal while the data pairs were inverted. So the decoder
tries both independently, all four combinations, and accepts the one whose
CRC checks out. Flipping the polarity of the stream as a whole, the way the
references do, does not catch this sensor.

Fields:

```
flags        = b[0] >> 2
pressure_kPa = (((b[0] & 3) << 8) | b[1]) * 0.75
temperature  = b[2] - 30          (°C)
id           = b[5]<<16 | b[4]<<8 | b[3]      (little-endian, 24 bits)
unknown      = b[7]<<8 | b[6]     (usually 0xFFFF)
crc          = b[8]
```

The CC1101 preset (2-FSK, 20 kBaud, 28.56 kHz deviation, 325 kHz bandwidth)
comes from ProtoView, where it is proven against real sensors.

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
TPMS                    4 dev  RX
02c99d   2.19b   26C        ▁▃▅▇
7ad779   2.25b   24C        ▁▃▅
1b04f2   2.15b   31C        ▁▃
0a11c3   2.29b   19C        ·
OK:info       R:wake      L:auto
```

The bars on the right are the signal level: the taller, the closer the
sensor. That is how the wheels are told apart — carry the Flipper from one
wheel to the next and watch which row goes up. Outlined bars mean the
sensor has been quiet for over a minute and its readings are stale.

Keys:

| Key | What it does |
|---|---|
| **Up / Down** | select a sensor |
| **OK** | details of the selected sensor, press again to go back |
| **OK, long press** | clear the list (before moving to another car, say) |
| **Right** | one wake pulse, 0.7 s |
| **Left** | periodic waking on and off, a pulse every 5 s; when on, the header shows `wake` |
| **Back** | from the details to the list, from the list out of the app |

The detail screen shows pressure in bar, PSI and kPa, temperature, the
current signal level and the peak over the last 10 seconds, how many frames
arrived, how long ago the last one did, the flags and the protocol field
whose meaning is unknown.

A sensor at rest stays silent: it transmits when the wheel turns or when it
is hit by a low-frequency field, the same way a factory activation tool
works. On a bench it therefore needs waking (**Right** or **Left**), and
the sensor has to be held against the **back** of the Flipper, where the
125 kHz coil is. In a moving car waking is unnecessary: the wheels turn and
the sensors transmit by themselves.

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
tpms_rx                            # 433.92 MHz, JSON
tpms_rx 433920000 json wake        # with sensor waking
tpms_rx 433920000 raw wake         # raw timings, for diagnosis
```

Every frame is one line:

```json
{"t":123456,"proto":"renault","id":"7ad779","raw":"d9453479d77affffbf","pressure_kpa_x100":24375,"temp_c":22,"flags":54,"unknown":65535,"rssi_dbm_x10":-625}
```

The numbers are integers: the firmware `printf` is not required to handle
`%f`. Fields are recomputed on the computer from `raw` —
`host/tpms/decoder.py` stays the single source of truth for the protocol.

Stop it with `Ctrl+C`.

## When no frames arrive

1. Check that waking is on and that the sensor lies against the back of the
   Flipper. Without that it stays silent.
2. Run `tpms_rx 433920000 raw wake`, or pick **raw** mode in the UI, and
   raw timings will start flowing. The sensor's answer looks like a burst
   of some 130 pulses with values around 50 us in a row.
3. If there are no such bursts, the sensor is not waking up, or the
   frequency is different.
4. If there are bursts but no frames, the protocol differs. Save the log
   with **Save log…**, then work on it offline through **Open .sub /
   dump…** or `--decode`. Only `host/tpms/decoder.py` needs changing; the
   firmware does not have to be rebuilt.

## Limitations

* While the app is open on the Flipper it occupies the screen. It can be
  closed from the outside as well: `loader close` or `ufbt launch` — the
  app handles the exit signal.
* The radio serves one owner at a time. Local reception is always on, but
  yields to a USB session and takes the radio back when that session ends.
  If yielding does not happen within two seconds, the command answers
  `{"error":"radio busy"}`.
* Eight sensors are held in memory. A ninth evicts whichever has been
  silent the longest.
* If the host stops reading the stream without closing the command, the
  session ends by itself after 5 seconds and frees the radio.
* Raw mode is capped at 200,000 intervals per session: FSK noise arrives as
  a solid stream and would otherwise flood the channel.

## Tests

```sh
cd host && python3 -m pytest -q          # 51 tests, no hardware needed
cd flipper/decoder_test && ./run.sh      # the firmware decoder on the host
cd flipper/view_test && ./run.sh         # the firmware screens on the host
```

Both decoder suites run the same test vector from ProtoView (ID `7ad779`,
243.75 kPa, 22 °C), check the CRC, all polarity combinations (including the
one a real sensor produces), tolerance to ±15% jitter and the rejection of
corrupted frames. A real frame from a 407003VU0B is pinned in the tests as
a regression.

`view_test` draws the app screens with the very same code as the firmware,
only the canvas is an emulator: it prints the picture as ASCII art and
complains if text ran past 128×64 or landed on a neighbouring column.
Character widths are taken with headroom, so it is pickier than the real
screen. It covers the empty list, four wheels, scrolling with eight sensors
and extreme values (1023 raw pressure units, −40 °C, −100 dBm).

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
  tpms_renault.c         chips -> Manchester -> CRC -> fields
  tpms_lf.c              125 kHz field for waking the sensor
  tpms_preset.h          CC1101 registers
flipper/decoder_test/    the same decoder run on a computer
flipper/view_test/       the same screens run on a computer
host/tpms/
  decoder.py             decoder and frame builder
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

1. Publish the repository on GitHub (the catalog accepts GitHub only).
2. Take screenshots **with the qFlipper screenshot feature**, without
   changing their resolution or format, and put them into
   `flipper/tpms_bridge/screenshots/` — which shots exactly is written in
   the README there.
3. Fill in `origin` and `commit_sha` in `catalog/manifest.yml`.
4. Fork the catalog, put the manifest at
   `applications/Sub-GHz/tpms_bridge/manifest.yml` on a branch named like
   `<username>/tpms_bridge_1.0`, and validate it locally:

   ```sh
   python3 -m venv venv && source venv/bin/activate
   pip install -r tools/requirements.txt
   export UFBT_HOME="$PWD/venv/ufbt" && ufbt update
   python3 tools/bundle.py --nolint \
       applications/Sub-GHz/tpms_bridge/manifest.yml bundle.zip
   ```

5. Open the pull request. Every later update needs a new version number in
   `application.fam`.

## License

MIT, see [LICENSE](LICENSE).

## Sources

* [rtl_433](https://github.com/merbanan/rtl_433) — the protocol reference
* [ProtoView](https://github.com/antirez/protoview) — CC1101 preset and test vector
* [Flipper Zero documentation](https://docs.flipper.net/zero)

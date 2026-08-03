# NFC Alerter

Passive 13.56 MHz reader detector for Flipper Zero. Carry it in a pocket; it
alerts you — by vibration first — when a reader interrogates you.

Uses the ST25R3916's hardware External Field Detector: it listens for a
reader's carrier and, in the default mode, never transmits anything of its own.

> **Detects readers, not tags.** A card, badge, or fob will *never* trigger
> this — passive tags have no transmitter and only reflect a reader's energy.
> You need something that **emits**: a phone with NFC on (unlocked), or a
> payment/access terminal. Present it to the **back** of the Flipper.

## Screens

Navigate with Left/Right from Status. Everything is one press from home.

```
   [Session] <- [Status] -> [Events] -> [Event detail]
                   |  \
              OK=Settings  Up=Diagnostics
```

- **Status** — armed state, current threat tier, live exposure timer, and the
  history strip (last ~5 minutes; bar height = highest tier in that slot).
- **Events** — timestamped log, newest first. OK opens detail: protocol,
  decoded reader command, exposure duration, capture mode.
- **Session** — counts by tier, first/last event, longest exposure. OK clears.
- **Settings** — alerts, mode, logging. Back saves.
- **Diagnostics** — raw EFD bit, latch state, raw edge counter, alarm
  self-test. Kept for testing; see [TESTING.md](docs/TESTING.md).

## Threat tiers

The world is saturated with 13.56 MHz, so a bare carrier is not an alarm.

| Tier | Trigger | Default response |
|---|---|---|
| **Info** | Carrier < 300 ms | Logged, silent |
| **Warn** | Carrier sustained | Vibration |
| **Alarm** | Reader addressed us / issued a command | Vibration + sound |

## Alerts

Vibration is the primary channel — a pocket alert should be *felt* before it is
heard, and audio is muffled by fabric and useless in a loud room. By default
vibration engages one tier earlier than sound.

Configurable: vibration pattern, sound pattern, tone (1–4 kHz), volume, the
minimum tier for each channel, silent mode, LED, and SD logging. Patterns are
`Off / Single / Double / SOS / Pulse / Constant`.

Press **OK** in Settings to audition the current configuration without needing
a reader.

## Detection modes

| Mode | Sees | Trade-off |
|---|---|---|
| **Sentinel** (default) | Any carrier, any protocol | Fully passive; no protocol detail |
| **Decoy** | Protocol + reader command bytes | Responds as a fake card |

Decoy mode presents a random-UID card so the reader will talk to it, which is
what reveals *what* is being asked for (`Auth key A`, `Read block`, `Select
application`, …). This is diagnostic only: the log records that an
interrogation happened and which command was used — never card contents.

## Build and deploy

```bash
./deploy.sh                # auto-detects the Flipper's serial port
./deploy.sh /dev/ttyACM0   # or name it
```

Then: **Apps → NFC → NFC Alerter**

### SDK version must match the firmware

The loader rejects a FAP whose API major version differs from the firmware's,
reporting only `Preload failed: Invalid file`.

Check the device with `info device`, then confirm the SDK matches:

```bash
grep -m1 '^Version' ~/.ufbt/current/sdk_headers/f7_sdk/targets/f7/api_symbols.csv
```

This device runs Unleashed `unlshd-089e` = API 87.8. ufbt's published channels
carry newer SDKs that build fine and then refuse to load, so pin it:

```bash
curl -sL -o sdk089.zip \
  https://github.com/DarkFlippers/unleashed-firmware/releases/download/unlshd-089/flipper-z-f7-sdk-unlshd-089.zip
ufbt update --local=sdk089.zip --hw-target=f7
```

CI builds against official firmware (release + dev) — that is what keeps the
app eligible for the Apps Catalog, since local builds use a fork SDK.

### Why a custom push script

`ufbt launch` and the SDK's `storage.py` both open an RPC session that
conflicts with an active CLI session and hang. `tools/flipper_push.py` speaks
the plain CLI protocol and verifies with an on-device md5.

Two failure modes it works around, both of which cause *silent* corruption:

1. **Commands must end with `\r` only.** A trailing `\n` stays in the stream
   and `storage write_chunk` counts it as the first payload byte, shifting
   every chunk by one — correct file size, wrong contents.
2. **Wait for the prompt after each chunk** or the stream desynchronizes.

### If the CLI stops responding

The serial CLI can wedge (port opens, commands return nothing). Power-cycle:
hold **Back + Left** for ~5 seconds.

Note the loader will not force-close a running FAP — exit it with **Back** on
the device before deploying a new build.

## Tuning

Constants in `nfc_alerter_i.h`:

| Constant | Default | Effect |
|---|---|---|
| `POLL_INTERVAL_MS` | 20 | EFD poll rate |
| `FIELD_MIN_PULSE_MS` | 20 | Minimum rising edge to count |
| `FIELD_HOLDOFF_MS` | 750 | Silence before clearing a detection |
| `TIER_WARN_MS` | 300 | Info → Warn promotion |

`FIELD_HOLDOFF_MS` matters most. Readers pulse their carrier rather than
holding it steady, so a symmetric debounce fails — the bit never holds still
long enough and a real reader reads as noise. The latch fires on the first
rising edge and only clears after the field has been continuously absent for
the hold-off, collapsing a burst train into one detection.

## License

MIT — see [LICENSE](LICENSE).

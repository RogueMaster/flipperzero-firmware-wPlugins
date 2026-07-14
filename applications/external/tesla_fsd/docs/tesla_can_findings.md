# Tesla CAN Findings HW3 - EU Version

This file records confirmed CAN findings from live car captures. Keep entries
evidence-based: include frame ID, bus, signal layout, observed payloads, and any
known uncertainty.

## 0x229 Gear Lever / Right Stalk

Frame:
- CAN ID: `0x229`
- Observed bus on LilyGO T-2CAN: `can1`
- DLC: `3`
- DBC message: `ID229GearLever`
- Position signal: `GearLeverPosition229`
- Decode: `position = (data[1] >> 4) & 0x07`
- Counter: `counter = data[1] & 0x0F`
- Byte 2 observed as `0x00` for these gear-lever actions.

Confirmed position mapping:

| Value | Meaning |
| --- | --- |
| `0` | Center / idle |
| `1` | Half up |
| `2` | Full up |
| `3` | Half down |
| `4` | Full down |

Observed action examples:

| Time | Payloads | Decoded |
| --- | --- | --- |
| `25.189s-25.327s` | `4B3100`, `B24400` | Up action passing half-up to full-up |
| `27.628s-28.028s` | `A62C00`, `DE2E00`, `D42F00`, `A92000` | Full up |
| `30.628s` | `833B00` | Half down |
| `33.027s-33.328s` | `A11500`, `3D1600`, `191700`, `AC1800` | Half up |
| `55.419s-55.827s` | `343F00`, `493000`, `9C4300`, `B24400`, `B04500` | Down action passing half-down to full-down |

Important correction:
- Do not identify right-stalk actions from `0x340`. Live captures show `0x340`
  repeating counter-like payloads such as `0400`, `0500`, `0800`, and `0A04`
  during normal traffic, so using exact `0x340` payloads creates false
  positives.

### 0x229 TX Notes

The `0x229` byte 0 checksum/CRC was derived from live captures. The builder was
validated against 375 captured `0x229` frames with zero mismatches.

Neutral CRC table by counter:

```text
counter:  0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
byte0:   46  44  52  6D  43  41  DD  F9  4C  A5  F6  8C  49  2F  31  3B
```

Position XOR deltas applied to the neutral CRC byte:

| Position | Meaning | XOR delta |
| --- | --- | --- |
| `0` | Center | `0x00` |
| `1` | Half up | `0xE0` |
| `2` | Full up | `0xEF` |
| `3` | Half down | `0x0F` |
| `4` | Full down | `0xF1` |

TX reliability note:
- Generating `0x229` frames on a free-running timer was intermittent because it
  could collide with the car's own neutral `0x229` stream or use a stale
  counter.
- The more reliable strategy is live-frame-timed override:
  - arm the requested gear position for a short window,
  - wait for a real live `0x229`,
  - transmit the override immediately with `live_counter + 1`.

## 0x3C2 Right Steering-Wheel Scroll

Frame:
- CAN ID: `0x3C2`
- Observed buses on LilyGO T-2CAN: `can0` and `can1`
- Relevant mux: `data[0] & 0x03 == 1`
- Signal: signed 6-bit right scroll tick count in `data[3] & 0x3F`
- Decode:

```c
uint8_t raw = data[3] & 0x3F;
int8_t tick = (raw & 0x20) ? (int8_t)(raw - 64) : (int8_t)raw;
```

Neutral payload:

```text
2955000000000080
```

Confirmed action mapping:
- Positive tick: wheel up.
- Negative tick: wheel down.
- Single ticks appear as `+/-1` or small repeated `+/-1` frames.
- Bursts appear as several same-direction nonzero ticks within a short cluster.

Observed examples:

| Time | Payloads | Decoded |
| --- | --- | --- |
| `25.974s`, `27.574s`, `28.874s-30.176s` | `2955000100000080` | `+1`, wheel up single / repeated single |
| `31.474s-34.477s` | `2955003F00000080` | `-1`, wheel down single / repeated single |
| `36.675s-39.379s` | `2955000300000080`, `2955000200000080`, `2955000500000080` | `+3`, `+2`, `+5`, wheel up burst |

Expected wheel-down burst shape:
- Same as wheel-up burst but negative, for example raw values:
  - `0x3D` = `-3`
  - `0x3E` = `-2`
  - `0x3B` = `-5`

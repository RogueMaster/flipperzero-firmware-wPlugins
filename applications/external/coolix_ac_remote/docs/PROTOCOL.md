# Coolix IR protocol

Ported from ESPHome:

- `esphome/components/remote_base/coolix_protocol.cpp` — line coding
- `esphome/components/coolix/coolix.cpp` — state word composition

Constants for the one-shot buttons come from IRremoteESP8266's `src/ir_Coolix.h`.

## Line coding

Carrier 38 kHz, 33% duty. All times are multiples of one tick, **T = 560 us**.

| Element      | Mark | Space |
|--------------|------|-------|
| Leader       | 8T (4480) | 8T (4480) |
| Bit `1`      | 1T (560)  | 3T (1680) |
| Bit `0`      | 1T (560)  | 1T (560)  |
| Frame footer | 1T (560)  | —         |
| Inter-frame gap | —      | 10T (5600) |

A frame carries a 24-bit word as three bytes, **most significant byte first**.
Each byte is sent **MSB bit first, normally, and then again inverted** — so 48
bits go on the wire per frame. The whole frame is transmitted **twice**,
separated by the 10T gap.

```
[leader][B2][~B2][B1][~B1][B0][~B0][footer]  gap  [leader]…[footer]
```

Timing count: `2 + (3 bytes x 16 bits x 2) + 1 = 99` per frame,
`99 + 1 + 99 = 199` for the full signal. Buffer is sized 200.

## State word

Base `0xB20F00`. `0xB2` identifies the family and the `0x0F` nibble is fixed.

| Field       | Mask     | Values |
|-------------|----------|--------|
| Fan         | `0xF000` | Auto `0xB000`, Low `0x9000`, Med `0x5000`, High `0x3000`, forced-Auto `0x1000` |
| Temperature | `0x00F0` | table below |
| Mode        | `0x000C` | Cool `0x0`, Dry/Fan `0x4`, Auto `0x8`, Heat `0xC` |

Rules the unit imposes, mirrored by the app:

- **Auto** and **Dry** force the fan field to `0x1000`; the Fan button is
  greyed out in those modes.
- **Dry** and **Fan-only** share the same mode bits. They are told apart by
  the fan field: Dry uses forced-auto `0x1000`, Fan-only uses a real fan speed.
- **Fan-only** carries no setpoint. Its temperature nibble is the marker
  `0xE0` instead of a degree code.
- **Off** is not a mode bit pattern — it is the standalone word `0xB27BE0`.

### Temperature codes (17–30 °C)

Not monotonic, so it stays a lookup table.

| °C | nibble | °C | nibble |
|----|--------|----|--------|
| 17 | `0x00` | 24 | `0x40` |
| 18 | `0x10` | 25 | `0xC0` |
| 19 | `0x30` | 26 | `0xD0` |
| 20 | `0x20` | 27 | `0x90` |
| 21 | `0x60` | 28 | `0x80` |
| 22 | `0x70` | 29 | `0xA0` |
| 23 | `0x50` | 30 | `0xB0` |

Sanity check: Auto / 25 °C / fan auto composes to `0xB21FC8`, which is
IRremoteESP8266's `kCoolixDefaultState`.

## One-shot command words

These are complete words, not state modifiers. The AC sends nothing back, so
the app tracks them as local display flags only.

| Button   | Word       | Note |
|----------|------------|------|
| Power off| `0xB27BE0` | sent when Mode is set to Off |
| Swing    | `0xB26BE0` | vane oscillation on/off |
| Direct   | `0xB20FE0` | steps the vertical vane one position |
| Turbo    | `0xB5F5A2` | |
| LED      | `0xB5F5A5` | display backlight |
| Sleep    | `0xB2E003` | |
| Silence  | `0xB5F5B6` | "Feeling Point" on some units — Extra screen |
| Swing H  | `0xB2F5A2` | horizontal vane — Extra screen |
| Fan cmd  | `0xB2BFE4` | fan-speed-only command — Extra screen |
| Clean    | `0xB5F5AA` | Extra screen |

## Not implemented

The 24-bit Coolix protocol has no timer command, so the base app's timer
screen was replaced by the Extra screen. Timers exist only in the separate
48-bit "Coolix48" variant, which is a different wire format.

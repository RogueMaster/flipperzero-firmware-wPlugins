# UI Guide

Every pixel of the 128 × 64 display, explained.

## Layout overview

```
y= 0..7   ┌────────────────────────────────────────────────────────┐
          │ Ch042  2442 MHz                                  N1234 │   ← Header
y= 8      │────────────────────────────────────────────────────────│
y= 9..12  │  ▲ ■    ▲   ▲                              ■           │   ← Marker band
y=13      │────────────────────────────────────────────────────────│
y=15..16  │      ▼                                                 │   ← Cursor band
y=17..51  │      ┊         ▌                                       │   ← Bar chart
          │      ┊       ▌▌▌                  ▌                    │
          │      ┊    ▌▌▌▌▌▌▌      ▌▌▌  ▌    ▌▌                    │
          │    ▌▌┊▌▌▌▌▌▌▌▌▌▌▌▌    ▌▌▌▌▌▌▌▌▌  ▌▌▌▌▌▌                │
y=52      │────────────────────────────────────────────────────────│
y=57..62  │ Pk37@28           Cur:5                    Dwl:150us   │   ← Footer
          └────────────────────────────────────────────────────────┘
```

## Header (y=0..7)

```
Ch042  2442 MHz                                              N1234
[left: cursor channel + frequency]                           [right: status]
```

### Left text — cursor info

`Ch042  2442 MHz` means: cursor is on channel 42, which corresponds to 2442 MHz.

Frequency formula for nRF24L01+: `freq_MHz = 2400 + N`

So channel 0 = 2400 MHz, channel 125 = 2525 MHz.

### Right text — status indicator

| Display | Meaning |
|---------|---------|
| `N1234` | Scanning. Number = sweep count completed |
| `PAUSE` | Manually paused (you pressed OK) |
| `MAX!` | Auto max-hold mode triggered (a bar reached the top) |

## Marker band (y=9..12)

Persistent overlays showing common 2.4 GHz protocols' channels. They never move.

### WiFi markers ▲

Three filled triangles at NRF24 channels 12, 37, 62 — the centers of WiFi channels 1, 6, 11 (the canonical non-overlapping trio in US/EU).

| Symbol | WiFi ch | NRF24 ch | Frequency |
|--------|---------|----------|-----------|
| ▲ | 1  | 12 | 2412 MHz |
| ▲ | 6  | 37 | 2437 MHz |
| ▲ | 11 | 62 | 2462 MHz |

When you see bars stacked under a ▲, you're looking at WiFi traffic.

### BLE markers ■

Three filled squares at NRF24 channels 2, 26, 80 — corresponding to BLE advertising channels 37, 38, 39.

| Symbol | BLE adv ch | NRF24 ch | Frequency |
|--------|------------|----------|-----------|
| ■ | 37 | 2  | 2402 MHz |
| ■ | 38 | 26 | 2426 MHz |
| ■ | 39 | 80 | 2480 MHz |

Bars stacked under a ■ likely indicate active BLE devices nearby.

## Cursor band (y=14..16)

The black ▼ triangle is your cursor. It points to the channel the footer's `Cur:N` reads from.

A dotted vertical line extends from the triangle through the entire chart area, so you can see exactly which bar the cursor selects, even when bars are tall.

Move the cursor with **← / →** (single-press = ±1 channel, hold/repeat = ±5 channels).

## Chart (y=17..51)

126 vertical bars, one per nRF24 channel, from x=1 to x=126.

### Bar height

```
bar_height_pixels = hit_count × 35 / 32
```

- `hit_count` is a saturating accumulator (0–32) of RPD detections
- Each RPD hit increments by 2; no decay (max-hold mode)
- When any bar reaches max (32), the entire scan auto-pauses (top-right shows `MAX!`)

### Bar floor

The bottom of bars is at y=51, just above the bottom divider line. There's a 2-pixel safety buffer between the marker band's bottom and the chart's top — bars never touch the divider, so they always look "intentional", not "clipped".

### Min visibility floor

Single hits would render as 1-pixel bars (often invisible). The chart enforces a minimum of 3 pixels for any non-zero count, so even one hit registers visually.

## Footer (y=57..62)

```
Pk37@28           Cur:5                    Dwl:150us
[Pk: peak]        [Cur: cursor channel]    [Dwl: dwell time]
```

### Pk (peak) — left

`Pk37@28` means: the channel with the highest current hit count is channel 37, with 28 hits.

Useful for "which channel is busiest?" at a glance.

### Cur (cursor) — center

`Cur:5` means: the cursor's current channel has 5 hits.

This is the live readout for whatever channel your cursor is on. Move cursor → Cur changes.

The Cur slot uses **adaptive layout** — its position auto-centers between Pk and Dwl based on each value's actual width, so the spacing always looks balanced.

### Dwl (dwell time) — right

`Dwl:150us` means: each channel is sampled for 150 microseconds before checking RPD.

Adjust with **↑ / ↓**:
- Short press: ±10 µs
- Long press / repeat: ±50 µs
- Range: 130 µs (NRF24 Tstby2a minimum) – 2000 µs

**Lower dwell** = faster sweep, less sensitivity (might miss short bursts)  
**Higher dwell** = slower sweep, catches weaker / shorter signals

## Operating modes

### Scanning

Default state. Worker thread sweeps continuously. Bars accumulate. Display updates ~17 times per second.

Right-side header: `N1234` (sweep counter increments).

### Paused (manual)

Press OK while scanning. Worker pauses, display freezes. Right-side header: `PAUSE`.

Press OK again to resume — accumulation continues from where it stopped, no reset.

### Max Hold (auto)

When **any** bar reaches the top (hit count = 32), the worker auto-stops the entire scan. Display freezes for inspection. Right-side header: `MAX!`.

This is the spectrum-analyzer convention — stop accumulating when something interesting "wins", so you can read the captured snapshot without it changing under you.

While in MAX HOLD, you can still move the cursor to inspect any channel's hit count via the footer's `Cur:N`.

Press OK to clear all hits and restart scanning. The cursor stays where you left it.

## Controls reference

| Key | Short press | Long press / Repeat |
|-----|-------------|---------------------|
| **← / →** | Cursor ±1 channel | Cursor ±5 channels |
| **↑ / ↓** | Dwell ±10 µs | Dwell ±50 µs |
| **OK** | Pause / Resume / Reset (in MAX HOLD) | — |
| **Back** | Exit app | — |

---

🛒 **[PINGEQUA 2-in-1 RF Devboard](https://www.pingequa.com/products/flipper-zero-nrf24-cc1101-2-in-1-rf-devboard?utm_source=github&utm_medium=docs_ui&utm_campaign=rflab)** — get the hardware that makes this possible.

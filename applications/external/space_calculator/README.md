# Space Travel Calculator

A minimalist trajectory calculator for the Flipper Zero. Plan real interplanetary missions with KSP-style visual feedback.

## Overview

Answer the critical mission planning questions:
- **"When's the next good launch window?"**
- **"How much fuel (delta-v) will this mission need?"**  
- **"How long until my crew gets there?"**
- **"Is this launch date optimal or should I wait?"**

Watch planets move in real-time as you scroll through launch dates. See transfer windows open and close. Make informed mission decisions.

## Features

- **Real-time orbital calculations** with KSP-style visual feedback
- **Extensive destination options**: Mars, Moon, Venus, Mercury, Jupiter, plus major moons (Europa, Titan, Ganymede, etc.)
- **Dynamic transfer visualization**: Watch planets move as you scroll through launch dates
- **Mission planning tools**: Delta-v requirements, flight times, launch windows
- **Visual transfer window identification**: See optimal windows by orbit ellipse shape

## User Interface

### Main View (Orbit Diagram)
```
EARTH → MARS
────────────
LAUNCH: 147
FLIGHT: 259d
BURN: →32°
[Orbit diagram]
```

### Data View
```
TRANSFER DATA
─────────────
ΔV TOTAL: 5.64 km/s
DEPART: 3.58 km/s
ARRIVE: 2.06 km/s
PHASE: 44.3°
WINDOW: GOOD
```

### Phase View
Shows current planetary alignment and days until optimal transfer window.

## Controls

- **UP/DOWN**: Select destination planet
- **LEFT/RIGHT**: Adjust launch date (day of year)
- **OK**: Cycle through views (Orbit → Data → Phase)
- **BACK**: Exit application

## Technical Details

- Assumes circular planetary orbits for simplicity
- Uses January 1, 2024 as epoch for calculations
- All calculations in metric units (km/s, days)
- Response time <50ms for all interactions

## Building

```bash
# From flipper firmware root
./fbt fap_space_travel_calculator
```

## Installation

Copy `space_travel_calculator.fap` to your Flipper Zero's SD card under `/apps/Tools/`

## Physics Reference

Uses simplified patched conic approximation for interplanetary transfers. Ideal for quick field calculations when you need to know if that Mars launch window is actually worth taking.

---

*"Space is hard. Math shouldn't be."*
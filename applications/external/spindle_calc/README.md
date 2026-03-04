# Spindle Calculator — Flipper Zero App

A trade tool for carpenters and joiners to calculate spindle counts and spacings for balustrades, right on site — no phone or calculator needed.

![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-App-orange)
![Category](https://img.shields.io/badge/Category-Tools-blue)
![License](https://img.shields.io/badge/License-MIT-green)

---

## What it does

Given a length and spindle size, it calculates:
- How many spindles fit while keeping gaps within the **99 mm building regulation limit**
- The exact gap between each spindle
- Centre-to-centre spacing

There are two modes:

### Stair mode
Enter the **diagonal (rake) length** between posts and the **stair angle**. The app converts the diagonal to a horizontal run using `horiz = diagonal × cos(angle)`, then calculates all spacings from that. Results include both the horizontal gap and the rake gap (measured along the slope).

The built-in accelerometer can read the stair angle automatically — just lay the Flipper flat on a tread and press OK.

### Straight run mode
Enter the total **horizontal length** between posts. The app calculates count and spacing directly.

---

## Controls

### Angle screen
| Button | Action |
|--------|--------|
| Up / Down | ±0.1° |
| Left / Right | ±1.0° |
| OK | Lock angle and continue |
| Back | Return to menu |

### Length input screens
| Button | Action |
|--------|--------|
| Up / Down (short) | ±1 mm |
| Up / Down (hold) | ±10 mm |
| Left / Right (short) | ±10 mm |
| Left / Right (long press) | ±100 mm |
| Left / Right (on spindle field) | Change spindle size |
| OK | Next field / Calculate |
| Back | Previous field / Menu |

### Result screens
| Button | Action |
|--------|--------|
| Up / Down | Scroll results |
| Back | Return to inputs |

---

## Building

Requires the [Flipper Zero firmware SDK](https://github.com/flipperdevices/flipperzero-firmware) or [uFBT](https://github.com/flipperdevices/flipperzero-ufbt).

```bash
# Using uFBT (recommended)
ufbt

# Deploy directly to connected Flipper
ufbt launch
```

---

## Installing from the catalog

1. Open the **Flipper Mobile App** or go to [lab.flipper.net](https://lab.flipper.net)
2. Browse to **Apps → Tools → Spindle Calculator**
3. Tap **Install**

---

## Building regulation note

This app enforces the **99 mm maximum gap** between spindles as required by UK Building Regulations Approved Document K (and equivalent standards). Always verify final measurements on site.

---

## License

MIT — see [LICENSE](LICENSE)

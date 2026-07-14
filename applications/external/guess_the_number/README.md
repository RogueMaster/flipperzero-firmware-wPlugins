# Guess The Number - Flipper Zero

> An enhanced number guessing game with progressive feedback, score tracking, and immersive multi-sensory experience.

[![FAP: Build for multiple SDK sources](https://github.com/kWAYTV/guess-the-number-fz/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/kWAYTV/guess-the-number-fz/actions/workflows/build.yml)

## ✨ Features

- **Progressive Hint System** - Distance-based feedback (very close, close, far)
- **Score Tracking** - Personal best score persistence across games
- **Multi-Sensory Feedback** - Dynamic LED colors, haptic patterns, and sound effects
- **Intuitive Controls** - Fine (+/-1) and coarse (+/-10) adjustment modes
- **Professional UI** - Centered layout with visual emphasis and clear typography

## 🎮 Gameplay

Guess the secret number (0-99) in the fewest attempts possible. The game provides intelligent feedback based on your proximity to the target, with enhanced audio and visual cues.

**Controls:**

- `↑/↓` Fine adjust (+/-1)
- `←/→` Quick adjust (+/-10)
- `OK` Submit guess
- `BACK` Reset game (hold to exit)

## 🚀 Installation

### Pre-built Releases (Recommended)

Download the appropriate `.fap` file for your firmware from the [latest release](https://github.com/kWAYTV/guess-the-number-fz/releases/latest):

- `guess_the_number_official-dev.fap` - Official Dev firmware
- `guess_the_number_official-release.fap` - Official Release firmware
- `guess_the_number_momentum-dev.fap` - Momentum Dev firmware
- `guess_the_number_momentum-release.fap` - Momentum Release firmware

Deploy the file to your Flipper Zero's `apps/Games` directory.

## 🛠️ Development

Built with Flipper Zero SDK using [leedave's boilerplate](https://github.com/leedave/flipper-zero-fap-boilerplate).

**Build Requirements:**

- [uFBT](https://github.com/flipperdevices/flipperzero-ufbt)
- Flipper Zero firmware API v86.0+

### Build from Source

```bash
git clone https://github.com/kWAYTV/guess-the-number-fz
cd guess-the-number-fz
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --hw-target=f7 --channel=dev
ufbt
```

## 📄 License

MIT License - see [LICENSE](LICENSE) for details.

---

**Target:** 7 | **API:** 86.0 | **Category:** Games

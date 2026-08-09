# Walkie Talkie — FRS / PMR446 Channel Scanner for Flipper Zero

> **Audio limitation:** The CC1101 data mirror is a digital receive-data stream, not
> demodulated analog voice. Treat this app as a channel activity/RSSI scanner unless
> external demodulation hardware is added; speaker output is not reliable voice audio.

📻 Listen in on walkie-talkie channels with your Flipper Zero. Tunes the built-in CC1101 radio to the 22 standard FRS channels (US/Canada) or the 16 PMR446 channels (Europe) and plays whatever it hears through the speaker, with auto-squelch and a channel scanner.

> **Receive-only.** This app does not transmit, and it does not decode CTCSS/DCS — subchannel numbers are labels for matching your handheld radio's display, not a privacy-code filter.
>
> It also cannot play FM broadcast radio; those frequencies are outside the CC1101's supported bands.

## ✨ Features

- Two channel plans, switchable in Settings:
  - **FRS** (US/Canada) — all 22 channels, 462/467 MHz
  - **PMR446** (Europe) — all 16 analogue channels, 446.00625–446.19375 MHz
- Scrollable channel list for the active band
- Channel scanner: scans up or down, pauses automatically when a signal is detected, and resumes after the transmission ends
- Auto-squelch with adjustable sensitivity (or turn it off to hear raw static)
- Live RSSI readout with a 5-bar signal-strength meter
- Subchannel (privacy-code) labels 1–38 for quick reference

## 📸 Screenshots

| Listen Now | Settings |
|---|---|
| ![Listen Now screen](screenshots/Screenshot1.png) | ![Settings screen](screenshots/Screenshot2.png) |

| Channel List | About |
|---|---|
| ![Channel list](screenshots/Screenshot3.png) | ![About screen](screenshots/Screenshot4.png) |

## 🎛️ Controls

### Listen Now (main screen)

| Button | Short press | Long press |
|---|---|---|
| Up / Down | Channel up / down | Squelch sensitivity + / − |
| Left / Right | Subchannel − / + (0 = none) | Scan direction down / up |
| OK | Mute / unmute | Start, resume, or stop scanning |
| Back | Open menu | Exit app |

### Menu / Channel List / Settings

| Button | Action |
|---|---|
| Up / Down | Move cursor |
| OK | Select (in Channel List: tune to the highlighted channel; in Settings: act on the highlighted row) |
| Left / Right | Settings: switch band, toggle Auto-Squelch, or adjust sensitivity |
| Back | Back to previous screen (long press exits the app) |

## 🔧 Building from source

Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (micro Flipper Build Tool):

```sh
pip install ufbt
cd Walkie_Talkie
ufbt          # builds walkie_talkie.fap into dist/
ufbt launch   # builds, installs to a connected Flipper, and runs it
```

## 📦 Installing a release

Copy `walkie_talkie.fap` to your Flipper's SD card under `apps/Sub-GHz/`, then launch it from **Apps → Sub-GHz → Walkie Talkie**.

## ⚖️ Legal note

Receiving FRS or PMR446 transmissions is legal in most jurisdictions, but laws vary — check your local regulations. This app never transmits.

## 🤔 To do

- Consider a custom radio preset to improve audio quality

## 👤 Author

Made by **coolshrimp** (Coolshrimp Modz)

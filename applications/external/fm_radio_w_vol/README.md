![Project logo](images/logo.png)

# flipperzero-radio-with-volume-control

Flipper Zero external app and hardware project for TEA5767 FM reception, PT2257/PT2259-S audio control, PAM8406 output stage, and working RDS decode from the TEA5767 MPXO path. The current PCB v1.1 product name is **FReD FM - Flipper Zero FM + RDS Radio Board**.

## Table of Contents

- [Quick Start](#quick-start)
- [Project Status](#project-status)
- [Features](#features)
- [Controls](#controls)
- [Current UI](#current-ui)
- [Config Options](#config-options)
- [Manual RDS Carrier Offset](#manual-rds-carrier-offset)
- [RDS Debug Capture](#rds-debug-capture)
- [Build and Release](#build-and-release)
- [Hardware](#hardware)
- [PCB (Current Revision)](#pcb-current-revision)
- [App Screenshots](#app-screenshots)
- [Persistent Settings](#persistent-settings)
- [TEA5767 Audio Tuning](#tea5767-audio-tuning)
- [Provenance and Licensing](#provenance-and-licensing)
- [References](#references)
- [Acknowledgements](#acknowledgements)

## Quick Start

Linux/macOS (developer workflow with `ufbt`):

- From the project directory: `ufbt update && ufbt build && ufbt launch`
- This builds, installs, and launches the app over USB.

Windows (qFlipper):

- Download the `.fap` from the latest GitHub Release.
- Open `qFlipper` and connect your Flipper Zero.
- Copy the `.fap` to `/ext/apps/GPIO/`.
- Run it on Flipper from `Applications -> GPIO`.

## Project Status

PCB v1.1 is now the main hardware revision for this project.

- PCB v1.1 is the finished board revision intended for sale and ongoing support.
- PCB v1.0 is now a legacy revision kept for archive and partial backward compatibility notes.
- The v1.1 hardware goal is considered achieved: TEA5767 receive path, PT volume control, PAM8406 output stage, and RDS decode all work on the finished board.
- Audio quality on PCB v1.1 is clean. The input filter and separate LDO rails removed the audible switching-noise problem seen in earlier hardware iterations.
- Stereo playback is confirmed on the finished v1.1 board.
- RDS decode is now considered successful on real captures from the v1.1 MPXO path.

Software target and compatibility:

- The current firmware is written primarily for PCB v1.1.
- PCB v1.0 remains partially compatible, but does not provide the full v1.1 RDS path.
- TEA5767-only builds still work for users who only want FM tuning without the full dedicated PCB.

## Features

- TEA5767 FM control with manual tune (`76.0-108.0 MHz`) and seek.
- PT2257 and PT2259-S support for audio attenuation and mute over I2C.
- PAM8406 control from Flipper GPIO for power, mute, and Class AB/Class D mode selection.
- MPXO-based RDS receive path on `PA4`, including real-time decode and optional debug capture.
- RDS decode pipeline targeting pilot-derived carrier recovery (`carrier = 3 * pilot`).
- RDS decoding architecture conceptually inspired by the classic `SAA6588` block design, implemented fully in software on Flipper Zero.
- Constellation Visualizer view of the recovered `DBPSK` constellation (enabled by `RDS Debug`) with manual carrier offset control in `0.01 Hz` steps, bounded to `+/- 6.0 Hz`.
- Numbered RAW/meta debug dumps for offline analysis and decoder tuning.
- Preset storage and persistent app settings on SD card.
- Support-oriented debug workflow: if RDS is weak or unstable, users can send RAW/meta pairs for analysis.

## Controls

- `Left/Right` (short): tune `- / + 0.1 MHz`.
- `Left/Right` (hold): seek previous / next station.
- `OK` (short): mute or unmute audio.
- `OK` (hold): save or select the current preset.
- `Up/Down` (short): next or previous saved preset from `presets.fff`.
- `Up/Down` (hold or repeat): volume up/down by changing PT attenuation.
- `Back`: return to menu or exit the app.

Constellation view (`Menu -> Constellation Visualizer`, visible when `RDS Debug = On`):

- Top row: frequency (left), PS (center), carrier offset (right).
- `Left/Right` (short): carrier manual offset `- / + 0.01 Hz`.
- `Up/Down` (short): next/previous preset and restore both frequency and saved offset.
- `OK` (short): save/select preset with current frequency and current offset.
- `OK` (hold): create a numbered RAW/meta dump pair.
- Bottom status: transient capture/save status (`REC %`, `REC WR`, `REC OK`, `REC ERR`, `SAVE`).

## Current UI

The current listen screen is intentionally compact.

- Top line: station name from RDS PS if available, otherwise `Radio FM`.
- Frequency line: current tuned frequency.
- Audio line: `A:MT`, `A:ST`, or `A:MO`, plus current attenuation in dB.
- Tuning line: IF counter and IF error. The former `RF:` area is intentionally left empty in normal mode.
- With `RDS Debug` enabled, the tuning line shows compact RDS sync state as `R:<state>` in that empty area.
- RSSI line: current TEA5767 signal level and quality.

When `RDS Debug` is enabled, a separate Constellation view is also available from the main menu.

## Config Options

Open with `Menu -> Config`.

- `SNC`: TEA5767 Stereo Noise Cancelling.
- `De-emph`: TEA5767 de-emphasis (`50us` or `75us`).
- `HighCut`: TEA5767 high-cut filter.
- `Mono`: force mono reception.
- `Backlight`: keep Flipper backlight on while the app runs.
- `RDS`: enable or disable the RDS pipeline. This is now saved in settings and defaults to `On` if no saved setting exists.
- `RDS Debug`: enables compact debug information in the listen view and adds `Constellation Visualizer` in the main menu.
- `Volume`: mute/unmute from the config screen.
- `PT Chip`: select `PT2257` or `PT2259-S` protocol.
- `Amp Power`: enable or disable the PAM8406 output stage.
- `Amp Mode`: choose `AB` or `D`.

## Manual RDS Carrier Offset

The RDS demodulator derives the 57 kHz carrier from the stereo pilot (`carrier = 3 * pilot`). The Constellation Visualizer shows the recovered `DBPSK` symbol constellation after that demodulation stage. The view also adds a manual fine offset on top of the recovered carrier. It is deliberately manual: the value is useful for seeing how the `DBPSK` constellation rotates and why each station, receiver position, and local RF environment can need a slightly different correction.

The offset is stored per preset, so a saved station restores both frequency and its tuned RDS offset. There is no automatic PLL/autoset layer in this app; the manual control is part diagnostic tool, part educational view into the demodulator.

## RDS Debug Capture

When `RDS Debug` is enabled, holding `OK` in `Constellation Visualizer` creates a numbered dump pair on the SD card:

- RAW path pattern: `/ext/apps_data/fmradio_controller_pt2257/rds_capture_u16le_0001.raw`
- Meta path pattern: `/ext/apps_data/fmradio_controller_pt2257/rds_capture_meta_0001.txt`

Each new dump increments the index instead of overwriting the previous files.

These files are intended for support and decoder tuning:

- If RDS does not work well on a given station, send the matching RAW/meta pair.
- The RAW file captures the ADC stream from the MPXO path.
- The meta file records frequency, midpoint, measured sample rate, DMA stats, write stats, and capture status.

## Build and Release

This is a Flipper Zero external app (`application.fam`).

Local build workflows:

- `ufbt` (recommended):
  - `ufbt build`
  - `ufbt launch`
- `fbt`:
  - Clone Flipper firmware repo.
  - Copy or link this app into `applications_user/`.
  - Run `./fbt fap_fmradio_controller_pt2257`.

Automated releases (GitHub Actions):

- Workflow: `.github/workflows/release.yml`
- Release tag scheme remains repository-managed and may be adjusted independently from hardware revision numbering.

## Hardware

### Supported Boards

- TEA5767 external FM module.
- PT2259-S is the intended onboard volume-control IC for the dedicated PCB revisions.
- PT2257 remains supported for prototype and bench setups.
- PCB v1.1 is the current main board revision.
- PCB v1.0 is retained as a legacy reference revision.

### Audio Chain

Current production direction:

- TEA5767 L/R -> onboard PT stage -> onboard PAM8406 -> speakers
- TEA5767 MPXO -> MCP6001 front-end -> `PA4` ADC for RDS

Confirmed on PCB v1.1:

- Filter + LDO separation removed the audible switching-noise problem from the Flipper 5 V rail.
- Audio playback is clean and stereo.
- The onboard PAM8406 stage works as the main amplifier path.

### TEA5767 Module Pinout

- `3V3` (pin 9) -> TEA5767 VCC
- `GND` (pin 18) -> TEA5767 GND
- `SCL` (C0, pin 16) -> TEA5767 SCL
- `SDA` (C1, pin 15) -> TEA5767 SDA
- `PA4` (pin 4) -> MPXO / RDS ADC path on PCB v1.1

### PT2257 / PT2259-S Interface

- PT control shares the I2C bus with TEA5767.
- `SCL` (C0, pin 16) -> PT SCL
- `SDA` (C1, pin 15) -> PT SDA
- PT address is fixed to `0x88` on the dedicated PCB.

### PAM8406 GPIO Control on PCB v1.1

- `PB3` / Flipper pin 5 -> `PAM_MUTE`
- `PB2` / Flipper pin 6 -> `PAM_MODE_ABD`
- `PC3` / Flipper pin 7 -> `PAM_SHDN`

Mode logic used by the firmware:

- `MODE HIGH` -> Class D
- `MODE LOW` -> Class AB

The firmware writes mode before releasing shutdown, which matches the intended safe startup sequence.

### Compatibility Notes

- PCB v1.1 is the fully supported feature-complete revision.
- PCB v1.0 is not the main revision anymore.
- Minimal TEA-only setups can still use the FM control portion of the app.
- Prototype setups with PT2257 remain supported.

## PCB (Current Revision)

Product name:

- **FReD FM - Flipper Zero FM + RDS Radio Board**
- **FReD** stands for **Flipper Radio Experimental Decoder**.
- The name fits the board because PCB v1.1 is not only an FM receiver. With the companion software it is also a small RDS learning and experimentation platform with live `DBPSK` constellation view, manual carrier-offset testing, and real-station debug capture.

Current main revision:

- PCB v1.1 is now the main hardware revision for this project.
- PCB v1.1 is the revision intended for current Tindie sales.
- PCB v1.0 is retained only as the older archive / legacy revision.

Tindie listing:

- [FReD FM - Flipper Zero FM + RDS Radio Board](https://www.tindie.com/products/electronicstore/flipper-zero-fm-radio-board-with-pt-volume-control/)


### PCB v1.1 Summary

- One dedicated board combines TEA5767 tuning, PT-controlled volume, PAM8406 speaker drive, and the MPXO path for RDS.
- PCB v1.1 is the sale/support revision, not just a lab prototype.
- Separate filtering and cleaner power handling fixed the switching-noise problem seen on earlier hardware.
- Real hardware playback is confirmed as clean stereo on the finished board.

### RDS MPX Summary

- RDS is taken from the TEA5767 MPXO output, conditioned by the onboard analog front-end, and brought into the Flipper ADC on `PA4`.
- The decoder derives the RDS carrier from the stereo pilot using `carrier = 3 * pilot`, bounded to the standard `57000 +/- 6 Hz` window.
- The demodulation path is conceptually inspired by the `SAA6588` architecture: pilot-locked carrier recovery, `DBPSK` demodulation, block sync, and correction implemented in software.
- The Constellation Visualizer shows the recovered `DBPSK` constellation, which makes carrier trim and station quality visible on the device.
- The Constellation view adds a manual fine trim in `0.01 Hz` steps and stores that offset per preset.
- The app can save numbered RAW/meta captures, which makes real-station debug and offline tuning practical.
- Real captures from the PCB v1.1 MPXO path now decode valid RDS on finished hardware.

### PCB v1.1 Photos

| Full assembly version | Real PCB | Tindie sell option |
| --- | --- | --- |
| ![PCB v1.1 full assembly version](images/pcb/1_1/full_assambly_version.jpg) | ![PCB v1.1 real PCB view](images/pcb/1_1/real_pcb_view.jpg) | ![PCB v1.1 Tindie sell option](images/pcb/1_1/sell_option_div.jpg) |

### PCB v1.1 Renders

| Top render | Front render | Back render |
| --- | --- | --- |
| ![PCB v1.1 top render](images/pcb/1_1/1_capture-2026-04-15T13_19_23.801Z.jpeg) | ![PCB v1.1 front render](images/pcb/1_1/2_front_capture-2026-04-15T13_18_26.673Z.jpeg) | ![PCB v1.1 back render](images/pcb/1_1/3_back_capture-2026-04-15T13_18_40.008Z.jpeg) |

## App Screenshots

Current PCB v1.1 / current-UI screenshots:

|  |  |  |  |
| --- | --- | --- | --- |
| ![Current UI screenshot 1](images/Screenshot-20260504-131512.png) | ![Current UI screenshot 2](images/Screenshot-20260504-131541.png) | ![Current UI screenshot 3](images/Screenshot-20260504-131734.png) | ![Current UI screenshot 4](images/Screenshot-20260504-145422.png) |
| ![Current UI screenshot 5](images/Screenshot-20260504-145933.png) | ![Current UI screenshot 6](images/Screenshot-20260504-145949.png) | ![Current UI screenshot 7](images/Screenshot-20260504-145958.png) | ![Current UI screenshot 8](images/Screenshot-20260504-150029.png) |
| ![Current UI screenshot 9](images/Screenshot-20260504-150048.png) | ![Current UI screenshot 10](images/Screenshot-20260504-150058.png) | ![Current UI screenshot 11](images/Screenshot-20260504-150108.png) | ![Current UI screenshot 12](images/Screenshot-20260504-150129.png) |
| ![Current UI screenshot 13](images/Screenshot-20260504-150144.png) | ![Current UI screenshot 14](images/Screenshot-20260504-162131.png) |  |  |

These screenshots cover the current listening screen, menu flow, config pages, preset handling, and the RDS/Constellation workflow on PCB v1.1.

## Persistent Settings

Settings file:

- Path: `/ext/apps_data/fmradio_controller_pt2257/settings.fff`

Saved keys:

- `Freq10kHz`
- `PtAttenDb`
- `PtMuted`
- `PtChipType`
- `TeaSNC`
- `TeaDeemph75us`
- `TeaHighCut`
- `TeaForceMono`
- `BacklightKeepOn`
- `RdsEnabled`
- `RdsDebugEnabled`
- `AmpPower`
- `AmpModeClassD`

Default behavior when a setting is missing:

- `RDS` defaults to `On`.
- `RDS Debug` defaults to `Off`.

Presets file:

- Path: `/ext/apps_data/fmradio_controller_pt2257/presets.fff`
- Keys: `Count`, `Freq10kHz`, `Index`

## TEA5767 Audio Tuning

### SNC

- `On`: can reduce hiss/noise on weaker stations.
- `Off`: can sound more natural on strong stereo stations.

### De-emphasis

- `50us` is typical for EU.
- `75us` is typical for US.

### HighCut / Mono

- These are live TEA5767 receive-path options and can change how marginal stations sound.
- On strong local stations they may have little audible effect.

## Provenance and Licensing

- **Upstream origin**: https://github.com/victormico/flipperzero-radio
- **Upstream FlipC listing**: [![FlipC.org](https://flipc.org/victormico/flipperzero-radio/badge?nowerr=1)](https://flipc.org/victormico/flipperzero-radio?nowerr=1)
- **This version**: packaged as `[TEA+PT2257] FM Radio` (appid `fmradio_controller_pt2257`) to coexist with TEA5767-only builds.
- **Maintained by**: pchmielewski1
- **License**: GPLv3 (see `LICENSE`)

If you publish binaries built from modified sources, publish the corresponding source for that exact version and keep attribution.

## References

App inspired by [Radio](https://github.com/mathertel/Radio) for Arduino.

## Acknowledgements

Special thanks to [Derek Jamison](https://github.com/jamisonderek) for [Flipper Zero Tutorials](https://github.com/jamisonderek/flipper-zero-tutorials), and [Oleksii Kutuzov](https://github.com/oleksiikutuzov) for [Lightmeter](https://github.com/oleksiikutuzov/flipperzero-lightmeter), used as an app template.

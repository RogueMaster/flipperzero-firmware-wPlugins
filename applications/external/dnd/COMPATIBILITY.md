# Compatibility matrix

| Environment | Status | Notes |
|---|---|---|
| Flipper Zero | Target | 128x64 screen and five-way buttons; physical verification remains pending |
| RogueMaster branch 420 | Recipient verification pending | Source passes local strict host compilation; this source-only patch was not rebuilt with the firmware toolchain |
| Official firmware | Not verified | May require manifest/API adjustments |
| Momentum firmware | Not verified | May require manifest/API adjustments |
| SD card present | Required for persistence | Bundled references use app assets; profiles and other mutable user state use persistent app data |
| No SD card / removed SD | Unsupported for saves | App reports save/load failure; device behavior remains in the hardware matrix |
| Schema 1 | Unsupported | Outside the supported compatibility baseline |
| Schema 2 | Supported | Current checksummed text format with complete party presets |
| Unknown schemas | Preserved, not loaded | Files remain untouched and cannot be replaced by a blank autosave; future versions must add an explicit migration branch |

The source-only release does not bundle a compiled FAP. Build both targets using the command in `README.md`.

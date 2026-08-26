# Compatibility matrix

| Environment | Status | Notes |
|---|---|---|
| Flipper Zero | Target | 128x64 screen and five-way buttons; physical verification remains pending |
| RogueMaster branch 420 | Compiler verified | Both explicit FAP targets are built before release packaging |
| Official firmware | Not verified | May require manifest/API adjustments |
| Momentum firmware | Not verified | May require manifest/API adjustments |
| SD card present | Required for persistence | Bundled assets and character files use each application's asset namespace |
| No SD card / removed SD | Unsupported for saves | App reports save/load failure; device behavior remains in the hardware matrix |
| Schema 1 | Unsupported | Pre-release format; no migration is included |
| Schema 2 | Supported | Current checksummed text format with complete party presets |
| Pre-freeze and unknown schemas | Unsupported | No migration code is included in this pre-release build |

The source-only release does not bundle a compiled FAP. Build both targets using the command in `README.md`.

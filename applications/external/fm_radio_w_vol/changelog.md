v0.11:
- FReD FM catalog release (appid fred_fm)
- APP_DATA_PATH storage under /ext/apps_data/fred_fm/
- Unified fred_fm_* symbol naming across the app
- RDS capture backpressure fix for reliable full SD writes
- Source refactor: src/fred_fm/ + src/drivers/ layout, per-module headers, documented public APIs
- RDS DSP filter coefficients split into dedicated tap headers (HB1/HB2/HB3/FIR41)

v0.10:
- RDS decode pipeline stabilization and constellation debug view
- Numbered RAW/meta RDS capture dumps for offline tuning
- PCB v1.1 MPXO path support, preset carrier offset per station
- PAM8406 GPIO control, PT2257/PT2259-S volume, persistent settings

v0.9:
- Early FReD FM board support with TEA5767 tuning and PT volume control

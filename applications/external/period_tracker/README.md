# Period Tracker for Flipper Zero

A menstrual cycle tracking application for Flipper Zero. Track cycles for multiple people with predictions, alerts, and statistical learning.

![Period Tracker](period_tracker.png)

## Features

- **Multi-user support**: Track cycles for multiple people with individual profiles
- **Cycle predictions**: Next period, PMS, ovulation, and fertile windows
- **Event logging**: Log period starts and symptoms with a date picker
- **Statistical learning**: Adjusts cycle predictions based on logged data
- **Daily digest**: Upcoming events and alerts for all profiles at a glance
- **PIN protection**: Optional 4-digit PIN with lockout after failed attempts
- **Local storage**: All data on the SD card; no network or cloud
- **CSV format**: Event history in human-readable CSV files

## Installation

### Prerequisites

- Flipper Zero with firmware **1.3.4+** (or any compatible firmware)
- **ufbt** installed on your computer
- USB cable to connect Flipper

### Quick install

1. **Install ufbt** (if needed):
   ```bash
   python3 -m pip install --upgrade ufbt
   ```

2. **Clone this repository**:
   ```bash
   git clone https://github.com/gorshunovr/period_tracker.git
   cd period_tracker
   ```

3. **Build and install**:
   ```bash
   ufbt launch
   ```

The app installs to `/ext/apps/Tools/period_tracker.fap` on your Flipper.

### Manual installation

1. Build: `ufbt`
2. Copy `dist/period_tracker.fap` to `/ext/apps/Tools/` on the SD card
3. On Flipper: **Apps** → **Tools** → **Period Tracker**

## Usage

### First run

On first launch you will see a short onboarding screen. Press OK to create your first profile.

### Creating a profile

1. Main Menu → **Settings** → **Add New Profile**
2. Enter name and details (cycle length, period length, optional health notes)

### Logging a period

1. Select a profile from the main menu
2. Choose **Log Event** → **Period Start**
3. Pick the date and confirm

### Viewing predictions

1. Select a profile → **View Predictions**
2. See next period, PMS window, ovulation, and fertile window (where applicable)

### Daily digest

Shows alerts and upcoming events across all profiles: cycle day, expected symptoms, and upcoming period dates.

### PIN protection

1. Main Menu → **Settings** → **PIN Settings**
2. Set a 4-digit PIN (required on every launch when enabled)
3. After 4 failed attempts the app locks

**To reset a locked PIN:** delete `/ext/apps_data/period_tracker/pin.txt` and `pin.locked` via qFlipper or the SD card.

## Data storage

All data is stored on the SD card at `/ext/apps_data/period_tracker/`:

| File | Purpose |
| --- | --- |
| `{name}.profile` | Profile metadata |
| `{name}.csv` | Event log |
| `profiles.idx` | Active profile list |
| `settings.txt` | App settings |
| `pin.txt` | PIN (if enabled) |

### CSV format

```csv
date,event_type,value
2026-07-01,period_start,
2026-07-01,symptom,Cramps
2026-07-02,symptom,Headache
```

## Development

```bash
ufbt              # build
ufbt launch       # build and run on device
ufbt format       # format C sources
ufbt lint         # check formatting
ufbt clean        # clean build artifacts
```

API target: firmware **1.3.4+** (API 86.0+).

### Test data

Generate multi-profile sample data (dates relative to today):

```bash
cd test_data
python3 generate_test_data.py
```

Copy the generated `.profile`, `.csv`, `profiles.idx`, and `settings.txt` files to
`/ext/apps_data/period_tracker/` on the Flipper SD card. See [test_data/README.md](test_data/README.md).

## Limitations

- **No background alerts**: external apps cannot set RTC alarms; alerts appear when the app is open
- **Local only**: no network sync
- **Monochrome UI**: 128×64 text interface
- **Storage**: keep event history reasonable (under ~1000 entries per profile recommended)

## Contributing

1. Fork the repository
2. Create a feature branch
3. Format with `ufbt format` and test with `ufbt launch`
4. Open a pull request

## License

This project is licensed under the [MIT License](LICENSE).

## Support

- Issues: [GitHub Issues](https://github.com/gorshunovr/period_tracker/issues)
- Community: [forum.flipper.net](https://forum.flipper.net/)
- Flipper docs: [developer.flipper.net](https://developer.flipper.net/)

---

**Disclaimer**: This app is for informational purposes only and should not be used as a sole method of contraception or medical advice. Always consult healthcare professionals for medical decisions.

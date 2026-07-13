# Period Tracker

A menstrual cycle tracker for Flipper Zero. Track cycles for multiple people with predictions, alerts, and statistical learning. All data stays on the SD card.

## Features

- **Multi-user profiles** - track cycles for family members separately
- **Predictions** - next period, PMS window, ovulation, and fertile window
- **Event logging** - log period starts and symptoms with a date picker
- **Statistical learning** - predictions adapt from logged cycle history
- **Daily digest** - upcoming events and status for all profiles at a glance
- **Cycle statistics** - regularity classification and confidence ranges
- **PIN protection** - optional 4-digit PIN with lockout after failed attempts
- **Local storage** - CSV event logs and profile files on the SD card; no network or cloud

## How to use

1. Create a profile (name, average cycle length, optional health notes)
2. Log period starts and symptoms as they occur
3. Open **View Predictions** or **Daily Digest** for upcoming dates
4. Optionally enable PIN under **Settings**

## Data storage

- Files live on the Flipper SD card under /ext/apps_data/period_tracker/
- No telemetry and no internet access
- The SD card is not encrypted by this app; protect physical access as needed

## Important

This app is for informational purposes only. It is not medical advice and should not be used as a sole method of contraception. Consult a healthcare professional for medical decisions.

## Source and support

- Source: https://github.com/gorshunovr/period_tracker
- Issues: https://github.com/gorshunovr/period_tracker/issues

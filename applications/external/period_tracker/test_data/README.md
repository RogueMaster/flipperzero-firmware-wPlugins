# Test data

Generate realistic multi-profile sample data for device testing and screenshots.

## Generate

```bash
cd test_data
python3 generate_test_data.py
```

Creates (relative to this directory):

- `Alice`, `Emma`, `Sarah`, `Olivia` — active profiles (`.profile` + `.csv`)
- `TestArchived` — archived profile (filtered out by the app)
- `profiles.idx` — active profile list

Dates are computed from **today**, so regenerate whenever data feels stale.

`settings.txt` is a static template (alarm time, default symptom list, PIN flag).

## Install on Flipper

Copy generated files onto the SD card:

```text
/ext/apps_data/period_tracker/
```

Copy:

- `*.profile`, `*.csv`, `profiles.idx`, `settings.txt`

Optional PIN for testing: create `pin.txt` containing `1234` only if you want PIN entry on launch.

Do **not** copy `generate_test_data.py` or this README.

## Profiles

| Profile | Scenario |
| --- | --- |
| Alice | Regular 28-day cycles |
| Emma | PCOS, irregular cycles |
| Sarah | Contraception, light symptoms |
| Olivia | Perimenopause, irregular |
| TestArchived | Archived (must not appear in Select Profile) |

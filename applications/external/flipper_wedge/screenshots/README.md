# Screenshots for Flipper App Store

This directory contains screenshots required for the Flipper Application Catalog submission.

## Requirements

All screenshots **must** be taken using qFlipper's built-in screenshot feature:
- Do **NOT** resize or modify screenshots
- Keep original resolution and format from qFlipper
- Use descriptive filenames

## Required Screenshots

Please capture the following screenshots:

1. **`01_nfc_scanning.png`** - NFC scanning screen with connection status
2. **`02_rfid_scanning.png`** - RFID scanning screen in action
3. **`03_ndef_mode.png`** - NDEF mode showing text parsing capability
5. **`04_settings.png`** - Settings configuration screen

## How to Capture Screenshots

1. Connect your Flipper Zero to computer via USB
2. Open **qFlipper** application
3. Launch **Flipper Wedge** on your device
4. Navigate to the screen you want to capture
5. In qFlipper, click the **"Screenshot"** button (camera icon)
6. Save with the appropriate filename from the list above

## Important Notes

- The **first screenshot** (`01_main_menu.png`) will be used as the preview image in the catalog
- Minimum of 5 screenshots required
- You can add more screenshots if desired
- Do not commit this README.md to the catalog repo (only needed in source repo)

## After Capturing

Once screenshots are captured:

```bash
# Add screenshots to git
git add screenshots/*.png

# Commit
git commit -m "Add screenshots for Flipper App Store submission"
```

Then proceed with the submission steps in [FLIPPER_APP_STORE_SUBMISSION.md](../FLIPPER_APP_STORE_SUBMISSION.md).

# Flipper App Store Submission Guide

This document provides instructions for submitting **Flipper Wedge** to the official Flipper Application Catalog.

## Current Status

- ✅ Release branch created: `flipper_wedge_1.0`
- ✅ Manifest file created: `manifest.yml`
- ⚠️ **REQUIRED**: Screenshots need to be captured
- ⏳ Pending: Push release branch and submit to catalog

## Prerequisites Checklist

### 1. App Requirements
- ✅ App builds with uFBT
- ✅ Compatible with latest Release/RC firmware
- ✅ App icon exists (10x10px 1-bit PNG): `icons/flipper_wedge_10px.png`
- ⚠️ **Screenshots needed** (see below)

### 2. Repository Requirements
- ✅ Public GitHub repository
- ✅ README.md with clear documentation
- ✅ Changelog file: `docs/changelog.md`
- ✅ MIT License

### 3. Compliance
- ✅ No illegal activity
- ✅ Does not bypass Flipper Zero's intentional limits
- ✅ Complies with Play Store and App Store policies

## Required Screenshots

Screenshots **must** be taken using qFlipper's built-in screenshot feature:

### How to Capture Screenshots

1. **Connect Flipper Zero** to computer via USB
2. **Open qFlipper** application
3. **Launch Flipper Wedge** on your device
4. In qFlipper, click **"Screenshot"** button (camera icon)
5. Save screenshots with descriptive names

### Required Screenshots (5 minimum)

Create a `screenshots/` directory and capture:

1. **`01_main_menu.png`** - Main mode selection menu
2. **`02_nfc_scanning.png`** - NFC scanning screen with status
3. **`03_rfid_scanning.png`** - RFID scanning screen
4. **`04_ndef_mode.png`** - NDEF mode showing text parsing
5. **`05_settings.png`** - Settings configuration screen

**IMPORTANT**:
- Do **NOT** resize or modify screenshots
- Keep original resolution and format from qFlipper
- First screenshot will be the preview image in the catalog

## Manifest File Details

The `manifest.yml` file has been created with the following structure:

```yaml
id: flipper_wedge
name: "Flipper Wedge"
version: "1.0"
category: "Tools"
short_description: "Read RFID/NFC tags and type UIDs as HID keyboard input via USB or Bluetooth. Supports NDEF text records."
description: "@README.md"
changelog: "@docs/changelog.md"
author: "Dangerous Things"
icon: "icons/flipper_wedge_10px.png"
sourcecode:
  type: git
  location:
    origin: https://github.com/DangerousThings/flipper-wedge.git
    commit_sha: 114c1f897b3b4606123d8b2fc7e34f13811de733
screenshots:
  - './screenshots/01_main_menu.png'
  - './screenshots/02_nfc_scanning.png'
  - './screenshots/03_rfid_scanning.png'
  - './screenshots/04_ndef_mode.png'
  - './screenshots/05_settings.png'
```

### Key Fields Explained

- **`@README.md`**: Catalog will automatically load and format your README
- **`@docs/changelog.md`**: Changelog is pulled from this file
- **`commit_sha`**: Points to the exact commit of this release
- **Category**: "Tools" matches the `fap_category` in `application.fam`

## Submission Steps

### Step 1: Capture Screenshots

```bash
# Create screenshots directory
mkdir -p screenshots/

# Use qFlipper to capture 5+ screenshots
# Save them to the screenshots/ directory
```

### Step 2: Update Manifest Commit SHA (if needed)

If you make any changes after creating the manifest:

```bash
# Get the latest commit SHA
git rev-parse HEAD

# Update the commit_sha field in manifest.yml
```

### Step 3: Commit and Push Release Branch

```bash
# Add manifest and screenshots
git add manifest.yml screenshots/ FLIPPER_APP_STORE_SUBMISSION.md
git commit -m "Add Flipper App Store manifest and screenshots for v1.0"

# Push release branch to GitHub
git push origin flipper_wedge_1.0
```

### Step 4: Create GitHub Release Tag

```bash
# Create and push tag
git tag -a v1.0 -m "Flipper Wedge v1.0 - Initial Release"
git push origin v1.0
```

### Step 5: Submit to Flipper Application Catalog

1. **Fork** the catalog repository:
   - https://github.com/flipperdevices/flipper-application-catalog

2. **Clone your fork**:
   ```bash
   git clone https://github.com/YOUR_USERNAME/flipper-application-catalog.git
   cd flipper-application-catalog
   ```

3. **Create application directory**:
   ```bash
   mkdir -p applications/Tools/flipper_wedge
   ```

4. **Copy manifest.yml**:
   ```bash
   # Copy from your flipper-wedge repo to catalog repo
   cp /path/to/flipper-wedge/manifest.yml applications/Tools/flipper_wedge/
   ```

5. **Validate manifest** (optional but recommended):
   ```bash
   # The catalog repo has validation scripts
   # Check their documentation for validation commands
   ```

6. **Commit and push**:
   ```bash
   git add applications/Tools/flipper_wedge/manifest.yml
   git commit -m "Add Flipper Wedge v1.0 to catalog"
   git push origin main
   ```

7. **Create Pull Request**:
   - Go to: https://github.com/flipperdevices/flipper-application-catalog
   - Click "New Pull Request"
   - Compare: `main` ← `YOUR_USERNAME:main`
   - Title: `Add Flipper Wedge v1.0`
   - Description:
     ```markdown
     # Flipper Wedge v1.0

     ## App Description
     Read RFID/NFC tags and type UIDs as HID keyboard input via USB or Bluetooth.
     Supports NDEF text records.

     ## Features
     - RFID (125 kHz): EM4100, HID Prox, Indala
     - NFC (13.56 MHz): ISO14443A/B, MIFARE, NTAG
     - NDEF text record parsing
     - USB and Bluetooth HID keyboard output
     - 5 scanning modes

     ## Repository
     https://github.com/DangerousThings/flipper-wedge

     ## Checklist
     - [x] App builds with uFBT
     - [x] Compatible with latest firmware
     - [x] Screenshots taken with qFlipper
     - [x] Manifest validated
     - [x] README and changelog included
     ```

## Post-Submission

### Expected Timeline
- **Review time**: 1-2 business days (if properly formatted)
- **Availability**: App appears in catalog within minutes of PR merge

### Maintenance Requirements
- **Update frequency**: No specific requirement
- **Response time**: Must respond to catalog maintainers within 14 days
- **Version updates**: Increment `fap_version` in `application.fam` for each update

### Updating the App

When releasing a new version:

1. **Update version** in `application.fam`:
   ```python
   fap_version="1.1"  # Increment version
   ```

2. **Create new release branch**:
   ```bash
   git checkout -b flipper_wedge_1.1
   ```

3. **Update changelog**: Add new version section to `docs/changelog.md`

4. **Update manifest.yml**:
   - Update `version` field
   - Update `commit_sha` to new commit
   - Update `changelog` if significant changes

5. **Follow submission steps again** (PR to catalog repo)

## Troubleshooting

### Common Issues

**Build fails during catalog validation:**
- Ensure app builds with latest firmware
- Check for API compatibility issues
- Test with uFBT: `ufbt build`

**Screenshots rejected:**
- Must use qFlipper's screenshot feature
- Do not resize or modify images
- Keep original PNG format

**Manifest validation errors:**
- Check YAML syntax (indentation, quotes)
- Verify all required fields present
- Ensure commit_sha is valid 40-character hex

**App not appearing in catalog:**
- Check PR status on GitHub
- Verify branch/commit is accessible
- Ensure GitHub repository is public

## Resources

- **Flipper App Catalog**: https://github.com/flipperdevices/flipper-application-catalog
- **Contributing Guide**: https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Contributing.md
- **Manifest Documentation**: https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Manifest.md
- **qFlipper Download**: https://flipperzero.one/update

## Contact

If you encounter issues during submission:
- **GitHub Issues**: https://github.com/DangerousThings/flipper-wedge/issues
- **Flipper Forum**: https://forum.flipper.net/
- **Catalog Issues**: https://github.com/flipperdevices/flipper-application-catalog/issues

---

**Next Step**: Capture screenshots using qFlipper and commit them to the `screenshots/` directory.

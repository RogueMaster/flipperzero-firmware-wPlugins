# Quick Reference Cards

Fast lookup guide for common maintenance tasks. Keep this open while working.

---

## Quick Command Reference

### Build Commands

```bash
# Build for specific firmware
./build.sh official              # Official (latest stable tag)
./build.sh unleashed             # Unleashed (release branch)
./build.sh momentum              # Momentum (release branch)
./build.sh official --tag 1.3.4  # Specific version

# Build all firmwares
./build-all-firmwares.sh

# Deploy to connected Flipper
./deploy.sh official

# Clean build
cd /home/work/flipperzero-firmware
./fbt -c fap_flipper_wedge
```

### Git Commands

```bash
# Check status
git status

# Commit changes
git add <files>
git commit -m "Description of changes"

# Create release tag
git tag v1.1
git push origin v1.1

# View recent commits
git log --oneline -10
```

### Firmware Updates

```bash
# Update Official firmware
cd /home/work/flipperzero-firmware
git fetch --all --tags
git checkout <new-version>
git submodule update --init --recursive

# Repeat for other firmwares
cd /home/work/unleashed-firmware
# ... same commands ...
```

---

## File Location Quick Map

```
Project Root: /home/work/flipper wedge/

Core Files:
  hid_device.c/h              Main app entry point
  application.fam              App manifest

Helpers:
  helpers/hid_device_hid.c     USB/BT HID interface
  helpers/hid_device_nfc.c     NFC worker & NDEF
  helpers/hid_device_rfid.c    RFID worker
  helpers/hid_device_format.c  UID formatting
  helpers/hid_device_storage.c Settings persistence

Scenes:
  scenes/hid_device_scene_menu.c        Main menu
  scenes/hid_device_scene_startscreen.c Scanning screen
  scenes/hid_device_scene_settings.c    Settings screen

Documentation:
  CLAUDE.md                    Maintenance guide
  README.md                    User documentation
  docs/FIRMWARE_COMPATIBILITY.md  Firmware compatibility matrix
  docs/API_MIGRATION_LOG.md      API change history
  docs/TESTING_AUTOMATION.md     Testing guide
  docs/changelog.md              Version history

Build Scripts:
  build.sh                     Single firmware build
  deploy.sh                    Build + deploy
  build-all-firmwares.sh       Multi-firmware build
```

---

## Common Issues & Quick Fixes

### Build Failed

**Symptom**: `./build.sh` exits with errors

**Quick Fix**:
```bash
# 1. Check symlink
ls -l /home/work/flipperzero-firmware/applications_user/flipper_wedge

# 2. If missing, recreate
ln -s "/home/work/flipper wedge" /home/work/flipperzero-firmware/applications_user/flipper_wedge

# 3. Clean build
cd /home/work/flipperzero-firmware
./fbt -c fap_flipper_wedge

# 4. Retry
cd "/home/work/flipper wedge"
./build.sh official
```

### Warnings During Build

**Symptom**: Build succeeds but shows warnings

**Action**: Do NOT ignore!
```bash
# 1. Note the warning message
# 2. Check API_MIGRATION_LOG.md for similar issues
# 3. Fix the root cause (likely deprecated API)
# 4. Test thoroughly after fix
```

### App Crashes on Flipper

**Symptom**: App crashes when launched

**Quick Diagnosis**:
```bash
# 1. Check firmware version matches
# Build for correct firmware
./deploy.sh <firmware-that-matches-device>

# 2. If still crashes, check serial logs
# (Requires USB connection and serial monitor)
```

### Git Status Shows Untracked Files

**Symptom**: `dist/` folder shows up in git status

**Quick Fix**:
```bash
# dist/ should be in .gitignore
echo "dist/" >> .gitignore
git add .gitignore
git commit -m "Ignore dist directory"
```

---

## Testing Quick Commands

### Pre-Commit Checks

```bash
# Quick validation before commit
./build.sh official    # Must succeed
git status             # Review changes
git diff               # Review actual changes
```

### Pre-Release Checks

```bash
# Full build test
./build-all-firmwares.sh

# Verify outputs
ls -lh dist/*/

# Run manual hardware tests
# (See CLAUDE.md testing checklist)
```

### Firmware Compatibility Check

```bash
# Build against latest firmware
cd /home/work/flipperzero-firmware
git pull --recurse-submodules
cd "/home/work/flipper wedge"
./build.sh official

# Watch for new warnings or errors
```

---

## Critical Code Patterns

### Don't Remove These!

**NFC Auto-Restart** ([helpers/hid_device_nfc.c](../helpers/hid_device_nfc.c)):
```c
case NfcPollerEventTypeStopped:
    if (nfc->running) {
        // CRITICAL: Auto-restart on error
        nfc_poller_start(nfc->poller, callback, context);
    }
    break;
```

**HID Worker Thread Safety** ([helpers/hid_device_hid_worker.c](../helpers/hid_device_hid_worker.c)):
```c
// CRITICAL: Always use message queue for UI ↔ worker communication
furi_message_queue_put(worker->queue, &msg, FuriWaitForever);
```

**Settings Persistence** ([helpers/hid_device_storage.c](../helpers/hid_device_storage.c)):
```c
// CRITICAL: Don't change format without migration logic
// Format: key=value\n (plain text)
```

### Always Do These!

**Stop Workers Before HID Switch**:
```c
// ALWAYS stop workers before changing HID mode
hid_device_nfc_stop(app->nfc);
hid_device_rfid_stop(app->rfid);
// ... then change HID mode ...
hid_device_hid_worker_stop(app->hid_worker);
// ... switch ...
hid_device_hid_worker_start(app->hid_worker);
```

**Thread-Safe View Updates**:
```c
// ALWAYS use with_view_model for view updates
with_view_model(
    view,
    HidDeviceStartscreenModel* model,
    {
        // Modify model here
        model->status = "Scanning...";
    },
    true  // redraw
);
```

---

## API Quick Lookup

### NFC APIs

```c
// Create poller
NfcPoller* poller = nfc_poller_alloc(nfc_device, protocol);

// Start polling
nfc_poller_start(poller, callback, context);

// Stop polling
nfc_poller_stop(poller);

// Free
nfc_poller_free(poller);
```

### RFID APIs

```c
// Create worker
LfRfidWorker* worker = lfrfid_worker_alloc();

// Start reading
lfrfid_worker_start_read(worker, callback, context);

// Stop
lfrfid_worker_stop(worker);

// Free
lfrfid_worker_free(worker);
```

### USB HID APIs

```c
// Set USB HID config
furi_hal_usb_set_config(&usb_hid, NULL);

// Check connection
bool connected = furi_hal_usb_is_connected();

// Type key
furi_hal_hid_kb_press(HID_KEYBOARD_A);
furi_hal_hid_kb_release(HID_KEYBOARD_A);
```

### BT HID APIs

```c
// Set BT profile
bt_set_profile(bt, BtProfileHidKeyboard);

// Check connection
bool active = furi_hal_bt_is_active();

// Type key
furi_hal_bt_hid_kb_press(HID_KEYBOARD_A);
furi_hal_bt_hid_kb_release(HID_KEYBOARD_A);
```

---

## Version Release Checklist

Quick checklist for creating a new release:

```
[ ] Code changes complete
[ ] Builds on all 4 firmwares (no warnings)
[ ] Full hardware test suite passed
[ ] docs/changelog.md updated
[ ] docs/FIRMWARE_COMPATIBILITY.md updated
[ ] README.md version updated (if needed)
[ ] application.fam version bumped
[ ] Git commit: "Release vX.Y"
[ ] Git tag: vX.Y
[ ] Build FAPs for all firmwares: ./build-all-firmwares.sh
[ ] Upload FAPs to release
[ ] Push tag: git push origin vX.Y
```

---

## Firmware Compatibility Quick Check

| Firmware | Default Branch/Tag | Build Command |
|----------|-------------------|---------------|
| **Official** | Latest stable tag | `./build.sh official` |
| **Unleashed** | release | `./build.sh unleashed` |
| **Momentum** | release | `./build.sh momentum` |
| **RogueMaster** | release | `./build.sh roguemaster` |

**Quick Test**: If unsure which firmware to test, always test Official first.

---

## Emergency Rollback

If a change breaks everything:

```bash
# 1. Find last working commit
git log --oneline

# 2. Revert to it (creates new commit)
git revert <bad-commit-hash>

# 3. Or hard reset (destructive!)
git reset --hard <last-good-commit>

# 4. Rebuild
./build-all-firmwares.sh

# 5. Test thoroughly
```

---

## Useful Grep Patterns

```bash
# Find all NFC API usage
grep -r "nfc_poller" helpers/

# Find all HID API usage
grep -r "furi_hal_hid" helpers/

# Find all RFID API usage
grep -r "lfrfid_worker" helpers/

# Find TODO comments
grep -r "TODO" . --exclude-dir=dist

# Find all settings keys
grep "hid_device_storage_" helpers/hid_device_storage.c
```

---

## Log Files & Debug Output

```bash
# Check build logs
cd /home/work/flipperzero-firmware
./fbt fap_flipper_wedge 2>&1 | tee build.log

# Serial logs from Flipper (if connected)
# Requires: screen or minicom
screen /dev/ttyACM0 115200

# Or use fbt's built-in serial monitor
./fbt cli
```

---

## Documentation Update Checklist

After any significant change:

```
[ ] CLAUDE.md - Update if architecture/patterns changed
[ ] README.md - Update if user-facing features changed
[ ] docs/changelog.md - Always update for releases
[ ] docs/FIRMWARE_COMPATIBILITY.md - Update with new firmware versions tested
[ ] docs/API_MIGRATION_LOG.md - Update if API changes occurred
[ ] Code comments - Update if logic changed
```

---

## Support & Resources

**Flipper Docs**: https://docs.flipper.net/
**Official Firmware**: https://github.com/flipperdevices/flipperzero-firmware
**Unleashed Firmware**: https://github.com/DarkFlippers/unleashed-firmware
**Momentum Firmware**: https://github.com/Next-Flip/Momentum-Firmware
**Flipper Forum**: https://forum.flipper.net/

**Project Docs**:
- [CLAUDE.md](../CLAUDE.md) - Comprehensive maintenance guide
- [README.md](../README.md) - User documentation
- [BUILD_MULTI_FIRMWARE.md](../BUILD_MULTI_FIRMWARE.md) - Multi-firmware build guide

---

*Keep this file bookmarked for quick reference during maintenance work.*

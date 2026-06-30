# CLAUDE.md - Flipper Wedge Maintenance Guide

**Purpose**: Primary maintenance guide for AI assistants maintaining the Flipper Wedge Flipper Zero application.

**Focus**: Stability, cross-firmware compatibility, regression prevention.

---

## Quick Navigation

- **[ARCHITECTURE_PATTERNS.md](docs/ARCHITECTURE_PATTERNS.md)** - Critical code patterns and architecture details
- **[QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md)** - Commands, troubleshooting, quick fixes
- **[BUILD_MULTI_FIRMWARE.md](BUILD_MULTI_FIRMWARE.md)** - Build and deploy workflows
- **[FIRMWARE_COMPATIBILITY.md](docs/FIRMWARE_COMPATIBILITY.md)** - Tested versions and compatibility
- **[README.md](README.md)** - User documentation

---

## Project Status

**Current Version**: 1.0 (Feature-Complete)
**Status**: Active Maintenance
**Target**: Stability and multi-firmware compatibility

### Core Features (Complete)
✅ RFID (125 kHz) reading - EM4100, HID Prox, Indala
✅ NFC (13.56 MHz) reading - ISO14443A/B, MIFARE, NTAG
✅ NDEF parsing - Type 2, Type 4, Type 5 text records
✅ USB HID keyboard output
✅ Bluetooth HID keyboard output
✅ 5 scanning modes (NFC, RFID, NDEF, NFC+RFID, RFID+NFC)
✅ Dynamic USB/BLE switching (no app restart required)
✅ Configurable settings (delimiter, Enter key, output mode, vibration)
✅ Settings persistence
✅ Scan logging to SD card
✅ Haptic/LED/sound feedback
✅ Mode startup behavior (remember last mode or default)

### Supported Firmwares
- **Official** (Primary) - flipperzero-firmware
- **Unleashed** - unleashed-firmware
- **Momentum** (includes Xtreme) - Momentum-Firmware
- **RogueMaster** (Secondary) - roguemaster-firmware

---

## Maintenance Philosophy

### Primary Goals
1. **Stability First**: No new features without explicit user request
2. **Cross-Firmware Compatibility**: Test on all 4 firmwares before release
3. **Regression Prevention**: Validate all existing features after any change
4. **API Compatibility**: Monitor firmware API changes and adapt proactively
5. **User Experience**: Maintain consistent behavior across firmware versions

### What NOT to Do
- ❌ Add new features unless explicitly requested
- ❌ Refactor working code without a clear maintenance benefit
- ❌ Optimize code that isn't causing problems
- ❌ Change UX patterns without user feedback
- ❌ Update firmware dependencies without thorough testing

### When to Make Changes
- ✅ Firmware API breaks compatibility (required adaptation)
- ✅ Critical bugs affecting core functionality
- ✅ Security vulnerabilities
- ✅ User-reported issues with evidence
- ✅ Explicitly requested enhancements from maintainers

---

## Build & Deploy Workflows

See **[BUILD_MULTI_FIRMWARE.md](BUILD_MULTI_FIRMWARE.md)** for detailed build instructions.

### Quick Reference

```bash
# Build for specific firmware
./build.sh [official|unleashed|momentum|roguemaster]

# Build and deploy to connected Flipper
./deploy.sh [firmware]

# Build for all firmwares
./build-all-firmwares.sh
```

**Output**: `dist/<firmware>/<version>/flipper_wedge.fap`

### Firmware Update Detection

Build scripts automatically detect firmware version changes:

```
⚠️ FIRMWARE VERSION CHANGED: release → tag 1.3.5 (a1b2c3d)
```

**When you see this**:
1. Read firmware changelog for API changes
2. Run regression tests after building
3. Check for deprecation warnings
4. Test on actual hardware before distribution

---

## Testing & Validation Protocols

### Pre-Release Testing Checklist

Before releasing any update, **ALL** tests must pass on **ALL** supported firmwares.

#### Build Validation
- [ ] Official firmware: builds without warnings
- [ ] Unleashed firmware: builds without warnings
- [ ] Momentum firmware: builds without warnings
- [ ] RogueMaster firmware: builds without warnings (or document known issues)

#### Core Functionality Tests

**NFC Reading** (test on each firmware)
- [ ] ISO14443A tag UID read correctly
- [ ] 4-byte UID formatted correctly
- [ ] 7-byte UID formatted correctly
- [ ] Tag removal detected
- [ ] Multiple consecutive scans work

**RFID Reading** (test on each firmware)
- [ ] EM4100 tag UID read correctly
- [ ] HID Prox tag works (if available)
- [ ] Tag removal detected
- [ ] Multiple consecutive scans work

**NDEF Parsing** (test on each firmware)
- [ ] Type 2 NDEF text record parsed (NTAG)
- [ ] Type 4 NDEF text record parsed (if available)
- [ ] Type 5 NDEF text record parsed (if available)
- [ ] Non-NDEF tag shows error in NDEF mode
- [ ] Non-NDEF tag outputs UID in NFC/combo modes

**HID Output** (test on each firmware)
- [ ] USB HID connection detected
- [ ] USB HID types characters correctly
- [ ] USB HID Enter key works when enabled
- [ ] BT HID pairing works
- [ ] BT HID types characters correctly
- [ ] Dual output (USB + BT) works simultaneously
- [ ] "Connect USB or BT HID" shown when disconnected

**Scan Modes** (test on each firmware)
- [ ] NFC Only: scan → output UID → cooldown
- [ ] RFID Only: scan → output UID → cooldown
- [ ] NDEF Mode: scan NDEF tag → output text → cooldown
- [ ] NDEF Mode: scan non-NDEF → red LED → no output
- [ ] NFC+RFID: both scanned → combined output
- [ ] NFC+RFID: timeout after 5s if second tag missing
- [ ] RFID+NFC: both scanned → combined output
- [ ] RFID+NFC: timeout after 5s if second tag missing

**Settings & Persistence** (test on each firmware)
- [ ] Delimiter changes apply to output
- [ ] Append Enter toggle works
- [ ] Output mode (USB/BLE) switches dynamically without restart
- [ ] Vibration level changes work
- [ ] Scan logging to SD works when enabled
- [ ] Mode startup behavior persists across restarts
- [ ] Settings persist across app restarts

**UI/UX** (test on each firmware)
- [ ] Mode selector navigates correctly
- [ ] Back button returns to previous screen
- [ ] Status messages display clearly
- [ ] Haptic feedback fires on scan
- [ ] LED feedback works (green=success, red=error)
- [ ] BT pairing instructions display correctly

#### Edge Case Tests
- [ ] Tag left on reader: ignored until removed
- [ ] Rapid consecutive scans: cooldown prevents spam
- [ ] Very long NDEF payload: truncated gracefully
- [ ] App survives USB disconnect/reconnect
- [ ] App survives BT disconnect/reconnect
- [ ] No crashes during normal operation
- [ ] No memory leaks (run app for extended period)

### Regression Test Protocol

After **any** code change (bug fix, firmware adaptation, etc.):

1. **Build on all firmwares** - Ensure no new warnings
2. **Run core functionality tests** (minimum)
3. **Test the specific area changed** (thorough)
4. **Check for unintended side effects** in related code
5. **Verify settings persistence** still works
6. **Document any behavioral changes**

---

## Firmware Update Procedures

### When a New Firmware Version is Released

Follow this procedure **for each supported firmware** when a new version is released:

#### Step 1: Update Local Firmware Clone
```bash
cd /home/work/flipperzero-firmware  # or unleashed/momentum/roguemaster
git fetch --all --tags
git checkout <new-version>  # or release branch
git submodule update --init --recursive
```

#### Step 2: Review Firmware Changelog
- Read the firmware's CHANGELOG.md or release notes
- Look for **API changes** affecting:
  - NFC/RFID workers
  - HID keyboard (USB/BT)
  - GUI components (ViewDispatcher, SceneManager, etc.)
  - Storage/settings APIs
  - Notification/feedback APIs

#### Step 3: Build and Check for Issues
```bash
cd "/home/work/flipper wedge"
./build.sh <firmware>
```

Watch for:
- **Compilation errors** → API breaking change
- **New warnings** → Deprecation or API change
- **Missing symbols** → Removed API functions
- **Link errors** → Library changes

#### Step 4: Address API Changes

If APIs have changed, follow this pattern:

##### Pattern 1: Deprecated API
```c
// Old API (deprecated)
nfc_worker_start(worker, callback);

// New API
nfc_worker_start_ex(worker, callback, context);

// Solution: Update calls, test thoroughly
```

##### Pattern 2: Removed API
```c
// Old API (removed)
furi_hal_usb_hid_kb_press(key);

// New API (different module)
usb_hid_keyboard_press(hid, key);

// Solution: Find replacement in firmware docs, update code
```

##### Pattern 3: Changed Behavior
```c
// Example: NFC poller now requires explicit restart after error
// Old: Auto-restarts on error
// New: Must call nfc_poller_restart() manually

// Solution: Update error handling logic
```

**Important**: Always check the firmware's migration guide or API documentation.

#### Step 5: Run Full Regression Tests
- [ ] Build succeeds on updated firmware
- [ ] Run complete testing checklist (see above)
- [ ] Test on actual hardware with the new firmware flashed
- [ ] Verify no behavioral changes in existing features

#### Step 6: Update Other Firmwares
- Repeat Steps 1-5 for **all** supported firmwares
- Document any firmware-specific quirks or workarounds

#### Step 7: Update Build Scripts (if needed)
- If default branches/tags change, update build.sh defaults
- Update BUILD_MULTI_FIRMWARE.md with new version info

#### Step 8: Document Changes
Update [docs/changelog.md](docs/changelog.md) with:
- Firmware versions tested
- Any code changes made for compatibility
- Known issues or workarounds

---

## Code Architecture

See **[docs/ARCHITECTURE_PATTERNS.md](docs/ARCHITECTURE_PATTERNS.md)** for detailed architecture patterns and critical code sections.

### Critical Patterns (Never Remove!)

1. **NFC Auto-Restart** - Auto-recovers from NFC poller failures
2. **HID Worker Thread** - Non-blocking Bluetooth HID operations
3. **Dynamic Output Mode Switching** - USB ↔ BLE without restart
4. **Scan State Machine** - Lifecycle management for single/combo modes
5. **Settings Persistence** - Immediate save in callbacks (never rely only on Back button)

### Key Files

```
flipper_wedge/
├── hid_device.c/h               # Main app, state machine
├── helpers/
│   ├── hid_device_hid_worker.c  # HID worker thread
│   ├── hid_device_nfc.c         # NFC + NDEF parsing
│   ├── hid_device_rfid.c        # RFID worker
│   └── hid_device_storage.c     # Settings persistence
└── scenes/
    ├── hid_device_scene_startscreen.c  # Main scanning UI
    └── hid_device_scene_settings.c     # Settings UI
```

---

## Monitoring Firmware API Changes

### Critical APIs to Watch

Monitor these firmware areas for breaking changes:

#### 1. NFC APIs
**Files to watch**: `lib/nfc/`, `furi_hal_nfc.h`

**Current usage**:
- `nfc_poller_alloc/start/stop/free()` - NFC poller lifecycle
- `iso14443_4a_poller_send_apdu()` - Type 4 NDEF
- `iso15693_poller_read_single_block()` - Type 5 NDEF

**Location**: [helpers/hid_device_nfc.c](helpers/hid_device_nfc.c)

#### 2. RFID APIs
**Files to watch**: `lib/lfrfid/`, `furi_hal_rfid.h`

**Current usage**:
- `lfrfid_worker_alloc/start_read/stop/free()` - RFID worker lifecycle
- Protocol-specific UID extraction (EM4100, HID Prox, Indala)

**Location**: [helpers/hid_device_rfid.c](helpers/hid_device_rfid.c)

#### 3. HID APIs
**Files to watch**: `furi_hal_usb_hid.h`, `furi_hal_bt_hid.h`

**USB HID**:
- `furi_hal_usb_set_config(&usb_hid, NULL)` - Enable USB HID
- `furi_hal_hid_kb_press/release()` - Type keys

**BT HID**:
- `bt_set_profile(bt, BtProfileHidKeyboard)` - Enable BT HID
- `furi_hal_bt_hid_kb_press/release()` - Type keys

**Location**: [helpers/hid_device_hid.c](helpers/hid_device_hid.c)

#### 4. GUI/Scene APIs
**Files to watch**: `gui/`, `scene_manager.h`

**Current usage**:
- `ViewDispatcher`, `SceneManager` - Navigation
- `Submenu`, `VariableItemList` - Standard widgets

**Location**: [hid_device.c](hid_device.c), [scenes/](scenes/)

### How to Monitor

1. **Subscribe to firmware release notifications** on GitHub
2. **Read changelogs** before updating
3. **Search for "BREAKING"** in release notes
4. **Check API header files** for deprecation warnings
5. **Build regularly** against latest firmware to catch issues early

---

## Troubleshooting

See **[docs/QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md)** for common issues and quick fixes.

### Most Common Issues

**Build fails**: Check symlink, rebuild firmware, check API changes
**App crashes**: Verify firmware version matches, rebuild FAP
**Tags not detected**: Test with official apps, check auto-restart logic
**Settings not persisting**: Check SD card, verify immediate save in callbacks

---

## Quality Gates for Updates

### Before Committing Any Code Change

- [ ] Code compiles on **all** supported firmwares without warnings
- [ ] Change is **necessary** (bug fix, firmware adaptation, or requested feature)
- [ ] Change does **not** break existing functionality (regression test)
- [ ] Change follows **existing code style** and patterns
- [ ] No over-engineering or unnecessary refactoring

### Before Tagging a Release

- [ ] **Full regression test** passes on all firmwares
- [ ] All changes documented in [docs/changelog.md](docs/changelog.md)
- [ ] README.md updated if user-facing changes
- [ ] Build scripts tested and working
- [ ] FAP files built for all firmwares
- [ ] Version number incremented in `application.fam`
- [ ] Git tag created with version number

### Release Testing Matrix

Test **every combination**:

| Firmware | NFC | RFID | NDEF | USB HID | BT HID | Settings |
|----------|-----|------|------|---------|--------|----------|
| Official | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Unleashed | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Momentum | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| RogueMaster | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ⚠️ |

**Legend**:
- ✅ Fully tested and working
- ⚠️ Tested with known issues documented
- ❌ Not working (blocking issue)

**Goal**: All checkmarks green before release (RogueMaster warnings acceptable if documented).

---

## Communication Protocol

### When Firmware Updates Break Compatibility

If a firmware update introduces breaking API changes:

1. **Document the issue** in a new GitHub issue
2. **Identify the breaking change** (API function, behavior change, etc.)
3. **Find the new API** in firmware source or docs
4. **Implement fix** following existing patterns
5. **Test thoroughly** on affected firmware
6. **Verify other firmwares** still work
7. **Update changelog** with firmware-specific notes

### When to Ask for Help

Ask the user for guidance when:
- Multiple equally valid solutions exist for an API change
- Firmware behavior changed without clear migration path
- Test results are ambiguous or inconsistent
- Major architectural decision needed

Provide:
- Clear description of the problem
- Firmware versions affected
- Potential solutions with trade-offs
- Recommendation based on project goals

---

## Final Notes

### Stability is the Goal

This project is **feature-complete**. The goal is to **maintain stability** as firmwares evolve, not to add new features.

**Resist the temptation to**:
- Refactor code that works
- Add "nice to have" features
- Optimize without profiling
- Change UX without user feedback

**Focus energy on**:
- Keeping up with firmware API changes
- Fixing reported bugs
- Maintaining cross-firmware compatibility
- Improving test coverage
- Documenting quirks and workarounds

### Cross-Firmware Compatibility is Critical

Users rely on **all four firmwares**. A release that breaks one firmware is unacceptable.

**Always**:
- Test on all firmwares before releasing
- Document firmware-specific issues
- Provide workarounds when possible
- Keep build scripts up to date

### When in Doubt, Test More

If uncertain about a change:
- Run more tests
- Test on more firmwares
- Test on actual hardware
- Ask for user validation

**Better to be slow and stable than fast and broken.**

---

## Documentation Index

- **[CLAUDE.md](CLAUDE.md)** - This file (maintenance guide for AI)
- **[ARCHITECTURE_PATTERNS.md](docs/ARCHITECTURE_PATTERNS.md)** - Code patterns and critical architecture
- **[QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md)** - Commands, troubleshooting, quick lookup
- **[BUILD_MULTI_FIRMWARE.md](BUILD_MULTI_FIRMWARE.md)** - Build and deploy workflows
- **[FIRMWARE_COMPATIBILITY.md](docs/FIRMWARE_COMPATIBILITY.md)** - Compatibility matrix
- **[API_MIGRATION_LOG.md](docs/API_MIGRATION_LOG.md)** - Historical API changes
- **[TESTING_AUTOMATION.md](docs/TESTING_AUTOMATION.md)** - Testing strategy
- **[changelog.md](docs/changelog.md)** - Version history
- **[README.md](README.md)** - User documentation

---

*This document is the primary reference for maintaining the Flipper Wedge app. Update it when new patterns or firmware-specific quirks are discovered.*

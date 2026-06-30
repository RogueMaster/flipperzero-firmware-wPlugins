# API Migration Log

This document tracks all firmware API changes that affected the Flipper Wedge app and how we adapted to them.

**Purpose**: Maintain a historical record of API changes to help with future migrations and understand the codebase evolution.

---

## Format

Each entry documents:
- **Date**: When the migration was made
- **Firmware(s) Affected**: Which firmware(s) introduced the change
- **API Changed**: What API was affected
- **Change Type**: Deprecated, Removed, Behavioral Change, New API
- **Old Code**: How we used to do it
- **New Code**: How we do it now
- **Reason**: Why the firmware changed the API
- **Impact**: What broke and how severe
- **Testing**: How we verified the migration

---

## Migration Entries

### 2024-11-24: Initial Release (No Migrations)

**Status**: No migrations required for initial release. All APIs are current as of firmware 0.105.0.

**Baseline APIs**:
- NFC: `nfc_poller_*` API family
- RFID: `lfrfid_worker_*` API family
- USB HID: `furi_hal_usb_*` and `furi_hal_hid_*`
- BT HID: `furi_hal_bt_*` and `bt_set_profile()`
- GUI: `ViewDispatcher`, `SceneManager`, standard widgets
- Storage: `storage_*` API family

---

## Template for Future Migrations

When a migration is needed, copy this template and fill it out:

### YYYY-MM-DD: [Brief Description]

**Date**: YYYY-MM-DD
**Firmware(s) Affected**: [Official/Unleashed/Momentum/RogueMaster/All]
**Firmware Version**: [tag/commit where change was introduced]
**API Changed**: [module and function names]
**Change Type**: [Deprecated/Removed/Behavioral Change/New API Required]

#### Old Code
```c
// How we used to do it
nfc_worker_start(worker, callback);
```

#### New Code
```c
// How we do it now
nfc_worker_start_ex(worker, callback, context);
```

#### Change Details

**Reason for API Change**:
- [Why the firmware team changed this]
- [Link to firmware PR/issue if available]

**Impact Severity**: [Critical/High/Medium/Low]
- Critical: App won't build or crashes
- High: Major feature broken
- Medium: Minor feature broken or deprecated warning
- Low: Cosmetic or optional feature affected

**What Broke**:
- [List of features/files affected]
- [Error messages seen]

**Files Modified**:
- `helpers/hid_device_nfc.c:123` - Updated worker initialization
- `helpers/hid_device_nfc.h:45` - Updated function signature
- [etc.]

#### Migration Steps

1. [Step-by-step process used to migrate]
2. [Search/replace patterns used]
3. [Manual changes needed]
4. [Testing performed]

#### Testing Verification

- [ ] Builds without warnings on Official
- [ ] Builds without warnings on Unleashed
- [ ] Builds without warnings on Momentum
- [ ] Builds without warnings on RogueMaster
- [ ] Feature works on all firmwares
- [ ] No regressions in other features
- [ ] Settings persistence still works

#### Compatibility Notes

**Backward Compatibility**: [Yes/No/Partial]
- [Can old firmware still run the new code?]
- [Can new firmware run old code?]

**Cross-Firmware Impact**:
- Official: [affected/not affected/workaround needed]
- Unleashed: [affected/not affected/workaround needed]
- Momentum: [affected/not affected/workaround needed]
- RogueMaster: [affected/not affected/workaround needed]

#### References

- Firmware PR: [link to PR that introduced change]
- Firmware Issue: [link to related issue]
- Discussion: [link to forum/discord discussion]
- Commit: [link to our commit that fixed it]

---

## Common Migration Patterns

### Pattern 1: Function Renamed with Deprecation Warning

**Scenario**: Firmware deprecates old function, adds new one with different name.

**Example**:
```c
// Old (deprecated, still works but warns)
nfc_worker_start(worker, callback);

// New (recommended)
nfc_worker_start_ex(worker, callback, context);
```

**Migration Strategy**:
1. Search codebase for old function name
2. Replace with new function, add required parameters
3. Test on all firmwares
4. Verify no warnings

**Timeline**: Migrate immediately to avoid future breakage.

---

### Pattern 2: Function Removed, New API Required

**Scenario**: Firmware removes function entirely, new API is different.

**Example**:
```c
// Old (removed, will not compile)
furi_hal_usb_hid_kb_press(key);

// New (different module/structure)
UsbHid* hid = furi_hal_usb_hid_get();
usb_hid_keyboard_press(hid, key);
```

**Migration Strategy**:
1. Find replacement API in firmware headers
2. Update initialization code to get new handle
3. Update all call sites
4. Test thoroughly (behavior may differ)

**Timeline**: Immediate, blocks build.

---

### Pattern 3: Behavioral Change (Same API, Different Behavior)

**Scenario**: Function signature unchanged but behavior is different.

**Example**:
```c
// Old behavior: Auto-restarts on error
nfc_poller_start(poller, callback, context);

// New behavior: Stops on error, must manually restart
nfc_poller_start(poller, callback, context);
// Now need to handle restart in callback:
if (event == NfcPollerEventTypeStopped && should_continue) {
    nfc_poller_start(poller, callback, context);
}
```

**Migration Strategy**:
1. Read firmware changelog carefully
2. Update callback/error handling logic
3. Add state tracking if needed
4. Test edge cases (errors, timeouts, etc.)

**Timeline**: May not be obvious, test thoroughly.

---

### Pattern 4: New Mandatory Dependency

**Scenario**: Firmware refactors module, requires new includes/dependencies.

**Example**:
```c
// Old
#include <furi_hal_nfc.h>

// New (refactored)
#include <lib/nfc/nfc_poller.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
```

**Migration Strategy**:
1. Update includes
2. Check `application.fam` for new dependencies
3. Update linker flags if needed
4. Rebuild and verify

**Timeline**: Immediate, blocks build.

---

## Preventive Strategies

### 1. Monitor Deprecation Warnings

Build regularly against latest firmware dev branches to catch deprecation warnings early.

```bash
# Weekly check against dev branches
./build.sh official --branch dev
./build.sh unleashed --branch dev
./build.sh momentum --branch dev
```

### 2. Subscribe to Firmware Changes

Watch these files in firmware repos for changes:
- `furi_hal_usb*.h` - USB HID changes
- `furi_hal_bt*.h` - Bluetooth changes
- `lib/nfc/nfc*.h` - NFC API changes
- `lib/lfrfid/lfrfid*.h` - RFID API changes
- `gui/` - GUI widget changes
- `storage/` - Storage API changes

### 3. Read Firmware Changelogs

Before updating firmware versions, always read:
- Official: `CHANGELOG.md` in flipperzero-firmware
- Unleashed: Release notes on GitHub
- Momentum: `CHANGELOG.md` in Momentum-Firmware
- RogueMaster: Check releases page

### 4. Test Early, Test Often

When firmware updates are released:
1. Build against new version immediately
2. Run basic smoke tests (even if build succeeds)
3. Check for behavioral changes
4. Document findings here

---

## API Stability Assessment

### Stable APIs (Unlikely to Change)
- Core Furi APIs (`furi_thread_*`, `furi_message_queue_*`)
- Basic HAL (`furi_hal_gpio_*`, `furi_hal_power_*`)
- Standard widgets (`Submenu`, `VariableItemList`)

### Moderately Stable APIs
- USB/BT HID (changes 1-2 times per year)
- Storage API (occasional refactoring)
- GUI core (`ViewDispatcher`, `SceneManager`)

### Volatile APIs (Change Frequently)
- **NFC stack** (actively developed, frequent changes)
- **RFID stack** (moderate changes)
- Notification system (occasional refactoring)

**Strategy**: Focus monitoring on volatile APIs.

---

## Migration Checklist

When migrating to a new API:

- [ ] Identify all files using the old API
- [ ] Read firmware documentation for new API
- [ ] Update code incrementally (one module at a time)
- [ ] Build after each change
- [ ] Test affected feature on all firmwares
- [ ] Run full regression test suite
- [ ] Update this document with migration details
- [ ] Update FIRMWARE_COMPATIBILITY.md with new firmware versions
- [ ] Commit changes with clear message: "Migrate to <new API> for <firmware>"

---

## Historical Context

### Why This Log Exists

Firmware APIs evolve rapidly. This log serves as:
1. **Historical record** of what changed and why
2. **Migration guide** for future API changes
3. **Compatibility reference** for understanding firmware differences
4. **Knowledge base** for new maintainers

### How to Use This Log

**When a firmware update breaks the build**:
1. Check this log for similar past migrations
2. Use the patterns documented here
3. Document your migration following the template

**When reviewing code**:
1. Reference this log to understand why code changed
2. Check if old patterns are still valid
3. Identify code that may need future migration

---

*Update this log every time an API migration is performed. Future you will be grateful.*

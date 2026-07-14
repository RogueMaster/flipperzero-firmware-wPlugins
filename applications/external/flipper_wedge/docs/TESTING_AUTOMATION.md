# Testing Automation Guide

This document provides guidance for automating regression tests to ensure firmware compatibility and prevent regressions as the codebase evolves.

---

## Current Testing Status

**Manual Testing**: ✅ Comprehensive test checklist in [CLAUDE.md](../CLAUDE.md#testing--validation-protocols)
**Automated Testing**: ⚠️ Partial (build validation only)
**CI/CD**: ❌ Not yet implemented

**Goal**: Automate repetitive testing while maintaining quality for hardware-dependent tests.

---

## Testing Pyramid

```
                   ┌─────────────────┐
                   │  Hardware Tests │  ← Manual (requires Flipper + tags)
                   │  (End-to-End)   │
                   └─────────────────┘
                  ┌───────────────────┐
                  │ Integration Tests │  ← Partially Automatable
                  │ (Module combos)   │
                  └───────────────────┘
               ┌────────────────────────┐
               │   Unit Tests           │  ← Fully Automatable
               │   (Helper functions)   │
               └────────────────────────┘
            ┌──────────────────────────────┐
            │  Build Tests                 │  ← Fully Automated
            │  (Compilation & warnings)    │
            └──────────────────────────────┘
```

---

## Automated Build Testing

### Current Implementation

The build scripts (`build.sh`, `build-all-firmwares.sh`) provide basic automation:

```bash
# Automated multi-firmware builds
./build-all-firmwares.sh
```

**What it tests**:
- ✅ Code compiles on all firmwares
- ✅ No linker errors
- ⚠️ Warnings (shown but not enforced)

**What it doesn't test**:
- ❌ Runtime behavior
- ❌ API compatibility
- ❌ Feature functionality

### Enhanced Build Testing Script

Create `scripts/test-builds.sh`:

```bash
#!/bin/bash
# Automated build testing for all firmwares
# Exit on first error, track warnings as failures

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(dirname "$SCRIPT_DIR")"
FAILED_BUILDS=()
WARNINGS_FOUND=()

echo "========================================="
echo "Automated Build Test Suite"
echo "========================================="
echo ""

# Test build for each firmware
test_firmware_build() {
    local firmware=$1
    local firmware_name=$2

    echo "Testing build: $firmware_name"
    echo "---"

    # Run build and capture output
    if output=$(cd "$APP_DIR" && ./build.sh "$firmware" 2>&1); then
        # Check for warnings
        if echo "$output" | grep -qi "warning:"; then
            echo "⚠️  $firmware_name: Warnings detected"
            WARNINGS_FOUND+=("$firmware_name")
        else
            echo "✅ $firmware_name: Clean build"
        fi
    else
        echo "❌ $firmware_name: Build failed"
        FAILED_BUILDS+=("$firmware_name")
    fi

    echo ""
}

# Test all firmwares
test_firmware_build "official" "Official"
test_firmware_build "unleashed" "Unleashed"
test_firmware_build "momentum" "Momentum"
test_firmware_build "roguemaster" "RogueMaster"

# Summary
echo "========================================="
echo "Build Test Summary"
echo "========================================="

if [ ${#FAILED_BUILDS[@]} -eq 0 ] && [ ${#WARNINGS_FOUND[@]} -eq 0 ]; then
    echo "✅ All builds passed with no warnings"
    exit 0
else
    if [ ${#FAILED_BUILDS[@]} -gt 0 ]; then
        echo "❌ Failed builds: ${FAILED_BUILDS[*]}"
    fi
    if [ ${#WARNINGS_FOUND[@]} -gt 0 ]; then
        echo "⚠️  Builds with warnings: ${WARNINGS_FOUND[*]}"
    fi
    exit 1
fi
```

**Usage**:
```bash
chmod +x scripts/test-builds.sh
./scripts/test-builds.sh
```

---

## Unit Testing Strategy

### Testable Modules

These modules can be unit tested without hardware:

**1. UID Formatting** ([helpers/hid_device_format.c](../helpers/hid_device_format.c))
- Input: Raw UID bytes
- Output: Formatted string with delimiter
- Test cases:
  - 4-byte UID with space delimiter
  - 7-byte UID with colon delimiter
  - 10-byte UID with no delimiter
  - Empty UID (edge case)
  - Max length UID (edge case)

**2. Settings Parsing** ([helpers/hid_device_storage.c](../helpers/hid_device_storage.c))
- Input: Key-value pairs
- Output: Parsed settings struct
- Test cases:
  - Valid settings file
  - Missing keys (defaults)
  - Invalid values (validation)
  - Corrupted file (error handling)

**3. NDEF Text Decoding** (within [helpers/hid_device_nfc.c](../helpers/hid_device_nfc.c))
- Input: NDEF message bytes
- Output: Decoded UTF-8 text
- Test cases:
  - UTF-8 text record
  - UTF-16 text record
  - Multi-record message (first text record)
  - Invalid NDEF structure
  - Empty message

### Unit Test Framework Options

**Option 1: Flipper's Built-in Unity Framework**
- Pros: Integrated with firmware build system
- Cons: Requires firmware environment

**Option 2: Standalone CMock/Unity**
- Pros: Can run on host machine without Flipper
- Cons: Requires mocking Furi APIs

**Recommendation**: Start with Option 1 for faster integration.

### Example Unit Test

Create `tests/test_format.c`:

```c
#include <furi.h>
#include "../helpers/hid_device_format.h"
#include "unity.h"

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
}

void test_format_4byte_uid_space_delimiter(void) {
    uint8_t uid[] = {0xDE, 0xAD, 0xBE, 0xEF};
    char output[64];

    hid_device_format_uid(uid, 4, " ", output, sizeof(output));

    TEST_ASSERT_EQUAL_STRING("DE AD BE EF", output);
}

void test_format_7byte_uid_colon_delimiter(void) {
    uint8_t uid[] = {0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
    char output[64];

    hid_device_format_uid(uid, 7, ":", output, sizeof(output));

    TEST_ASSERT_EQUAL_STRING("04:A1:B2:C3:D4:E5:F6", output);
}

void test_format_empty_uid(void) {
    uint8_t uid[] = {};
    char output[64];

    hid_device_format_uid(uid, 0, " ", output, sizeof(output));

    TEST_ASSERT_EQUAL_STRING("", output);
}

int run_tests(void) {
    UNITY_BEGIN();

    RUN_TEST(test_format_4byte_uid_space_delimiter);
    RUN_TEST(test_format_7byte_uid_colon_delimiter);
    RUN_TEST(test_format_empty_uid);

    return UNITY_END();
}
```

**Run tests**:
```bash
cd /home/work/flipperzero-firmware
./fbt test_flipper_wedge
```

---

## Integration Testing Strategy

### What to Test

Integration tests validate that modules work together correctly:

1. **NFC → Format → HID** pipeline
2. **RFID → Format → HID** pipeline
3. **Settings → Storage → Persistence** cycle
4. **Mode switching** state machine
5. **Error recovery** (NFC restart, HID reconnect)

### Mock Requirements

To test without hardware, we need to mock:
- `NfcPoller` - Simulate tag detection
- `LfRfidWorker` - Simulate RFID reads
- `FuriHalUsb` - Simulate USB connection
- `FuriHalBt` - Simulate BT connection
- `Storage` - Simulate SD card reads/writes

### Example Integration Test

Create `tests/test_nfc_to_hid_pipeline.c`:

```c
// Mock NFC tag detection
static void mock_nfc_tag_detected(uint8_t* uid, size_t uid_len) {
    // Simulate NfcPollerEvent with tag UID
    // Trigger callback with mock data
}

// Mock HID output
static char last_typed_string[256];
static void mock_hid_type_string(const char* str) {
    strcpy(last_typed_string, str);
}

void test_nfc_uid_formatting_and_typing(void) {
    // Setup
    uint8_t test_uid[] = {0x04, 0xAA, 0xBB, 0xCC};
    HidDevice* app = test_app_create();

    // Configure with space delimiter
    strcpy(app->delimiter, " ");

    // Simulate NFC tag detection
    mock_nfc_tag_detected(test_uid, 4);

    // Wait for processing
    furi_delay_ms(100);

    // Verify HID output
    TEST_ASSERT_EQUAL_STRING("04 AA BB CC", last_typed_string);

    // Cleanup
    test_app_destroy(app);
}
```

---

## Hardware Testing Automation

### Semi-Automated Testing with Test Fixtures

While full automation is difficult, we can semi-automate hardware tests:

**Setup**:
1. Flipper Zero connected via USB
2. Test tags in known positions
3. Test host computer for HID verification

**Script** (`scripts/hardware-test.sh`):

```bash
#!/bin/bash
# Semi-automated hardware test
# Requires: Flipper connected, test tags ready

echo "========================================="
echo "Hardware Test Suite"
echo "========================================="
echo ""

# Deploy app to Flipper
echo "Deploying app to Flipper..."
./deploy.sh official

# Wait for app to launch
sleep 2

echo ""
echo "Manual Test Steps:"
echo "---"
echo "1. Place NTAG215 on Flipper (has UID: 04:AA:BB:CC:DD:EE:FF)"
echo "   Expected output: 04 AA BB CC DD EE FF"
echo "   ✅ PASS  ❌ FAIL"
echo ""
echo "2. Remove tag, place EM4100 on Flipper (UID: 1A2B3C4D5E)"
echo "   Expected output: 1A 2B 3C 4D 5E"
echo "   ✅ PASS  ❌ FAIL"
echo ""
echo "3. Test NDEF mode with NDEF tag (contains: Hello World)"
echo "   Expected output: Hello World"
echo "   ✅ PASS  ❌ FAIL"
echo ""

read -p "Press Enter when done testing..."

echo "Test session complete. Review results above."
```

### Automated HID Output Verification

Create `scripts/verify-hid-output.py`:

```python
#!/usr/bin/env python3
"""
Monitors keyboard input to verify HID output from Flipper.
Run this script, then scan a tag with the Flipper app.
Script will verify the typed output matches expected UID.
"""

import sys
from pynput import keyboard

captured_text = []
expected_uid = "04 AA BB CC DD EE FF"

def on_press(key):
    try:
        captured_text.append(key.char)
    except AttributeError:
        if key == keyboard.Key.enter:
            output = ''.join(captured_text)
            print(f"\nCaptured: {output}")
            print(f"Expected: {expected_uid}")

            if output == expected_uid:
                print("✅ PASS")
                sys.exit(0)
            else:
                print("❌ FAIL")
                sys.exit(1)

print("Listening for HID input...")
print(f"Expected UID: {expected_uid}")
print("Scan tag now...")

with keyboard.Listener(on_press=on_press) as listener:
    listener.join()
```

**Usage**:
```bash
python3 scripts/verify-hid-output.py &
# Now scan tag with Flipper
```

---

## Continuous Integration (CI/CD)

### GitHub Actions Workflow

Create `.github/workflows/build-test.yml`:

```yaml
name: Build Test

on:
  push:
    branches: [ main, dev ]
  pull_request:
    branches: [ main ]

jobs:
  build-official:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Clone Official Firmware
        run: |
          git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
          ln -s "$PWD" flipperzero-firmware/applications_user/flipper_wedge

      - name: Build FAP
        run: |
          cd flipperzero-firmware
          ./fbt fap_flipper_wedge

      - name: Check for warnings
        run: |
          cd flipperzero-firmware
          ! ./fbt fap_flipper_wedge 2>&1 | grep -i "warning:"

      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: flipper_wedge-official.fap
          path: flipperzero-firmware/build/*/. extapps/flipper_wedge.fap

  build-unleashed:
    runs-on: ubuntu-latest
    steps:
      # Similar to above, but clone unleashed-firmware

  build-momentum:
    runs-on: ubuntu-latest
    steps:
      # Similar to above, but clone Momentum-Firmware
```

### Pre-Commit Hooks

Create `.git/hooks/pre-commit`:

```bash
#!/bin/bash
# Pre-commit hook: Run build tests before allowing commit

echo "Running pre-commit build tests..."

# Quick build check on Official firmware
if ! ./build.sh official > /dev/null 2>&1; then
    echo "❌ Build failed on Official firmware"
    echo "Fix build errors before committing."
    exit 1
fi

echo "✅ Build tests passed"
exit 0
```

**Install**:
```bash
chmod +x .git/hooks/pre-commit
```

---

## Test Data Sets

### Standard Test Tags

Maintain a set of known test tags for consistent testing:

**NFC Tags**:
1. **NTAG215** - UID: `04:AA:BB:CC:DD:EE:FF` (7-byte, no NDEF)
2. **NTAG215 with NDEF** - UID: `04:11:22:33:44:55:66`, NDEF: "Test Tag 1"
3. **MIFARE Classic 1K** - UID: `04:77:88:99` (4-byte)
4. **ISO15693** - UID: `E0:12:34:56:78:9A:BC:DE` (8-byte, Type 5 NDEF)

**RFID Tags**:
1. **EM4100** - UID: `1A2B3C4D5E` (5-byte)
2. **HID Prox** - UID: `0F0E0D0C0B` (5-byte)

**Document tags in** `tests/test-tags.md`:

```markdown
# Test Tag Inventory

## NFC Tags

| Tag ID | Type | UID | NDEF | Purpose |
|--------|------|-----|------|---------|
| NFC-1 | NTAG215 | 04:AA:BB:CC:DD:EE:FF | None | Basic NFC UID test |
| NFC-2 | NTAG215 | 04:11:22:33:44:55:66 | "Test Tag 1" | NDEF text record test |
| NFC-3 | MIFARE Classic | 04:77:88:99 | None | 4-byte UID test |
| NFC-4 | ISO15693 | E0:12:34:56:78:9A:BC:DE | "Type 5" | Type 5 NDEF test |

## RFID Tags

| Tag ID | Type | UID | Purpose |
|--------|------|-----|---------|
| RFID-1 | EM4100 | 1A2B3C4D5E | Basic RFID test |
| RFID-2 | HID Prox | 0F0E0D0C0B | HID protocol test |

## Storage

Tags are stored in the lab, box labeled "Contactless HID Test Tags".
```

---

## Testing Checklist Automation

Create `scripts/test-checklist.sh`:

```bash
#!/bin/bash
# Interactive test checklist
# Guides tester through manual tests, records results

CHECKLIST_FILE="test-results-$(date +%Y%m%d-%H%M%S).txt"

echo "=========================================" | tee $CHECKLIST_FILE
echo "Flipper Wedge - Test Checklist" | tee -a $CHECKLIST_FILE
echo "=========================================" | tee -a $CHECKLIST_FILE
echo "" | tee -a $CHECKLIST_FILE

test_item() {
    local category=$1
    local description=$2

    echo "[$category] $description"
    read -p "  Result (pass/fail/skip): " result

    case $result in
        pass|p) echo "  ✅ PASS" | tee -a $CHECKLIST_FILE ;;
        fail|f) echo "  ❌ FAIL" | tee -a $CHECKLIST_FILE ;;
        skip|s) echo "  ⏭️  SKIP" | tee -a $CHECKLIST_FILE ;;
        *) echo "  ❓ UNKNOWN" | tee -a $CHECKLIST_FILE ;;
    esac

    echo "" | tee -a $CHECKLIST_FILE
}

# NFC Tests
echo "=== NFC Tests ===" | tee -a $CHECKLIST_FILE
test_item "NFC" "ISO14443A tag UID read correctly"
test_item "NFC" "4-byte UID formatted correctly"
test_item "NFC" "7-byte UID formatted correctly"
test_item "NFC" "Tag removal detected"

# RFID Tests
echo "=== RFID Tests ===" | tee -a $CHECKLIST_FILE
test_item "RFID" "EM4100 tag UID read correctly"
test_item "RFID" "Tag removal detected"

# ... more tests ...

echo "=========================================" | tee -a $CHECKLIST_FILE
echo "Test results saved to: $CHECKLIST_FILE" | tee -a $CHECKLIST_FILE
```

**Usage**:
```bash
./scripts/test-checklist.sh
```

---

## Automated Test Summary

### What We Can Automate

| Test Type | Automation Level | Tool/Script |
|-----------|------------------|-------------|
| **Build Validation** | ✅ Fully Automated | `scripts/test-builds.sh` |
| **Unit Tests** | ✅ Fully Automated | Firmware test framework |
| **Integration Tests** | ⚠️ Partially (with mocks) | Custom test harness |
| **HID Output** | ⚠️ Semi-Automated | `scripts/verify-hid-output.py` |
| **Hardware Tests** | ❌ Manual (with guided script) | `scripts/hardware-test.sh` |
| **Firmware Compatibility** | ⚠️ Build-level only | CI/CD pipeline |

### Recommended Testing Workflow

**Daily** (automated):
```bash
# Pre-commit hook runs automatically
git commit -m "..."
```

**Weekly** (semi-automated):
```bash
# Full build test on all firmwares
./scripts/test-builds.sh

# Run unit tests (once implemented)
cd /home/work/flipperzero-firmware
./fbt test_flipper_wedge
```

**Before Release** (manual):
```bash
# Run full hardware test suite
./scripts/test-checklist.sh

# Test on all firmwares with real hardware
# Follow checklist in CLAUDE.md
```

---

## Future Automation Goals

### Short Term (1-3 months)
- [ ] Implement unit tests for formatting module
- [ ] Implement unit tests for settings parser
- [ ] Set up GitHub Actions for build validation
- [ ] Create test tag inventory

### Medium Term (3-6 months)
- [ ] Implement integration tests with mocks
- [ ] Semi-automate HID output verification
- [ ] Create firmware compatibility test matrix automation
- [ ] Automated warning detection in builds

### Long Term (6+ months)
- [ ] Hardware-in-the-loop testing rig (Flipper + test tags + scripting)
- [ ] Fully automated regression test suite
- [ ] Performance benchmarking automation
- [ ] Fuzz testing for NDEF parser

---

*Update this document as testing automation improves. The goal is to maximize automation while maintaining test quality.*

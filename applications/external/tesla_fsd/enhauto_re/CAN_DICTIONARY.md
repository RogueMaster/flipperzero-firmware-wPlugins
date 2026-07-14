# Tesla CAN Signal Dictionary — Sources and Findings

After failing to extract the enhauto Commander firmware directly, we found something
better: the open-source community has already pulled the complete Tesla CAN signal
dictionary straight from Tesla's own MCU firmware.

We don't need enhauto's dongle. We have Tesla's source-of-truth.

---

## Source 1: `mikegapinski/tesla-can-explorer` ⭐⭐⭐ (best)

URL: https://github.com/mikegapinski/tesla-can-explorer
Local: `/Volumes/OWC Envoy Ultra/fsd_hack/tesla-can-explorer/`

Mike Gapinski (creator of Tesla Android Diagnostic Tool) extracted the complete CAN
signal dictionary from Tesla's MCU firmware:
```
Source: /squashfs-root/usr/tesla/UI/lib/libQtCarVAPI.so.1.0.0
```

### Coverage

| File | Variant | Frames | Signals |
|------|---------|--------|---------|
| `can_frames_decoded_all_values.json` | Default | 577 | 40,484 |
| `can_frames_decoded_all_values_mcu2.json` | Model 3/Y MCU2 | 577 | 40,484 |
| `can_frames_decoded_all_values_mcu3.json` | Model 3/Y MCU3 | 577 | 40,484 |
| `can_frames_decoded_all_values_modelsx_amd.json` | Model S/X AMD Ryzen | ~ | ~ |
| `can_frames_decoded_all_values_modelsx_intel.json` | Model S/X Intel | ~ | ~ |

Each frame entry has:
```json
{
  "address_hex": "0x3FD",
  "address_dec": 1021,
  "frame_name": "UI_autopilotControl",
  "bus_name": "ETH",
  "bus_id": 3,
  "declared_signal_count": 74,
  "signals": [
    {
      "signal_name": "UI_enableFullSelfDriving",
      "signal_index": 21,
      "enum_map_symbol": "",
      "possible_values": [...]
    },
    ...
  ]
}
```

### What it provides
- ✅ Frame name (e.g. `UI_autopilotControl`, `UI_trackModeSettings`)
- ✅ Frame address (hex + dec)
- ✅ Bus name (`ETH`, `PT`, `CH`, `BDY` etc)
- ✅ Complete signal name list per frame
- ✅ Enum value tables for symbolic fields

### What it does NOT provide
- ❌ Bit positions (`start_position`, `width`, `endianness`)
- ❌ Scale / offset for numeric values
- ❌ Checksum byte location

This is because libQtCarVAPI.so is the **UI/View layer dictionary** — it knows the
namespace but not the wire format. The wire format lives in a separate module of
the MCU firmware (the gateway/canbus driver).

### What we want from this
The 0x3FD `UI_autopilotControl` frame contains **74 signals**, including:

```
UI_enableFullSelfDriving       ← the actual FSD enable flag
UI_hasFullSelfDriving          ← entitlement check
UI_fullSelfDrivingSuspended    ← Tesla suspended FSD
UI_fsdStopsControlEnabled      ← traffic light & stop sign control
UI_fsdContinueOnGreenWithCIPV  ← roll through greens
UI_fsdBetaRequest              ← FSD Beta opt-in
UI_fsdMaxSpeedOffsetPercentage ← speed cap %
UI_fsdVisualizationEnabled     ← UI viz
UI_smartSetSpeed
UI_smartSetSpeedOffset
UI_autosteerActivation
UI_autopilotDrivingProfile
UI_apply2021_1958_ISA          ← EU ISA reg
UI_apply2021_646_ELKS          ← EU ELKS reg
UI_apply2021_1341_DDAW
UI_applyR152_AEBS
UI_applyEceR79                 ← R79 nag reg
UI_apmv3Branch                 ← AP MV3 branch selector
UI_enableApproachingEmergencyVehicleDetection
UI_autoTurnSignalMode
UI_factorySummonEnable
UI_hardCoreSummon
... (50+ more)
```

Our existing Flipper code modifies bits 19, 46, 47, 60. Those are KNOWN positions
from CanFeather. We don't yet know which `UI_*` signal name maps to which bit.
That mapping requires bit-position metadata we don't have from this dataset.

### Other key frames the dictionary lists

| CAN ID | Frame | Bus | Signals | Notes |
|--------|-------|-----|---------|-------|
| `0x313` | `UI_trackModeSettings` | ETH | 10 | `UI_stabilityModeRequest`, `UI_trackModeRequest`, `UI_trackStabilityAssist`, `UI_trackRotationTendency` |
| `0x293` | `UI_chassisControl` | ETH | 28 | `UI_tractionControlMode`, `UI_aebEnable`, `UI_redLightStopSignEnable`, `UI_steeringTuneRequest` |
| `0x118` | `DI_systemStatus` | ETH | 21 | `DI_trackModeState`, `DI_tractionControlMode`, `DI_gear` |
| `0x2B6` | `DI_chassisControlStatus` | ETH | 14 | `DI_stabilityModeState`, `DI_tractionControlModeUI` |
| `0x36B` | `DI_alertMatrix3` | ETH | 53 | `DI_a181_rearMotorMode`, `DI_a182_frontMotorMode` (read-only state) |
| `0x145` | `ESP_status` | ETH | 26 | `ESP_brakeTorqueTarget`, `ESP_stabilityControlSts2` |
| `0x399` | `DAS_status` | ETH | 26 | `DAS_autopilotState`, `DAS_blindSpotRearLeft/Right`, `DAS_summonAvailable` |
| `0x101` | `GTW_epasControl` | CH | — | `GTW_epasTuneRequest` (writable! changes steering effort) |
| `0x370` | `EPAS_sysStatus` | CH | — | `EPAS_handsOnLevel`, `EPAS_torsionBarTorque`, `EPAS_currentTuneMode` |
| `0x488` | `DAS_steeringControl` | CH | — | `DAS_steeringAngleRequest`, `DAS_steeringControlType` |

### Bus naming convention

| Tesla bus | What we call it |
|-----------|-----------------|
| `ETH` | Vehicle CAN / "Ethernet" backbone (most automation lives here) |
| `CH` | Chassis CAN — EPAS, EPB, ESP, brake systems |
| `PT` | Powertrain CAN — DI, BMS, charger |
| `BDY` | Body CAN — VCFRONT, VCLEFT, VCRIGHT, doors/windows/seats |
| `OBDII` | Standard OBD-II diagnostic |

The "ETH" bus on Tesla is NOT actual ethernet — it's just Tesla's name for the
high-bandwidth CAN-FD backbone. This is what comes out of the OBD-II port and
the X179 connector on Model 3/Y.

---

## Source 2: `talas9/tesla_can_signals` ⭐⭐ (best wire format)

URL: https://github.com/talas9/tesla_can_signals
Local: `/Volumes/OWC Envoy Ultra/fsd_hack/tesla_can_signals/`

Per-vehicle, per-bus JSON with **complete wire format metadata**.

### Coverage

| Vehicle | Files | Total messages |
|---------|-------|----------------|
| Model 3 | 1 (ETH only) | 148 |
| Model Y | 1 (ETH only) | 148 |
| Model S | 7 (BDY, BFT, CH, ETH, OBDII, PT, PT_BMSDBG) | ~225 |
| Model X | 8 (same + TH thermal) | ~241 |

### Per-message format

```json
{
  "APP_cameraTemperatures": {
    "message_id": 1177,
    "length_bytes": 8,
    "cycle_time": 1000,
    "originNode": "app",
    "senders": ["gtw"],
    "send_type": "Cyclic",
    "role": "Normal",
    "signals": {
      "APP_lRepeaterCameraTemperature": {
        "start_position": 32,
        "width": 8,
        "endianness": "LITTLE",
        "signedness": "UNSIGNED",
        "scale": 1,
        "offset": -60,
        "min": -60,
        "max": 195,
        "units": "C",
        "value_table_name": "SNA_u8",
        "value_description": {"SNA": 255},
        "receivers": ["odin"],
        "continuous": true
      },
      ...
    }
  }
}
```

### What this gives us

- ✅ **start_position** (bit offset) — what we needed!
- ✅ **width** (bits)
- ✅ endianness, signedness
- ✅ scale, offset, units, min/max for numeric signals
- ✅ value_description (enum mapping)
- ✅ cycle_time (broadcast period in ms)
- ✅ sender / receiver ECUs

### Limitation

The Model 3/Y `_ETH.compact.json` files only contain **148 messages** — less than
mikegapinski's 577. They appear to be a curated subset that doesn't include the
`UI_*` and `DAS_*` autopilot frames. We searched all 21 talas9 files and found
NO message with `autopilot` or `fsd` in the name.

### How to use the two together

Cross-reference like this:
```
1. Look up signal name in mikegapinski (mcu3.json) → get frame address + signal name
2. Look up the signal name in talas9 → get bit start_position, width
3. Apply to our Flipper code
```

This works for signals that exist in BOTH datasets (battery, drivetrain, ESP, EPAS).
For autopilot UI signals (the 74 in UI_autopilotControl), neither dataset has the
bit positions. Those still come from CanFeather's empirically-found bit numbers
(19, 46, 47, 60).

---

## Source 3: `tuncasoftbildik/tesla-can-mod` ⭐ (working code with new frames)

URL: https://github.com/tuncasoftbildik/tesla-can-mod
Local: `/Volumes/OWC Envoy Ultra/fsd_hack/tesla-can-mod/`

ESP32-C6 Tesla CAN mod with FSD activation, battery monitoring, **preconditioning**,
WiFi dashboard. Has **TESLA_CAN_BATTERY_REFERENCE.md** and **TESLA_CAN_STEERING_REFERENCE.md**
with documentation cross-referenced from multiple DBC sources.

### Working CAN frame templates that we can directly steal

**1. Battery preconditioning trigger** (CAN ID `0x082` / 130 dec — `UI_tripPlanning`):

```cpp
CanFrame pf;
pf.id = 0x082;
pf.dlc = 8;
memset(pf.data, 0, 8);
pf.data[0] = 0x05;   // bit 0 (tripPlanningActive) + bit 2 (requestActiveBatteryHeating)
driver.send(pf);
// Send periodically every 500ms
```

This triggers the BMS to start heating the battery — same as Tesla's "Precondition
for Supercharger" feature, but you can trigger it manually anywhere.

**2. Battery monitoring (read only):**

| CAN ID | Frame | Decoding |
|--------|-------|----------|
| `0x132` (306) | `BMS_hvBusStatus` | `voltage = ((data[1]<<8)\|data[0]) * 0.01` (V), `current = (int16)((data[3]<<8)\|data[2]) * 0.1` (A signed) |
| `0x292` (658) | `BMS_socStatus` | `soc = (((data[1]&0x03)<<8)\|data[0]) * 0.1` (%) |
| `0x312` (786) | `BMS_thermalStatus` | `temp_min = data[4]-40` (°C), `temp_max = data[5]-40` (°C) |
| `0x33A` (826) | `UI_ratedConsumption` | Wh/km energy consumption |

**3. HW4 FSD enable bits — different from CanFeather's HW4 handler!**

```cpp
// tuncasoftbildik HW4 mux 0:
setBit(frame, 46, true);   // Enable FSD
setBit(frame, 60, true);   // Additional FSD flag
setBit(frame, 59, false);  // Clear restriction  ← we don't do this

// tuncasoftbildik HW4 mux 2 (speed profile):
frame.data[7] = (frame.data[7] & 0x1F) | ((speedProfile & 0x07) << 5);
//                                        ^^^^^^^^^^^^^^^^^^^^^^^^^^^
//                                        bits 5-7 of byte 7 = bit 61-63
```

Our current code uses `(speed_profile & 0x07) << 4` which is **bits 4-6 = bit 60-62**.
Different by one bit. Worth verifying empirically which is right — there might be
a HW3 vs HW4 difference, or one of us is off by one.

**4. Steering effort change** (`0x101` / 257 dec — `GTW_epasControl`):

| Signal | Bits | Values |
|--------|------|--------|
| `GTW_epasTuneRequest` | 2\|3@0+ | 0=FAIL_SAFE, 1=DM_COMFORT, 2=DM_STANDARD, 3=DM_SPORT, 4=RWD_COMFORT, 5=RWD_STANDARD, 6=RWD_SPORT |
| `GTW_epasPowerMode` | 6\|4@0+ | 0=DRIVE_OFF, 1=DRIVE_ON, ... |
| `GTW_epasLDWEnabled` | 12\|1@0+ | 0=DISABLE, 1=ENABLE |

By spoofing this frame on Chassis CAN you can change Comfort/Standard/Sport steering
without going through the UI. **No public confirmation it works on HW4/Juniper yet.**

---

## Source 4: `commaai/opendbc` (we already have this)

Local: `/Volumes/OWC Envoy Ultra/fsd_hack/opendbc/`

The original Tesla DBC files used by openpilot. Has the bit positions we use today
in our Flipper code. Less complete than mikegapinski/talas9 but battle-tested.

---

## Combined Cross-reference Recipe

For any Tesla CAN feature you want to find:

```
1. Search mikegapinski (can_frames_decoded_all_values_mcu3.json):
   - Get the signal name and frame address
   - Confirm bus (ETH = on the X179/OBD-II we already use)

2. Search talas9 (Model3_ETH.compact.json):
   - If the signal is there → get start_position + width
   - If not → fall back to opendbc DBC

3. Search opendbc (tesla_*.dbc):
   - Older but bit-accurate
   - Most autopilot UI signals are here

4. Cross-check tuncasoftbildik handlers.h:
   - Working C++ code with empirical bit positions
   - Especially for HW4 (their bits 59/60 are slightly different from our code)
```

---

## What This Gives Us For Free

Things we can ADD to our Flipper app immediately because we have working CAN frame
templates from these sources:

| Feature | CAN frame | Source |
|---------|-----------|--------|
| **Battery preconditioning** | `0x082` byte[0]=0x05 | tuncasoftbildik handlers.h |
| **Live SoC %** | Read `0x292` | tuncasoftbildik |
| **Live pack voltage / current / kW** | Read `0x132` | tuncasoftbildik |
| **Live battery temp** | Read `0x312` | tuncasoftbildik |
| **Live energy consumption** | Read `0x33A` | tuncasoftbildik |
| **Live wheel torque** | Read `0x118` `DI_torqueDriver` etc | mikegapinski + opendbc |
| **Steering effort change** | Inject `0x101` `GTW_epasTuneRequest` | tuncasoftbildik STEERING_REFERENCE |
| **Track Mode toggle (untested)** | Inject `0x313` `UI_trackModeRequest` | mikegapinski (no bit position yet) |
| **Traction Control toggle (untested)** | Inject `0x293` `UI_tractionControlMode` | mikegapinski (no bit position yet) |

The **untested** ones need empirical bit-position discovery before they'll work.
The **working** ones can go into our v2.3 release immediately.

---

## Files

```
workspace/
├── tesla-can-explorer/        # mikegapinski — 40k signal dictionary (public)
├── tesla_can_signals/         # talas9 — bit positions per model/bus (public)
├── tesla-can-mod/             # tuncasoftbildik — working ESP32-C6 code (public)
├── opendbc/                   # commaai — DBC files we already use (public)
└── enhauto-re/
    ├── COMMANDS.md            # the master command catalog (public)
    ├── COMMANDER_WIFI_PANDA.md  # the Panda UDP reveal (public)
    └── CAN_DICTIONARY.md      # this file
```

(Internal research notes from disassembling the enhauto Android app are kept
out of this public folder. The published docs above contain only the
synthesised cross-source signal map, the protocol-level finding about the
Commander's WiFi Panda mode, and the action-catalog distillation. They do
not redistribute any binary artefact, protobuf descriptor, or other
copyrighted material from a third party.)

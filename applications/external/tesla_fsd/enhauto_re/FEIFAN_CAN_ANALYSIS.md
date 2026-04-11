# 非凡指揮官 CAN Signal Analysis

How the Feifan Commander (TSL 6th gen module, ~69K units sold in China)
likely implements its key features using CAN bus injection via OBD-II.

All signals below are confirmed present on Party CAN or Vehicle CAN from
opendbc `tesla_model3_party.dbc` and `tesla_model3_vehicle.dbc`.

---

## Feature: Continuous AP / AP Auto-Recovery

### Likely mechanism

The module monitors `DAS_status (0x39B)` for AP disengage events, then
re-injects either:

**(a) DAS_settings (0x293)** with autosteer bits re-enabled, or
**(b) STW_ACTN_RQ (0x238)** simulating a stalk pull to re-engage, or
**(c) SCCM_rightStalk (0x229)** simulating the physical AP engage gesture.

### Key writable signals

#### DAS_settings (0x293) — Party CAN

This frame carries the user's autopilot preferences. It has a counter
and checksum, so injection requires incrementing both.

| Signal | Bit | Width | Values | Role |
|--------|-----|-------|--------|------|
| `DAS_autosteerEnabled` | 38 | 1 | 0=off, 1=on | **AP master switch** |
| `DAS_autosteerEnabled2` | 24 | 1 | 0=off, 1=on | **AP secondary switch** |
| `DAS_aebEnabled` | 18 | 1 | 0/1 | AEB toggle |
| `DAS_fcwEnabled` | 34 | 1 | 0/1 | Forward collision warning |
| `DAS_fcwSensitivity` | 37 | 2 | 0-3 | FCW sensitivity |
| `DAS_slipStart` | 2 | 1 | 0/1 | Slip start mode |
| `DAS_driverSteeringWeight` | 1 | 2 | 0-3 | Steering weight |
| `DAS_driverAccelerationMode` | 44 | 1 | 0/1 | Accel mode |
| `DAS_obstacleAwareAcceleration` | 42 | 1 | 0/1 | Obstacle-aware accel |
| `DAS_adaptiveHeadlights` | 22 | 1 | 0/1 | Adaptive headlights |
| `DAS_offRoadAssist` | 3 | 2 | 0-3 | Off-road assist level |
| `DAS_settingCounter` | 52 | 4 | 0-15 | Frame counter |
| `DAS_settingChecksum` | 56 | 8 | 0-255 | Checksum |

Big-endian signals (bit numbering): `DAS_autosteerEnabled` at bit38|1@0+
means byte 4 bit 6 (38/8=4, 38%8=6) in Motorola order.

**Implementation plan:**
1. Sniff the current DAS_settings frame
2. When AP disengages (detected from DAS_status), copy the last frame
3. Set DAS_autosteerEnabled = 1
4. Increment counter, recalc checksum
5. Retransmit → AP re-engages

#### STW_ACTN_RQ (0x238) — Vehicle CAN

This frame represents steering wheel control inputs. On Highland (no
stalks), this is how AP gets engaged from the scroll wheel.

| Signal | Bit | Width | Values | Role |
|--------|-----|-------|--------|------|
| `SpdCtrlLvr_Stat` | 0 | 6 | encoded stalk positions | **AP engage gesture** |
| `DTR_Dist_Rq` | 8 | 8 | 0-200 | Follow distance request |
| `TurnIndLvr_Stat` | 16 | 2 | turn indicator | Turn signal |
| `HiBmLvr_Stat` | 18 | 2 | high beam lever | High beam |
| `StW_Sw00_Psd` | 32 | 1 | 0/1 | Scroll wheel button 0 |
| `StW_Sw01_Psd` | 33 | 1 | 0/1 | Scroll wheel button 1 |
| `StW_Sw02_Psd` | 34 | 1 | 0/1 | Scroll wheel button 2 |
| `HrnSw_Psd` | 30 | 2 | horn button | Horn |

`SpdCtrlLvr_Stat` value encoding (6 bits, 0-63):
- Exact mapping not in opendbc — needs sniff or reference from
  tuncasoftbildik / comma.ai codebase
- Likely: 0=idle, specific values for single-pull (TACC) and
  double-pull (AP engage)

**Implementation plan:**
1. Monitor DAS_status for AP disengage
2. Inject STW_ACTN_RQ with SpdCtrlLvr_Stat = AP_ENGAGE value
3. Wait ~500ms for DAS to acknowledge
4. Set SpdCtrlLvr_Stat back to idle

### Key readable signals (AP state monitoring)

#### DAS_status (0x39B) — Party CAN

| Signal | Bit | Width | Values | Role |
|--------|-----|-------|--------|------|
| `DAS_autopilotHandsOnState` | 42 | 4 | 0-15 | **Nag level (4-bit)** |
| `DAS_autoLaneChangeState` | 46 | 5 | 0-31 | Lane change state |
| `DAS_laneDepartureWarning` | 37 | 3 | 0-5 | Lane departure |
| `DAS_forwardCollisionWarning` | 22 | 2 | 0-3 | FCW active |
| `DAS_summonAvailable` | 51 | 1 | 0/1 | Summon ready |
| `DAS_autoparkReady` | 24 | 1 | 0/1 | Autopark ready |
| `DAS_visionOnlySpeedLimit` | 16 | 5 | ×5 kph | Vision speed limit |

#### DAS_status2 (0x389) — Party CAN

| Signal | Bit | Width | Values | Role |
|--------|-----|-------|--------|------|
| `DAS_ACC_report` | 26 | 5 | 0-24 | **ACC state (0=off..24)** |
| `DAS_driverInteractionLevel` | 38 | 2 | 0-2 | Driver interaction |
| `DAS_lssState` | 31 | 3 | 0-7 | Lane support state |
| `DAS_activationFailureStatus` | 14 | 2 | 0-2 | Why AP failed |

`DAS_ACC_report` is the definitive "is AP/TACC active" readback.

---

## Feature: Scroll Wheel Gear Shift (Highland)

Uses `STW_ACTN_RQ (0x238)` on Vehicle CAN:
- Read scroll wheel press events (`StW_Sw00_Psd` etc.)
- Translate to gear shift by injecting `SCCM_rightStalk (0x229)` with
  the appropriate gear value

Or directly inject `STW_ACTN_RQ` with `SpdCtrlLvr_Stat` set to a
gear-shift-compatible value.

---

## Feature: Strobe / Pulse High Beam

Uses `STW_ACTN_RQ (0x238)`:
- `HiBmLvr_Stat` (bit18|2): inject a rapid on/off/on/off pattern
  at ~200ms intervals

Or `SCCM_leftStalk (0x249)`:
- `SCCM_highBeamStalkStatus` (bit12|2): same approach

---

## Feature: Blind Spot Display on Phone

Reads from `DAS_status (0x39B)`:
- `DAS_sideCollisionWarning` (bit32|2) and
  `DAS_sideCollisionAvoid` (bit30|2)
- Forwards to phone via Bluetooth

---

## Implementation priority for Tesla Mod

### Tier 1 — Can implement NOW (Party CAN, same OBD-II tap)

| Feature | Read from | Write to | Complexity |
|---------|-----------|----------|------------|
| **AP state monitoring** | `0x39B` DAS_autopilotHandsOnState | — | 10 LOC |
| **ACC state** | `0x389` DAS_ACC_report | — | 10 LOC |
| **Lane change state** | `0x39B` DAS_autoLaneChangeState | — | 10 LOC |
| **FCW state** | `0x39B` DAS_forwardCollisionWarning | — | 5 LOC |
| **Vision speed limit** | `0x39B` DAS_visionOnlySpeedLimit | — | 5 LOC |
| **Blind spot warning** | `0x39B` DAS_sideCollision* | — | 10 LOC |
| **DAS_settings readback** | `0x293` all bits | — | 15 LOC |
| **AP activation failure** | `0x389` DAS_activationFailureStatus | — | 5 LOC |

### Tier 2 — Needs counter+checksum work (still Party CAN)

| Feature | Mechanism | Complexity |
|---------|-----------|------------|
| **AP auto-restart** | Inject `0x293` with autosteerEnabled=1 after disengage | 40 LOC + checksum |
| **DAS settings override** | Modify `0x293` bits (steering weight, accel mode, FCW sens) | 30 LOC |
| **Set speed** | Read/modify `DAS_control 0x2B9` DAS_setSpeed | 20 LOC + checksum |

### Tier 3 — Needs Vehicle CAN tap (different wire, same MCP2515)

| Feature | Mechanism | Complexity |
|---------|-----------|------------|
| **Stalk simulation** | Inject `0x238` STW_ACTN_RQ SpdCtrlLvr_Stat | 30 LOC |
| **Scroll wheel remap** | Read `0x238` StW_Sw*, translate to `0x229` gear | 40 LOC |
| **High beam strobe** | Inject `0x238` HiBmLvr_Stat rapid toggle | 20 LOC |
| **Horn honk** | Inject `0x238` HrnSw_Psd | 10 LOC |

---

## Sources

- opendbc/dbc/tesla_model3_party.dbc — Party CAN signal definitions
- opendbc/dbc/tesla_model3_vehicle.dbc — Vehicle CAN signal definitions
- Non-public: 非凡指揮官 product feature list (from Bilibili/Taobao)
- Inference: matching features to available CAN signals by elimination

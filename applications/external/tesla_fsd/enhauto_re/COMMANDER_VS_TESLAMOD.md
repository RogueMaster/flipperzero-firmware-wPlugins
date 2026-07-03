# S3XY Commander vs Tesla Mod — Complete Feature Comparison

## What is this

A line-by-line comparison of every feature the enhauto S3XY Commander
(€500 commercial dongle) offers, vs what Tesla Mod (this project) can
do today, and what it could do with further work.

Based on:
- 98 `ActionID` + 73 `SmartActionID` enums extracted from the Commander's
  official Android app (`libS3XYButtons_arm64-v8a.so`)
- mikegapinski/tesla-can-explorer MCU3 signal dump (40k signals from
  Tesla's own `libQtCarVAPI.so`) — determines CAN vs ETH bus assignment
- opendbc tesla_model3_party.dbc / tesla_model3_vehicle.dbc — CAN bit positions
- tuncasoftbildik steering reference — Chassis CAN bit positions

## Bus legend

| Tag | Meaning | Flipper + MCP2515 can reach? |
|-----|---------|------------------------------|
| `PARTY` | Party CAN (OBD-II Pin 6/14) | YES |
| `VEHICLE` | Vehicle CAN (X179 / body harness) | YES (different tap) |
| `CHASSIS` | Chassis CAN (EPAS / brake harness) | YES (different tap) |
| `ETH` | Ethernet bus (protobuf, internal MCU link) | NO — need Fleet API or MCU tap |
| `API` | Tesla Fleet HTTP API (cloud) | ESP32 only (WiFi needed) |

---

## ActionID (98 physical actions)

### Category: FSD / Autopilot

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 3 | `APEngage` / `AutopilotStart` | ✅ | ✅ `0x3FD` bit46 | PARTY | SHIPPED v1.0 |
| 4 | `APAssist` / `AutopilotExtended` | ✅ | ✅ `0x3FD` bit46+60 | PARTY | SHIPPED v1.0 |
| 5 | `APAutoSwitchLane` | ✅ | ✅ (part of FSD gate) | PARTY | SHIPPED |
| 29 | `AutopilotStart` | ✅ | ✅ | PARTY | SHIPPED |
| 31 | `AutopilotExtended` | ✅ | ✅ | PARTY | SHIPPED |
| 34 | `AutopilotSpeedAdjust` | ✅ | ✅ follow-distance map | PARTY | SHIPPED v1.0 |
| 61 | `AutopilotMax` | ✅ | ✅ speed profile 4 | PARTY | SHIPPED |
| 88 | `FollowDistance` | ✅ | ✅ `0x3F8` read | PARTY | SHIPPED |

### Category: Nag / Safety Override

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 13 | `SeatBeltsWarning` (nag kill) | ✅ | ✅ `0x370` counter+1 echo | PARTY | SHIPPED v2.1 |
| 67 | `EmergencyLaneKeepOff` | ✅ | ❌ | ETH | BLOCKED (ETH) |
| 42 | `MuteSpeedLimitWarning` | ✅ | ✅ `0x399` ISA chime suppress | PARTY | SHIPPED v2.0 |

### Category: Lighting

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 10 | `HazardLights` | ✅ | ✅ `0x3F5` hazardLightRequest | VEHICLE | SHIPPED v2.5 |
| 12 | `InteriorLights` | ✅ | ❌ | ETH | BLOCKED (ETH) |
| 33 | `LightsControl` | ✅ | ❌ | ETH | BLOCKED (ETH) |
| 36 | `PlaidLightsControl` | ✅ | ❌ | ETH | BLOCKED (ETH) |
| 39 | `Headlights` | ✅ | ❌ | ETH | BLOCKED (ETH) |
| 50 | `FrontFogLights` | ✅ | ⚠️ status only in DBC | VEHICLE | NEEDS SNIFF |
| 51 | `RearFogLights` | ✅ | ⚠️ status only | VEHICLE | NEEDS SNIFF |
| 52 | `AllFogLights` | ✅ | ⚠️ | VEHICLE | NEEDS SNIFF |
| 84 | `HighBeamStrobe` | ✅ | ⚠️ status only | VEHICLE | NEEDS SNIFF |
| 87 | `DynamicBrakeLights` | ✅ | ❌ | ETH | BLOCKED (ETH) |
| 83 | `LightStripBrightness` | ✅ | ❌ | ? | NEEDS S3XY LED strip addon |
| 30 | `RearDRL` (Smart) | ✅ | ⚠️ status only | VEHICLE | NEEDS SNIFF |

### Category: Wipers

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 32 | `WiperMode` | ✅ | ✅ `0x3F5` DAS_wiperSpeed | VEHICLE | SHIPPED v2.5 |
| 54 | `WipersWasher` | ✅ | ⚠️ (same frame, different value) | VEHICLE | EASY ADD |
| 47 | `WipersHeat` | ✅ | ❌ | ETH | BLOCKED (ETH) |

### Category: HVAC / Climate

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 27 | `HVAC_Blower` | ✅ | ❌ `UI_hvacReqBlowerSegment` | ETH `0x2F3` | BLOCKED — ESP32 Fleet API |
| 28 | `HVAC_DefogDefrost` | ✅ | ❌ `UI_hvacDefogState` | ETH `0x2F3` | BLOCKED |
| 38 | `BackWindowHeater` | ✅ | ⚠️ read state from `0x343` | ETH (write) | READ ONLY |
| 41 | `HVAC_SecondRowState` | ✅ | ❌ `UI_hvacReqSecondRowState` | ETH `0x2F3` | BLOCKED |
| 48 | `Recirc` | ✅ | ❌ `UI_hvacReqRecirc` | ETH `0x2F3` | BLOCKED |
| 49 | `AcDisable` | ✅ | ❌ | ETH | BLOCKED |
| 62 | `CabinTemp` | ✅ | ❌ | ETH | BLOCKED — Fleet API: `set_temps` |
| 66 | `BioweaponDefence` | ✅ | ❌ | ETH / API | ESP32 Fleet API |
| 67 | `KeepClimateOn` | ✅ | ❌ `UI_hvacReqKeepClimateOn` | ETH `0x2F3` | BLOCKED — Fleet API: `set_climate_keeper_mode` |
| 68 | `DogMode` | ✅ | ❌ | ETH / API | ESP32 Fleet API |
| 69 | `CampMode` | ✅ | ❌ | ETH / API | ESP32 Fleet API |
| 70 | `HVAC_On` | ✅ | ❌ | ETH / API | ESP32 Fleet API |
| 42 | `Precondition` | ✅ | ✅ `0x082` byte[0]=0x05 | PARTY | SHIPPED v2.3 |

### Category: Seat Heat / Ventilation

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 25 | `FrontSeatHeatLeft` | ✅ | ❌ `UI_frontLeftSeatHeatReq` | ETH `0x273` | BLOCKED — Fleet API: `remote_seat_heater_request` |
| 26 | `FrontSeatHeatRight` | ✅ | ❌ | ETH `0x273` | BLOCKED |
| 18-21 | `RearSeatHeat*` | ✅ | ❌ | ETH `0x273` | BLOCKED |
| 74 | `AllSeatHeat` | ✅ | ❌ | ETH | BLOCKED |
| 23 | `SteeringWheelHeat` | ✅ | ❌ `UI_steeringWheelHeatLevelReq` | ETH `0x274` | BLOCKED |
| 44 | `FrontSeatVentLeft` | ✅ | ❌ `UI_frontLeftSeatFanReq` | ETH `0x3B3` | BLOCKED |
| 45 | `FrontSeatVentRight` | ✅ | ❌ | ETH `0x3B3` | BLOCKED |

### Category: Drive Mode / Steering / Traction

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 4 | `DriveModeAccel` | ✅ | ❌ `UI_pedalMap` | ETH `0x334` | BLOCKED |
| 5 | `DriveModeRegen` | ✅ | ❌ | ETH | BLOCKED |
| 6 | `DriveModeSteering` | ✅ | ✅ `GTW_epasTuneRequest` `0x101` | CHASSIS | SHIPPED v2.5 (Chassis CAN tap required) |
| 46 | `StoppingMode` | ✅ | ❌ `UI_stoppingMode` | ETH `0x334` | BLOCKED |
| 55 | `TractionControl` | ✅ | ❌ `UI_tractionControlMode` | ETH `0x293` | BLOCKED |
| 56 | `SpeedControl` | ✅ | ❌ | ETH | BLOCKED |

### Category: Track Mode

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 14 | `TrackMode` | ✅ | ⚠️ read `DI_trackModeState` from `0x118` | ETH (write `0x313`) | READ ONLY |
| 59 | `TrackModeStability` | ✅ | ❌ `UI_stabilityModeRequest` | ETH `0x313` | BLOCKED |
| 60 | `TrackModeHandling` | ✅ | ❌ `UI_trackDrivePowerAvailability` | ETH `0x313` | BLOCKED |

### Category: Gear / Parking

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 57 | `GearShift` | ✅ | ✅ `0x229` SCCM_rightStalk | VEHICLE | SHIPPED v2.5 |
| 58 | `SimulatePARK` | ✅ | ✅ `0x229` parkButtonStatus | VEHICLE | SHIPPED v2.5 |

### Category: Doors / Trunk / Frunk / Charge Port

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 1-2 | `FrunkOpen` | ✅ | ❌ | API | ESP32 Fleet API |
| 3 | `TrunkOpen` | ✅ | ❌ | API | ESP32 Fleet API |
| 8 | `ChargePort` | ✅ | ❌ `UI_chargePortLatchRequest` | ETH `0x333` | BLOCKED — Fleet API |
| 35 | `ChildUnLock` | ✅ | ❌ | ETH | BLOCKED |
| 40 | `OpenDoor` | ✅ | ❌ | API | ESP32 Fleet API |
| 53 | `UnlockCar` | ✅ | ❌ | API | ESP32 Fleet API |

### Category: Mirrors

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 7 | `FoldMirrors` | ✅ | ⚠️ read state from `0x102`/`0x103` | VEHICLE (status) | READ ONLY — write path unclear |
| 17 | `MirrorsDip` | ✅ | ❌ | ETH | BLOCKED |
| 22 | `MirrorsDim` | ✅ | ❌ | ETH | BLOCKED |

### Category: Windows

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 71-73 | `VentWindows` / `VentLeft` / `VentRight` | ✅ | ❌ | ETH / API | ESP32 Fleet API |
| 90-93 | `FrontLeftWindow` through `RearRightWindow` | ✅ | ❌ | ETH | BLOCKED |

### Category: Suspension

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 77 | `Suspension` | ✅ | ❌ `UI_suspensionLevelRequest` | ETH `0x297` | BLOCKED |
| 96 | `RideHandling` | ✅ | ❌ `UI_adaptiveRideRequest` | ETH `0x297` | BLOCKED |

### Category: Seats

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 63-64 | `DriverMoveSeat` / `PassengerMoveSeat` | ✅ | ❌ | ETH | BLOCKED |
| 78-81 | Seat profile save/restore | ✅ | ❌ | ETH | BLOCKED |
| 85-86 | Passenger easy-entry profile | ✅ | ❌ | ETH | BLOCKED |

### Category: Media / Audio

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 9 | `MediaControl` | ✅ | ❌ | ETH | BLOCKED |
| 16 | `VoiceCommand` | ✅ | ❌ | ETH | BLOCKED |
| 97 | `Sounds` | ✅ | ❌ | ETH | BLOCKED |
| 94 | `Grok` | ✅ | ❌ | ETH | BLOCKED |
| 37 | `ThankYou` (horn+lights) | ✅ | ❌ | API+CAN combo | COMPLEX |

### Category: Other / Misc

| # | ActionID | Commander | Tesla Mod | Bus | Status |
|---|----------|-----------|-----------|-----|--------|
| 15 | `HonkHorn` | ✅ | ❌ | API | ESP32 Fleet API |
| 24 | `PartyMode` | ✅ | ❌ | ETH | BLOCKED |
| 43 | `ScreenTilt` | ✅ | ❌ | ETH | BLOCKED |
| 65 | `DisableMotor` | ✅ | ❌ | ? (Commander secret) | NEEDS SNIFF |
| 82 | `RearViewCamera` | ✅ | ❌ | ETH | BLOCKED |
| 89 | `HandWash` | ✅ | ❌ | compound | COMPLEX |
| 95 | `SentryMode` | ✅ | ❌ `GTW_sentryModeState` | ETH `0x348` | BLOCKED — Fleet API |
| 11 | `ParkBrake` | ✅ | ❌ | ETH | BLOCKED (safety-gated) |

### Category: BMS / Battery (Tesla Mod exclusive — Commander doesn't expose these)

| Feature | Commander | Tesla Mod | Bus | Status |
|---------|-----------|-----------|-----|--------|
| Live pack voltage/current | ❌ | ✅ `0x132` | PARTY | SHIPPED v2.3 |
| State of Charge (SoC) | ❌ | ✅ `0x292` | PARTY | SHIPPED v2.3 |
| Battery temp min/max | ❌ | ✅ `0x312` | PARTY | SHIPPED v2.3 |
| Vehicle speed (kph) | ❌ | ✅ `0x257` | PARTY | SHIPPED v2.5 |
| Steering tune mode readback | ❌ | ✅ `0x370` EPAS3S_currentTuneMode | PARTY | SHIPPED v2.5 |
| Steering torque (Nm) | ❌ | ✅ `0x370` EPAS3S_torsionBarTorque | PARTY | SHIPPED v2.5 |
| Brake applied state | ❌ | ✅ `0x145` ESP_driverBrakeApply | PARTY | SHIPPED v2.5 |
| OTA detection + auto-pause TX | ❌ | ✅ `0x318` GTW_updateInProgress | PARTY | SHIPPED v2.2 |
| CRC error monitoring | ❌ | ✅ MCP2515 register | — | SHIPPED v2.2 |

---

## SmartActionID (73 automation toggles)

Most of these are compound automations (e.g. "when I approach the car,
open the frunk + turn on lights + set climate") that the Commander
handles locally on the dongle. They're not single CAN frames — they're
scripted sequences of multiple actions.

**We can replicate the underlying actions** (via CAN or Fleet API) but
the automation logic (triggers, conditions, timers) would need to be
built as a separate layer. This is a v3.0+ roadmap item.

---

## Score

| | Commander (€500) | Tesla Mod (free) |
|---|---|---|
| Total actions defined | 98 + 73 = 171 | — |
| Actions SHIPPED today | ~90 (estimated) | **19 TX + RX handlers** |
| Actions on CAN (Flipper reachable) | ~30 | **15 implemented / ~18 possible** |
| Actions on ETH only | ~40 | 0 (physically unreachable via MCP2515) |
| Actions via Fleet API only | ~30 | 0 today (ESP32 port roadmap) |
| Actions needing Commander firmware | ~30 | 0 (need Panda UDP sniff) |
| **BMS / diagnostic features** | **0** | **7 read-only parsers** |
| **Safety gates (Listen-Only, OTA Guard)** | **0** | **3 safety features** |
| **Price** | **€500** | **$0 (+ ~$200 hardware)** |

## Key insight

**The Commander's real value is not CAN injection — it's the ETH bus
access and the scripted automation engine.** Most of the features that
make the Commander worth €500 (HVAC, seat heat, drive modes, track mode,
suspension, seat profiles, presenting doors) are on the Ethernet bus that
a simple CAN transceiver cannot reach.

For CAN-accessible features, Tesla Mod already covers **~83% of what's
possible** (15 of ~18 CAN-reachable actions). The remaining 3 (fog
lights, high beam strobe, RearDRL) need a write-path sniff because the
opendbc only has status outputs, not command inputs.

For ETH-only features, the path forward is the **ESP32 Fleet API client**
— which can replicate ~30 of the Commander's actions without any CAN
injection at all, just WiFi + Tesla OAuth.

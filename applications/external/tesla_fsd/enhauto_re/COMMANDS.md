# Complete Commander command map

Goal: the exhaustive list of every command a Tesla S3XY Commander can execute,
with the underlying delivery channel and — where known — the concrete CAN
frame or Tesla API endpoint.

Built from a cross-source synthesis:
- The official enhauto Android app's own action namespace, recovered from
  the protobuf descriptors that the app ships with — these define the
  complete catalogue of named actions (`ActionID`) and automations
  (`SmartActionID`) the dongle understands. The numbers below are exact,
  not estimated.
- Demangled C++ method names from the phone app — this is what reveals
  which actions are actually handled phone-side over the **Tesla Fleet
  HTTP API** vs forwarded BLE → dongle → CAN.
- Public Tesla CAN research: mikegapinski's 40k-signal dictionary, the
  commaai/opendbc Tesla DBC, talas9's per-model wire-format reference,
  tuncasoftbildik's working frame templates, and the original CanFeather
  signal research. Cited inline below where each frame template comes
  from.

This document does not redistribute any binary or descriptor file from a
third-party commercial product. It distils the *what* (an exhaustive
inventory of capabilities) and the *how* (which channel each capability
uses, and where to find the public CAN frame details for the
CAN-injected ones).

## Numbers

- **98 ActionIDs** — single button/knob physical actions (`ActionID` enum, `actions.proto` line 6)
- **73 SmartActionIDs** — automation toggles / modes (`SmartActionID` enum, line 127)
- ≈30 of the ActionIDs are implemented by the phone app over the **Tesla Fleet HTTP API**
- The remainder are BLE→dongle→CAN (CAN frames live in dongle firmware)

## Delivery channels

```
Phone app ──HTTPS──> Tesla Fleet API ──> Tesla cloud ──> Car         (≈30 commands)
Phone app ──BLE──> Commander dongle ──CAN──> Car                      (≈70 commands)
                          │
                          └──WiFi AP──UDP Panda protocol──> laptop/pi (read-only CAN dump)
```

## Channel 1 — Tesla Fleet HTTP API (phone-app path)

Extracted from `CTeslaAPIServer::Send*Request` symbols in the app binary.
These are direct Tesla Fleet API calls — **they don't need a Commander dongle
at all**. We can re-implement every one of these in our own Fleet API client.

| Commander action | Tesla Fleet API endpoint |
|------------------|--------------------------|
| `FrunkOpen` | `POST /api/1/vehicles/{id}/command/actuate_trunk` (`which_trunk=front`) |
| `TrunkOpen` | `POST /api/1/vehicles/{id}/command/actuate_trunk` (`which_trunk=rear`) |
| `DoorLock` | `POST /api/1/vehicles/{id}/command/door_lock` |
| `DoorUnlock` | `POST /api/1/vehicles/{id}/command/door_unlock` |
| `LeftDoorOpen` | `POST /api/1/vehicles/{id}/command/door_unlock` + per-side actuate |
| `HonkHorn` | `POST /api/1/vehicles/{id}/command/honk_horn` |
| `FlashLights` | `POST /api/1/vehicles/{id}/command/flash_lights` |
| `CarWakeUp` | `POST /api/1/vehicles/{id}/wake_up` |
| `BioweaponMode` | `POST /api/1/vehicles/{id}/command/set_bioweapon_mode` |
| `ClimateDogMode` | `POST /api/1/vehicles/{id}/command/set_climate_keeper_mode` (`climate_keeper_mode=2`) |
| `ClimateCampMode` | `set_climate_keeper_mode` (`=3`) |
| `ClimateKeepOn` | `set_climate_keeper_mode` (`=1`) |
| `ClimateOnOff` | `auto_conditioning_start` / `auto_conditioning_stop` |
| `ClimateVent` | `set_vehicle_control_mode` / window vent |
| `SetTemperature` | `set_temps` (single) |
| `SetTemps` | `set_temps` (driver + passenger) |
| `SetSeatTemperature` | `remote_seat_heater_request` |
| `ChargeOpenClose` | `charge_port_door_open` / `charge_port_door_close` |
| `GarageDoorOpenClose` | `trigger_homelink` |
| `RemoteBoombox` | `remote_boombox` |
| `SpeedLimit` | `speed_limit_activate` / `speed_limit_set_limit` |
| `GetVehicleData` | `GET /api/1/vehicles/{id}/vehicle_data` |
| `GetChargeState` | `GET /api/1/vehicles/{id}/data_request/charge_state` |
| `GetVehicles` | `GET /api/1/vehicles` |
| `GetAuthToken` | `POST /oauth2/v3/token` |
| `GetPartnerAuthToken` | `POST /oauth2/v3/token` (client_credentials) |
| `GetRefreshToken` | `POST /oauth2/v3/token` (refresh_grant) |
| `RegisterPartnerAccount` | `POST /api/1/partner_accounts` |
| `ValidatePublicKey` | `GET /api/1/partner_accounts/public_key` |
| `CommonGet` / `CommonPost` | generic wrappers |

**These 30 actions are 100% re-implementable today** using the Tesla Fleet
Vehicle Command Protocol. No dongle needed. They're the "standard" remote
commands Tesla already exposes.

## Channel 2 — BLE → dongle → CAN (dongle path)

Extracted from `SmartActionID` + `ActionID` enums. These are the actions that
require the Commander dongle to sit on CAN and inject frames. The byte-level
templates live in the dongle firmware.

### 2a. Actions where the CAN template is public / already observed on the bus

These are known from community sources — we can implement them on our own
MCP2515/TWAI box without any Commander.

| ActionID | CAN ID | Notes / source |
|----------|--------|----------------|
| `AutopilotStart` / `AutopilotExtended` / `AutopilotMax` | `0x3FD` (HW3/HW4), `0x3EE` (Legacy HW1/2) | bit46 FSD enable, bit60 FSDV14, CanFeather / our flipper-tesla-fsd |
| `AutopilotSpeedAdjust` | `0x3FD` | follow-distance bits map to speed profile |
| `FollowDistance` | `0x3FD` | read `0x118` DI_throttleStatus, write `0x3FD` |
| `SeatBeltsWarning` (nag) | `0x370` counter+1 echo | EPAS handsOnLevel spoof (MR !44, ported) |
| `HazardLights` | `0x273` `VCFRONT_lighting` | opendbc |
| `InteriorLights` | `0x273` bit | opendbc |
| `Headlights` | `0x273` / `0x223` | opendbc |
| `HonkHorn` (if via CAN not API) | `0x10C` / body UDS | mikegapinski |
| `GearShift` / `SimulatePARK` | `0x229` `SCCM_rightStalk` / `0x39D` | tuncasoftbildik template |
| `ChargePort` | `0x102` `VCLEFT_chargingHandleStatus` | opendbc |
| `Precondition` (RemoteBatteryPreheat) | `0x082` `UI_tripPlanning` byte[0]=0x05 | tuncasoftbildik (shipped in our v2.3.0) |
| `FoldMirrors` / `MirrorsDip` / `MirrorsDim` | `0x273` bits | opendbc |
| `WiperMode` / `WipersWasher` / `WipersHeat` | `0x3E2` `VCFRONT_wipers` | opendbc |
| `FrontFog/RearFog/AllFogLights` | `0x273` bits | opendbc |
| `BackWindowHeater` | `0x3B3` `HVAC_status` | opendbc |
| `HVAC_Blower` / `HVAC_On` / `HVAC_DefogDefrost` / `HVAC_SecondRowState` / `Recirc` | `0x2E5` `HVAC_Command` family | mikegapinski |
| `CabinTemp` / `CabinTempLeft` / `CabinTempRight` | `0x2E5` | mikegapinski |
| `FrontSeatHeat*` / `RearSeatHeat*` / `AllSeatHeat` / `SteeringWheelHeat` | `0x2E1` `VCRIGHT_hvacStatus` bits | opendbc |
| `FrontSeatVentLeft/Right` | `0x2E1` | opendbc |
| `DriveModeAccel` / `DriveModeRegen` / `DriveModeSteering` | `0x118` `DI_vehicleStatus` / `0x293` drivemode | opendbc |
| `StoppingMode` | `0x293` | opendbc |
| `TractionControl` / `ESP off` | `0x2A1` `ESP_status` | opendbc |
| `TrackMode` | `0x293` `UI_driverAssistRoadSign` + `0x2B9` | opendbc + talas9 |
| `TrackModeStability` / `TrackModeHandling` (0..100 sliders) | `0x2B9` payload | see `DashTrackModeHandling_*` enum values in .so |
| `VentWindows` / `VentLeft/Right` | `0x3E3` window control | opendbc (partial) |
| `FrontLeftWindow/FrontRightWindow/RearLeftWindow/RearRightWindow` | `0x3E3` per-window | opendbc (partial) |
| `Suspension` / `RideHandling` | `0x204` `VCFRONT_airSuspension` | mikegapinski |
| `HighBeamStrobe` | `0x273` bit | opendbc |
| `SentryMode` (if via CAN fallback) | `0x3F5` VCSEC | mikegapinski |
| `MediaControl` / `Sounds` / `RemoteBoombox` | `0x2F1` `UI_audioStatus` | opendbc |
| `PlaidLightsControl` / `DynamicBrakeLights` / `RearDRL` / `LightStripBrightness` | `0x273` / `0x264` | opendbc |
| `StopCanTxOnTeslaUpdate` (OTA pause) | monitor `0x318` `GTW_updateInProgress`, halt TX | shipped in our v2.2 |

### 2b. Actions where the exact CAN template is NOT yet public

These are the juicy "vendor secret" ones. The ActionID name + parameter type
is known from the protobufs, but the byte-level template is only in dongle
firmware. These are the ones worth sniffing when we get access to a real
Commander.

| ActionID | What it does | Why it's secret |
|----------|--------------|------------------|
| `DisableMotor` | Disable front OR rear motor (AWD → RWD mode) | Not a Tesla UI feature — Commander-only |
| `BioweaponDefence` (CAN fallback) | Engage biodef from button | Tesla API works, but Commander also has CAN fallback |
| `DogMode` / `CampMode` / `KeepClimateOn` (CAN fallback) | Same |
| `Grok` | LLM voice assist trigger | New in late 2025 |
| `ThankYou` | Horn+lights combo | Cute wrapper, templates unclear |
| `ParkBrake` | Engage EPB outside gear lever | Only 3 symbol hits, safety-gated |
| `ChildUnLock` | Child lock override | |
| `ScreenTilt` | Tilt center display (refresh MS/X) | |
| `DriverMoveSeat` / `PassengerMoveSeat` | Move seat via CAN, not Tesla API | |
| `Set/RestoreLeftSeatProfile` `Set/RestoreRightSeatProfile` `Set/RestorePassengerEasyEntrySeatProfile` | Save/restore seat memory | |
| `RearViewCamera` | Force rear cam on display | |
| `HandWash` | Wash mode (raise suspension, open frunk, etc.) | Compound action |
| `PresentingDoors*` (Smart) | Auto-present door on approach | Needs BLE presence + CAN door actuator |
| `PresentingTrunk*` (Smart) | Same for trunk | |
| `ActionsUponApproach` / `ActionsOnReverse` | Compound automation triggers | |
| `AutoWindowDrop` | Window cracks on door open | |
| `CameraOnFoldMirrors` | Turn on side cam when mirrors fold | |
| `SplitLedStrip` / `RainbowMode*` / `AllAmbientLightEffects` / `AccelerationModeLED` | LED strip controls (needs S3XY LED strip addon) | |
| `ScrollWheelGearShift` | Remap scroll wheel | |
| `PassengerEasyEntry` | Slide seat back on park | |
| `VolumeOnExit` | Drop volume to 0 on door open | |
| `HeadlightsWhenRain` | Auto headlights on wiper activity | |
| `Diagnostic` | Service mode / diagnostic entry | |
| `EmergencyLaneKeepOff` | Disable ELK | |
| `SuspensionByGear` | Auto-raise/lower per gear | |
| `GeneralAmbientLight` | Global ambient color/brightness | |
| `KickdownSport` / `KickdownResetPedal` | Gas pedal kickdown for sport mode shift | |
| `ForceManualHighBeam` / `AutoHighBeamOff` | High beam overrides | |
| `AutopilotRestart` / `AutopilotRestartHW2_5` | Auto-re-engage AP after disengagement | |
| `MuteSpeedLimitWarning` | Kill speed limit chime | |
| `AutoWipersOff` / `KeepWipersOff` | Disable auto wipers | |
| `RearVentAlwaysOn` / `RearVentAlwaysOff` | Force rear HVAC vent | |
| `ParkOnUnbuckled` | Force P when seatbelt unlatched | |
| `ChargePortSecurity` | Lock charge port | |
| `DynamicRegen` | Modulate regen by conditions | |
| `DoorHandleOnFullRelease` / `DoorHandleLongPress` | Handle gesture remap | |
| `Lights` | Generic lights macro | |

## Channel 3 — Commander WiFi Panda UDP (read-only)

Covered in `COMMANDER_WIFI_PANDA.md`. Not a command channel — it's a sniff
channel. Lets a laptop/Pi read every frame on Vehicle CAN + Chassis CAN over
UDP. Useful for **recording** the frames in 2b when we get hands on a
Commander.

## What this means for our app

**Flipper-tesla-fsd can implement a huge subset today** using nothing but
MCP2515 + public Tesla CAN research:

- Every action in table 2a (≈35 items)
- The full FSD unlock / nag kill / OTA pause / precondition we already ship
- An on-device Tesla Fleet API client (section 1) — but that needs WiFi,
  which the Flipper core doesn't have, so this path is off-Flipper (our
  companion web console)

Actions in table 2b stay out of scope until we either
(a) get a real Commander and sniff them on the Panda UDP channel, or
(b) someone leaks the dongle firmware.

## Immediate next actions

1. **Port table 2a to our Flipper app feature list.** Today we ship FSD
   unlock, nag killer, speed chime suppress, emergency vehicle detect, OTA
   pause, precondition, BMS dashboard. That's maybe 8 of the 35. The other
   ~27 are low-hanging fruit if we decide to broaden scope.
2. **Contact dzid26** (Task #44). Their "Dorky Commander" project is
   explicitly building an open-source alternative — if anyone has already
   done the 2b CAN sniffing work, it's them or the people they talk to.
3. **Do not** spend more time trying to extract the dongle firmware through
   the cloud API. The surface is sealed and the value has dropped.

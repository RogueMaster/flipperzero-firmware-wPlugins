#pragma once
/*
 * fsd_capability.h — tap capability verdicts (#125), pure / host-tested.
 *
 * "Will the nag killer actually work on the bus this device is plugged into?"
 * The #1 support issue is a wrong-bus / wrong-config tap: the X179 pin->bus map
 * varies by harness, so 0x370 (the nag echo source) and the DAS state that gates
 * it can land on different taps. This module turns a few seconds of passive RX
 * (which CAN ids were seen on a tap) into a per-FEATURE verdict — not a bus name.
 *
 * The ESP32 (capability.cpp) counts frames per id per bus over a short window
 * and feeds the seen-id set here; this header decides the verdicts. Header-only
 * (static inline), mirroring fsd_events.h / fsd_capture.h, so the verdict logic
 * lives in ONE place the host tests exercise directly — most importantly the
 * 0x399 dual-meaning trap.
 *
 * The 0x399 trap: 0x399 is DAS_status on HW3/Legacy, but on HW4 the SAME id is
 * the ISA speed chime and carries NO DAS state (which lives on 0x39B). So a HW4
 * car broadcasting 0x399 must NOT be read as "DAS state present". hw_version
 * disambiguates; if HW is still unknown it is inferred conservatively and the
 * caller flags the verdict "HW unconfirmed".
 *
 * Pure / deterministic: no I/O, no platform calls, no global state.
 */

#include "fsd_types.h" // TeslaHWVersion
#include <stdbool.h>

// Per-feature verdict. The dashboard renders OK / warn / fail / neutral.
typedef enum {
    CAP_OK = 0, // works on this tap
    CAP_DUAL_CAN, // partially present — a second tap (dual-CAN) is recommended
    CAP_MISSING, // not possible on this tap (required frame absent)
    CAP_NO_VALID, // the frame is here, but none in the window carried a valid value
} FSDCapVerdict;

// Best-guess bus-family hint (NEVER authoritative — pin<->bus varies by harness,
// confirm with the car's wiring diagram, or Service Mode -> CAN Port where the
// car has that page). Derived from which ids were seen.
typedef enum {
    CAP_HINT_NONE = 0, // not enough to guess
    CAP_HINT_PARTY, // 0x370 + DAS present -> looks Party-like
    CAP_HINT_CHASSIS, // steering present, no 0x370 -> looks Chassis/Vehicle-like
} FSDCapBusHint;

// Second bus hint, from 0x3C2 VCLEFT_switchStatus (just as unauthoritative).
// 0x3C2 is reported on the Vehicle CAN pair and not on the gateway-forwarded
// subset. That matches the public DBCs but has not been confirmed here, so it
// only ever reads as a hint.
typedef enum {
    CAP_VEH_HINT_NONE = 0, // no 0x3C2 and no body frame: nothing to go on
    CAP_VEH_HINT_DIRECT, // 0x3C2 seen -> looks like the Vehicle CAN pair
    CAP_VEH_HINT_FORWARDED, // body frames but no 0x3C2 -> looks like a forwarded subset
} FSDCapVehicleHint;

// The set of capability-relevant ids seen on one tap during the listen window.
// Names match the config.h CAN_ID_* roles, not bus names.
typedef struct {
    bool epas; // 0x370 EPAS_STATUS  — nag killer echo source
    bool das_hw4; // 0x39B DAS_STATUS_HW4 — DAS state on HW4 / Highland HW3
    bool das_hw3; // 0x399 DAS_STATUS_HW3 — DAS state on HW3/Legacy, ISA chime on HW4
    bool ap_control; // 0x3FD AP_CONTROL — FSD activation frame (HW3/HW4)
    bool ap_legacy; // 0x3EE AP_LEGACY  — FSD activation frame (Legacy/HW1/HW2)
    bool steer; // 0x129 STEER_ANGLE — Soft Engage gate
    bool steer_valid; // 0x129 with a VALID angle (fsd_steer_decode)

    // Vehicle/body-bus presence (#128). RX-only reachability, NOT actuation:
    // these frames carry rolling counters / CRC / VCSEC auth, so seeing them
    // proves only that body/comfort control is physically reachable on this tap.
    bool body_ui; // 0x273 UI_vehicleControl  — mirror fold/lock/wiper/horn/seat heat
    bool body_door; // 0x102 VCLEFT_doorStatus   — mirror state/tilt read-back
    bool body_window; // 0x119 VCSEC_windowRequests — windows
    bool body_lights; // 0x3E9 DAS_bodyControls    — lights/hazards/turn/wipers

    // Steering-wheel switches. Only read for vehicle_hint, never actuated here.
    bool vcleft_switch; // 0x3C2 VCLEFT_switchStatus
} FSDCapSeen;

typedef struct {
    FSDCapVerdict nag_killer; // 0x370 echo + DAS state to gate on
    FSDCapVerdict ap_first; // Abort Guard: needs DAS state
    FSDCapVerdict fsd_activation; // a frame here to modify
    FSDCapVerdict soft_engage; // 0x129 with a valid angle; MISSING => AP-First-only
    FSDCapVerdict body_control; // Vehicle/body-bus reachable (#128); RX presence only

    // Derived predicates (exposed for the UI / one-line verdicts).
    bool has_epas;
    bool has_das;
    bool has_ap_ctrl;
    bool has_steer;
    bool has_steer_valid; // 0x129 seen carrying a valid angle
    bool has_body; // any of the four body-bus frames seen -> Vehicle bus reachable
    bool body_ui; // 0x273 seen (mirror/lock/wiper/horn/seat-heat writes reachable)
    bool body_door; // 0x102 seen (mirror state read-back reachable)
    bool body_window; // 0x119 seen (window writes reachable)
    bool body_lights; // 0x3E9 seen (lights/hazards/turn/wiper writes reachable)
    bool has_vcleft_switch; // 0x3C2 seen

    FSDCapBusHint bus_hint;
    FSDCapVehicleHint vehicle_hint; // from 0x3C2, hint only
    bool hw_unconfirmed; // hw_version was Unknown -> inferred below
    TeslaHWVersion hw_effective; // HW used for the 0x399 interpretation
} FSDCapReport;

/** Evaluate one tap's seen-id set into per-feature verdicts.
 *
 * @param seen  ids observed on this tap during the listen window.
 * @param hw    detected hardware (from 0x398 / fsd_detect_hw_version). When
 *              TeslaHW_Unknown it is inferred: 0x39B -> HW4, else 0x3EE ->
 *              Legacy, else HW3/Legacy (the 0x399-readable assumption), and
 *              hw_unconfirmed is set so the caller can label it.
 *
 * The 0x399 trap is handled in `has_das`: 0x399 only counts as DAS state when
 * the (effective) HW is NOT HW4, because on HW4 0x399 is the ISA chime.
 */
static inline FSDCapReport fsd_capability_eval(FSDCapSeen seen, TeslaHWVersion hw) {
    FSDCapReport r = {
        CAP_MISSING, CAP_MISSING, CAP_MISSING, CAP_MISSING, CAP_MISSING,   false,
        false,       false,       false,       false,       false,         false,
        false,       false,       false,       false,       CAP_HINT_NONE, CAP_VEH_HINT_NONE,
        false,       hw,
    };

    // Resolve HW for the 0x399 interpretation.
    TeslaHWVersion eff = hw;
    if(eff == TeslaHW_Unknown) {
        r.hw_unconfirmed = true;
        if(seen.das_hw4)
            eff = TeslaHW_HW4; // 0x39B is HW4-only
        else if(seen.ap_legacy)
            eff = TeslaHW_Legacy; // 0x3EE is Legacy
        else
            eff = TeslaHW_HW3; // assume 0x399 is DAS state
    }
    r.hw_effective = eff;

    r.has_epas = seen.epas;
    r.has_das = seen.das_hw4 || (seen.das_hw3 && eff != TeslaHW_HW4);
    r.has_ap_ctrl = seen.ap_control || seen.ap_legacy;
    r.has_steer = seen.steer;
    r.has_steer_valid = seen.steer && seen.steer_valid;

    // Vehicle/body-bus reachability (#128): reachable if ANY body frame is seen.
    // Presence only — does not imply these frames can be actuated (counters/CRC/auth).
    r.body_ui = seen.body_ui;
    r.body_door = seen.body_door;
    r.body_window = seen.body_window;
    r.body_lights = seen.body_lights;
    r.has_body = seen.body_ui || seen.body_door || seen.body_window || seen.body_lights;
    r.has_vcleft_switch = seen.vcleft_switch;

    // Nag killer: needs both the 0x370 echo source AND DAS state to gate on.
    if(r.has_epas && r.has_das)
        r.nag_killer = CAP_OK;
    else if(r.has_epas && !r.has_das)
        r.nag_killer = CAP_DUAL_CAN; // 0x370 here, no DAS
    else
        r.nag_killer = CAP_MISSING; // no 0x370 to echo

    r.ap_first = r.has_das ? CAP_OK : CAP_MISSING;
    r.fsd_activation = r.has_ap_ctrl ? CAP_OK : CAP_MISSING;
    // Soft Engage reads the angle, so 0x129 counts once it carries a valid one.
    r.soft_engage = r.has_steer_valid ? CAP_OK :
                    r.has_steer       ? CAP_NO_VALID :
                                        CAP_MISSING; // AP-First-only
    r.body_control = r.has_body ? CAP_OK : CAP_MISSING; // else needs X179 A-pillar tap

    // Best-guess bus family (hint only).
    if(r.has_epas && r.has_das)
        r.bus_hint = CAP_HINT_PARTY;
    else if(r.has_steer && !r.has_epas)
        r.bus_hint = CAP_HINT_CHASSIS;
    else
        r.bus_hint = CAP_HINT_NONE;

    // Vehicle CAN hint (hint only): body frames that arrive without 0x3C2 look
    // like the subset the gateway forwards rather than the Vehicle CAN pair.
    if(r.has_vcleft_switch)
        r.vehicle_hint = CAP_VEH_HINT_DIRECT;
    else if(r.has_body)
        r.vehicle_hint = CAP_VEH_HINT_FORWARDED;
    else
        r.vehicle_hint = CAP_VEH_HINT_NONE;

    return r;
}

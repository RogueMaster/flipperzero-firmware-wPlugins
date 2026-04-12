# Security & responsible use

## Disclaimer

> [!WARNING]
> **Modifying CAN bus messages on a vehicle can cause dangerous behaviour
> or permanently damage your car.** The CAN bus carries braking, steering,
> airbag, and powertrain control signals. A malformed or out-of-spec frame
> can have serious physical consequences.

> [!WARNING]
> **FSD is a premium Tesla feature and must be properly purchased or
> subscribed to.** This project intercepts and modifies UI configuration
> frames at the CAN bus level. It does not bypass any cryptographic
> entitlement check on Tesla's servers, and using it without an active
> subscription is a violation of Tesla's Terms of Service. Doing so is
> your decision and your risk.

This project is published for testing, research, and educational purposes.
It is intended for use on **private property** and **off public roads**
unless and until you have your own legal opinion that operating it on a
public road is permitted in your jurisdiction.

The authors and contributors accept **no responsibility** for:
- Damage to your vehicle, including warranty voiding
- Personal injury or property damage
- Tesla account suspension or service revocation
- Violation of road traffic regulations in your country
- Civil or criminal liability arising from any of the above

If you don't fully understand what each setting does, **do not enable it
on a vehicle that drives on a public road**.

## Reporting a security issue

If you find:
- A way for this project to corrupt or destabilise a CAN bus beyond what
  the documented behaviour does
- An out-of-bounds read/write on the Flipper that could brick the device
- A subtle frame interaction that could cascade into a vehicle safety
  fault

Please **do not** open a public GitHub issue. Email
hypery11@gmail.com with the subject `[security] flipper-tesla-fsd` and
describe the issue, the reproduction steps, and the affected version. I'll
respond within a few days and credit you in the patch release notes if
you'd like.

For non-security bugs, the regular [issue tracker](https://github.com/hypery11/flipper-tesla-fsd/issues)
is fine.

## What this project does NOT touch

For peace of mind, here is the explicit list of CAN ID classes this
project's TX path can write to:

- `0x3FD` `UI_autopilotControl` — modifies bits 19, 46, 47, 59, 60 only;
  retransmits otherwise unchanged
- `0x3EE` `UI_autopilotControl` (Legacy HW1/HW2) — same as above on
  different bit positions
- `0x370` `EPAS3P_sysStatus` — sends a counter+1 echo with handsOnLevel = 1
  for the nag killer; the original frame from EPAS is not blocked
- `0x399` `ISA_speedLimit` — sets bit 5 of byte 1 to suppress the chime;
  recalculates the Tesla checksum
- `0x082` `UI_tripPlanning` — periodic write of `0x05` in byte 0 to
  trigger battery preconditioning, only when the user enables the
  Precondition setting

It does NOT write to:

- Brake controllers (`0x244` `IBST_status` and friends)
- Steering controllers (`0x129` `SteeringAngle*`)
- Powertrain (`0x118` `DI_vehicleStatus`, `0x132` BMS, `0x214` `DI_torque*`)
- ESP / stability control (`0x2A1` `ESP_status`)
- Door / window / lock actuators (`0x102`, `0x3E3`)
- Anything on Chassis CAN (we only sit on Vehicle / Party CAN bus)

The BMS, OTA detect, and follow-distance handlers are **read-only** parsers
— they update internal state, they never call `send_can_frame()`.

If you want to verify this for yourself, the dispatch is in
`scenes/fsd_running.c` and every `send_can_frame()` call site is gated by
`fsd_can_transmit(&state)` which honours Listen-Only mode and the OTA
Guard.

## Listen-Only mode

Since v2.4 the app boots in `Listen-Only` mode by default. The MCP2515
CAN controller is put into its hardware listen-only register, which is
**physically incapable of TX even on bus error frames**. You have to make
an explicit choice in Settings → Mode → Active before any frame leaves the
controller.

Use Listen-Only when you want to verify wiring, sniff traffic, or just be
sure you're not perturbing the bus.

## Recommended pre-flight

Before each session:

1. Pull the SIM card from the car (Model 3/Y: behind the glovebox)
2. Disable WiFi on the car (Settings → WiFi → Forget all networks)
3. Plug Flipper + Add-On into OBD-II
4. Boot the app, **stay in Listen-Only**
5. Watch the BMS dashboard / RX counter for 30 seconds — confirm sensible
   readings
6. Only then, switch to Active mode if you want TX

After each session:
- Switch back to Listen-Only or unplug the Flipper
- Re-enable WiFi if you need it for navigation / streaming
- Re-insert the SIM if you need cellular fallback

## Why all the caution

The original `Starmixcraft/tesla-fsd-can-mod` GitLab repo (the CanFeather
research we ported from) and its `Tesla-OPEN-CAN-MOD/tesla-open-can-mod`
successor namespace have both been taken down on GitLab, and a number of
related forks now carry the `deletion_scheduled` suffix. We don't know
exactly what triggered it — the working assumption is that visible legal
pressure on this kind of project is real and increasing. Conservative
defaults (Listen-Only first boot, OTA Guard, narrow TX surface, explicit
disclaimer) make this project survivable for longer and protect the
people who use it. The community has since reorganized on GitHub as
[ev-open-can-tools/ev-open-can-tools](https://github.com/ev-open-can-tools/ev-open-can-tools)
— a vehicle-agnostic CAN mod toolkit. The CanFeather mirror lives at
[Karolynaz/waymo-fsd-can-mod](https://github.com/Karolynaz/waymo-fsd-can-mod).

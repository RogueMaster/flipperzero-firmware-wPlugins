# Tesla Mod

Open-source Tesla CAN bus toolkit for Flipper Zero. It needs an MCP2515 CAN add-on (for example the Electronic Cats CAN Bus Add-On) wired into the car's CAN bus.

## What it does

- FSD region-gate bypass for HW3, HW4 and Legacy cars that already have an FSD package. A real FSD purchase or subscription is still required, and it does not unlock FSD on Tesla firmware 2026.14 and newer.
- Nag killer: DAS-aware hands-on echo with organic torque variation, only active while the car is actually asking for hands on the wheel.
- TLSSC Restore to bring back traffic light and stop sign control on VIN-banned cars.
- GTW Config Replay, speed profile sync, ISA speed chime suppression (HW4) and opt-in extras: Summon EU unlock, Continue on Green, right-hand drive override, adjustable Track Mode.
- Live BMS dashboard, DAS state readout and hardware auto-detect (HW3 / HW4 / Legacy) with a manual override.
- CAN Capture to the SD card (candump format) and Send Test profiles for your own research.

## Before you use it

- The first boot starts in Listen-Only mode. The CAN add-on physically cannot transmit until you switch to Active.
- Which bus carries which frame depends on the car and the harness. Check Service Mode → CAN Port in the car before wiring, and see HARDWARE.md in the repository.
- This changes CAN traffic on a moving vehicle. Use it at your own risk, keep your hands on the wheel and assume Tesla can detect it.

Wiring, supported cars, the ESP32 port and the full documentation: https://github.com/hypery11/flipper-tesla-fsd

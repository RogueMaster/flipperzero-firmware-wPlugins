## v1.0.0

First release.

### What's in the box
- FSD unlock for HW3 and HW4 (FSD V14)
- Auto-detection of hardware version via `GTW_carConfig` (`0x398`)
- Manual HW3/HW4 override
- Nag suppression (hands-on-wheel reminder cleared)
- Speed profile defaults to fastest, syncs from follow-distance stalk
- Live status on screen (HW version, FSD state, nag, frame counter)

### Requirements
- Flipper Zero (official or Unleashed/RogueMaster firmware)
- Electronic Cats CAN Bus Add-On (MCP2515)
- OBD-II connection to Tesla Party CAN bus

### Install
Drop `tesla_fsd.fap` into `SD Card/apps/GPIO/` on your Flipper.

### Compatibility
| Vehicle | Firmware | Mode |
|---------|----------|------|
| Model 3/Y HW3 | Any | HW3 |
| Model 3/Y HW4 | < 2026.2.3 | Use HW3 |
| Model 3/Y HW4 | >= 2026.2.3 | HW4 |
| Model S/X 2021+ | >= 2026.2.3 | HW4 |

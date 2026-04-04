# App Submission: Tesla FSD Unlock

## App Name
Tesla FSD

## Repository
https://github.com/hypery11/flipper-tesla-fsd

## Category
GPIO

## Description
Unlocks Tesla FSD via CAN bus frame modification. Supports HW3 and HW4 with auto-detection. Requires the Electronic Cats CAN Bus Add-On (MCP2515).

## Features
- Auto-detect HW3/HW4 from GTW_carConfig
- FSD enable bit injection on UI_autopilotControl
- Nag suppression
- Speed profile control (defaults to fastest)
- Live status display

## Hardware Required
- Flipper Zero
- Electronic Cats CAN Bus Add-On (or any MCP2515 GPIO module)

## License
GPL-3.0

## Build Status
Compiles against latest official firmware SDK.
